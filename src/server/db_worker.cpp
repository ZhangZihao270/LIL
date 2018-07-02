#include "db_worker.h"
#include <co_routine_inner.h>
#include <sched.h>
#include <unistd.h>
#include <iostream>
#ifdef USE_LOCK_TABLE
#define BIND_CPU
#endif

using namespace dbserver;

__thread std::queue<stCoRoutine_t*>* DbWorker::routine_pool_ = NULL;

bool DbWorker::init_ = false;
DbWorker* DbWorker::worker_ = NULL;
pthread_mutex_t DbWorker::global_lock_ = PTHREAD_MUTEX_INITIALIZER;
volatile int64_t *DbWorker::trans_thread_affinity = NULL;

void dump(const char *data, const int64_t len) {
  for(int i = 0; i < len; ++i)
    printf("%x", data[i]);
  printf("\n");
}

std::string dump(int64_t table_id, const string &key) {
  string ret;
  int64_t pos = 0;
#define DUMP_KEY(name, schema) \
  case TABLE(name): \
  { \
    name::Key k; \
    k.read(key.c_str(), key.size(), pos); \
    ret.append(#name); \
    ret.push_back(' '); \
    ret.append(k.to_string()); \
    break; \
  }
  switch(table_id) {
    TABLE_LIST(DUMP_KEY)
  }
#undef DUMP_KEY
  return ret;
}

void DbWorker::on_begin() {
#ifdef USE_ILOCK 
  ILock::init_thread_context(this);
  routine_pool_ = new std::queue<stCoRoutine_t*>();
#endif

//#ifdef BIND_CPU
//  int cpu_total_num = get_core_num() == -1 ? 
//    sysconf(_SC_NPROCESSORS_ONLN) : 
//    get_core_num();
//  int cpu_id = get_thread_index() % cpu_total_num;
//  cpu_set_t mask;
//  CPU_ZERO(&mask);
//  CPU_SET(cpu_id, &mask);
//  assert(0 == sched_setaffinity(0, sizeof(cpu_set_t), &mask));
//#endif
}

void DbWorker::handle_timed_task() {
#ifdef USE_ILOCK
  reinterpret_cast<iLockMgr*>(lock_mgr_)->timeout_trans();
#endif
}

void DbWorker::handle(void *ptr, Buffer &buffer) {
#ifdef USE_ILOCK
  NullPacket* pkt = reinterpret_cast<NullPacket*>(ptr);
  if( pkt->get_type() == PKT_START ) { //in thread processing
    handle_start(reinterpret_cast<StartPacket*>(pkt));
  }
  else if( PKT_GRANT_LOCK != pkt->get_type() ) { //in coroutine process
    CoExecutionContext ec(this, ptr, &buffer);
    co_handle(&ec); 
  }
  else { //wakeup blocked coroutine
    GrantLockPacket *lock_pkt = reinterpret_cast<GrantLockPacket*>(ptr);
    static_cast<iLockMgr*>(lock_mgr_)->wakeup_candidate(lock_pkt->get_lock_type(), lock_pkt->get_uid());
    delete lock_pkt; 
  }
#else
  process(ptr, buffer);
#endif
}

void DbWorker::co_handle(CoExecutionContext *ec) {
  stCoRoutine_t *routine = alloc_routine(); 

  if( NULL == routine ) {
    co_create(&routine, NULL, co_trans_func, ec);
  }
  else {
    routine->cStart = 0;
    routine->cEnd   = 0;
    routine->arg    = ec;
  }
  co_resume(routine);
 
  if( routine->cEnd )
    free_routine(routine);
}

void* DbWorker::co_trans_func(void *pdata) {
  CoExecutionContext ec = *reinterpret_cast<CoExecutionContext*>(pdata);
  ec.host_->process(ec.pkt_, *ec.buf_);
  return NULL;
}

void DbWorker::process(void *ptr, Buffer &buf) {
  NullPacket* pkt = reinterpret_cast<NullPacket*>(ptr);
  uint64_t htime = common::local_time();
  switch(pkt->get_type()) {
    case PKT_START:
      handle_start(reinterpret_cast<StartPacket*>(pkt));
      STAT_INC(STAT_SERVER, START_QTIME, htime - pkt->receive_time());
      STAT_INC(STAT_SERVER, START_TIME,  common::local_time() - htime);
      STAT_INC(STAT_SERVER, START_COUNT, 1);
      break;
    case PKT_PUT:
      handle_put(reinterpret_cast<PutPacket*>(pkt), buf);
      STAT_INC(STAT_SERVER, PUT_QTIME, htime - pkt->receive_time());
      STAT_INC(STAT_SERVER, PUT_TIME,  common::local_time() - htime);
      STAT_INC(STAT_SERVER, PUT_COUNT, 1);
      break;
    case PKT_GET:
      handle_get(reinterpret_cast<GetPacket*>(pkt), buf);
      STAT_INC(STAT_SERVER, GET_QTIME, htime - pkt->receive_time());
      STAT_INC(STAT_SERVER, GET_TIME,  common::local_time() - htime);
      STAT_INC(STAT_SERVER, GET_COUNT, 1);
      break;
    case PKT_COMMIT:
      handle_commit(reinterpret_cast<CommitPacket*>(pkt));
      STAT_INC(STAT_SERVER, END_QTIME, htime - pkt->receive_time());
      STAT_INC(STAT_SERVER, END_TIME, common::local_time() - htime);
      STAT_INC(STAT_SERVER, END_COUNT, 1);
      break;
    case PKT_BATCH_PUT:
      handle_batch_put(reinterpret_cast<BatchPutPacket*>(pkt), buf);
      STAT_INC(STAT_SERVER, END_QTIME, htime - pkt->receive_time());
      STAT_INC(STAT_SERVER, END_TIME, common::local_time() - htime);
      STAT_INC(STAT_SERVER, END_COUNT, 1);
      break;
    default:
      LOG_ERROR("unknown pkt type");
      break;
  }
}

void DbWorker::handle_get(GetPacket *pkt, Buffer &buf) {
  const uint64_t trans_id = pkt->get_request().transid();
  const int64_t table_id = pkt->get_request().tableid();
  const int64_t cols = pkt->get_request().cols();
  const int64_t lock_mode = pkt->get_request().lockmode();
  const string &key = pkt->get_request().key();

  TransCtx *ctx = NULL;

  void *p_row = NULL;
  int64_t pos = 0;
  bool row_found = false;
  RC is_found = RC_ABORT;

  if( !ensure_transaction(trans_id, ctx) ) {
    pkt->finish_with_error(StatusCode::ABORTED, "not in transaction");
    return;
  }
#define LOOK_UP_INDEX(T, schema) \
  case TID_ ## T : \
  { \
    T::Key &r_key = *(new(buf.buff()) T::Key); \
    T *r_row; \
    r_key.read(key.c_str(), key.size(), pos); \
    row_found = store_->get(r_key, r_row); \
    if( row_found && r_row->is_del() ) \
      row_found = false; \
    p_row = reinterpret_cast<void*>(r_row); \
    std::string str=dump(table_id,key);\
    if(row_found){\
    T::Value value; \
    is_found=mvcc_->findReadVersion<T>(ctx,reinterpret_cast<T *>(p_row),value);\
    value.write(buf.buff(), buf.size(), buf.pos(), cols);\
    }\
  } \
  break;
  switch(table_id) {
    TABLE_LIST(LOOK_UP_INDEX)
  }


#undef LOOK_UP_INDEX
    if(row_found&&is_found==RC_FIND){
        //LOG_INFO("read done\n");
        string value1(buf.buff(), buf.pos());
        LOG_DEBUG("row bytes size: %lu", value1.size());
        pkt->get_reply().set_value(value1);
        pkt->finish();
    }
    else if(is_found==RC_ABORT){
        trans_mgr_->end_trans(trans_id, false);
        if( !row_found )
            pkt->finish_with_error(StatusCode::NOT_FOUND, "row not found " + dump(table_id, key));
        else
            pkt->finish_with_error(StatusCode::ABORTED, "can not find right version" + dump(table_id, key));
    }
}

//the update semantics
void DbWorker::handle_put(PutPacket *pkt, Buffer &buf) {
  const uint64_t trans_id = pkt->get_request().transid();
  const int64_t table_id = pkt->get_request().tableid();
  const int64_t dml_type = pkt->get_request().dmltype();
  const int64_t cols = pkt->get_request().cols();

  const string &key = pkt->get_request().key();
  const string &value = pkt->get_request().value();

  TransCtx *ctx = NULL;

  pkt->get_reply().set_status(-1);
  if( !ensure_transaction(trans_id, ctx) ) {
    pkt->finish_with_error(StatusCode::ABORTED, "not in transaction");
    return;
  }
  //decode the input
  //dump(key.c_str(), key.size());
  //dump(value.c_str(), value.size());

  void *p_row = NULL;
  int64_t pos = 0;
  bool row_found = false, row_del = false;
  RC lock_secured = RC_OK;
  bool is_writeable=true;
#define ENSURE_ROW(T, schema) \
  case TID_ ## T : \
  { \
    T::Key &r_key = *(new(buf.buff()) T::Key); \
    T *r_row; \
    r_key.read(key.c_str(), key.size(), pos); \
    row_found = store_->get(r_key, r_row); \
    if( !row_found ) { \
      ensure_row(r_key, r_row); \
    } \
    row_del = r_row->is_del(); \
    p_row = reinterpret_cast<void*>(r_row); \
  } \
  break;

  //1. look up the index
  switch ( table_id ) {
    TABLE_LIST(ENSURE_ROW)
  }
#undef ENSURE_ROW

  //2. acquire the lock
  lock_secured = lock_mgr_->lock(ctx, reinterpret_cast<Header *>(p_row));
  //check isWriteable
  switch(table_id){
#define IS_WRITEABLE(name,schema)\
  case TABLE(name):\
  {\
      is_writeable=mvcc_->isWriteable<name>(ctx,reinterpret_cast<name *>(p_row));\
      break;\
  }
      TABLE_LIST(IS_WRITEABLE);
}
#undef IS_WRITEABLE

  //3. check intergrity
  if(is_writeable){
    //LOG_INFO("write table:%lld",table_id);
  if( RC_OK != lock_secured ) {
    trans_mgr_->end_trans(trans_id, false);
    pkt->finish_with_error(StatusCode::ABORTED, "lock not available " + dump(table_id, key));
  }
  else if( row_del && DML_UPDATE == dml_type ) {
    trans_mgr_->end_trans(trans_id, false);
    pkt->finish_with_error(StatusCode::NOT_FOUND, "row not found " + dump(table_id, key));
  }
  else if( !row_del && DML_INSERT == dml_type ) {
    trans_mgr_->end_trans(trans_id, false);
    pkt->finish_with_error(StatusCode::ALREADY_EXISTS, "row already exist " + dump(table_id, key));
  }
  else {
    //4. apply the write
    bool undo_ret = false;
    pos = 0;
    switch( table_id ) {
#define APPLY_ROW(name, schema) \
  case TABLE(name): \
  { \
    name::Value &r_value = *(new(buf.buff()) name::Value); \
    r_value.read(value.c_str(), value.size(), pos, cols); \
    undo_ret = ctx->save_undo_entry(*reinterpret_cast<name*>(p_row), cols); \
    ctx->add_wr_record(table_id,reinterpret_cast<name*>(p_row));\
    if( undo_ret ) {\
      reinterpret_cast<name*>(p_row)->value.apply(r_value, cols); \
      LOG_DEBUG("write: %s", reinterpret_cast<name*>(p_row)->value->to_string(cols).c_str()); \
    } \
    break; \
  }
      TABLE_LIST(APPLY_ROW);
    }
#undef APPLY_ROW
    if( undo_ret ) {
      pkt->get_reply().set_status(1);
      pkt->finish();
    }
    else {
      trans_mgr_->end_trans(trans_id, false);
      LOG_ERROR("txn %lld, undo buffer is full", trans_id);
      pkt->finish_with_error(StatusCode::ABORTED, "undo buffer is full");
    }
  }
}
else{
    trans_mgr_->end_trans(trans_id, false);
    pkt->finish_with_error(StatusCode::ABORTED, "has a new committed " + dump(table_id, key));
}
}

void DbWorker::handle_start(StartPacket *pkt) {
  const uint64_t trans_id = pkt->get_request().transid();
  if( trans_id != UINT64_MAX ) {
    LOG_ERROR("already in transaction, %lld", trans_id);
    //should we end the old transaction?
    pkt->get_reply().set_transid(trans_id);
    pkt->finish_with_error(StatusCode::ABORTED, "already in transaction");
    return;
  }
  uint64_t trans_id_new = UINT64_MAX;
  assert(trans_mgr_->start_trans(trans_id_new, get_thread_index()));
  LOG_DEBUG("start a new transaction [%llu]", trans_id_new);
  pkt->get_reply().set_transid(trans_id_new);
  pkt->finish();
}

void DbWorker::handle_commit(CommitPacket *pkt) {
  const uint64_t trans_id = pkt->get_request().transid();
  const int64_t  decision = pkt->get_request().decision();
  if( trans_id == UINT64_MAX ) {
    pkt->finish_with_error(StatusCode::ABORTED, "transaction id is not valid");
  }
  else if( decision != TXN_COMMIT ) {
    LOG_DEBUG("abort a txn[%llu]", trans_id);
    trans_mgr_->end_trans(trans_id, false);
    pkt->get_reply().set_status(-1);
    pkt->finish();
  }
  else {
    LOG_DEBUG("commit a txn [%llu]", trans_id);
    trans_mgr_->end_trans(trans_id);
    pkt->get_reply().set_status(0);
    pkt->finish();
  }
}

void DbWorker::handle_batch_put(BatchPutPacket *pkt, Buffer &buf) {
  const uint64_t trans_id = pkt->get_request().transid();
  const string   &data = pkt->get_request().data();
  int64_t pos = 0, dml_type, tid, cols;
  int error_code = 0;
  //LOG_INFO("batch_put");
  TransCtx *ctx = NULL;
  if( !ensure_transaction(trans_id, ctx) ) {
    pkt->finish_with_error(StatusCode::ABORTED, "not in transaction");
    return;
  }

  LOG_DEBUG("batch write size: %lld", data.size());

  while( pos < data.size() ) {
    if( 0 != (error_code = decode(dml_type, data.c_str(), data.size(), pos)) ) {
      LOG_DEBUG("decode dml_type fail, %d, size: %lld, pos: %lld", error_code, data.size(), pos);
      break;
    }
    if( 0 != (error_code = decode(tid, data.c_str(), data.size(), pos)) ) {
      LOG_DEBUG("decode tid fail, %d, size: %lld, pos: %lld", error_code, data.size(), pos);
      break;
    }
    if( 0 != (error_code = decode(cols, data.c_str(), data.size(), pos)) ) {
      LOG_DEBUG("decode cols fail, %d, size: %lld, pos: %lld", error_code, data.size(), pos);
      break;
    }
    if( 0 != (error_code = put(tid, ctx, dml_type, cols, data, pos, buf) ) ) {
      LOG_DEBUG("put row fail: %d, size: %lld, pos: %lld", error_code, data.size(), pos);
      break;
    }
    LOG_DEBUG("type: %lld, tid: %lld, pos: %lld", dml_type, tid, pos);
  }
  if( 0 == error_code ) {
    //commit the transaction
    LOG_DEBUG("commit a txn[%llu]", trans_id);
    trans_mgr_->end_trans(trans_id);
    pkt->get_reply().set_status(0);
    pkt->finish();
  }
  else {
    LOG_DEBUG("abort a txn[%llu], code: %d", trans_id, error_code);
    trans_mgr_->end_trans(trans_id, false);
    pkt->get_reply().set_status(error_code);
    pkt->finish_with_error(StatusCode::ABORTED, "batch write fail and transaction is aborted");
  }
}

int DbWorker::put(const int64_t tid, TransCtx *ctx, const int64_t dml_type,
    const int64_t cols, const string &data, int64_t &pos, Buffer &buf) {
  int ret = 0;
#define PUT_TABLE(name, schema) \
  case TID_ ## name : \
  {\
    ret = put<name>(ctx, dml_type, cols, data, pos, buf,tid);\
  }\
    break;

  switch(tid) {
    TABLE_LIST(PUT_TABLE)
    default:
      LOG_ERROR("invalid table id: %lld", tid);
      assert(0);
      break;
  }
#undef PUT_TABLE
  return ret;
}

//check whether the transaction identifier is valid
bool DbWorker::ensure_transaction(const uint64_t trans_id, TransCtx *&ctx) {
  if( UINT64_MAX == trans_id ) return false;
  ctx = trans_mgr_->get_trans(trans_id);
  return NULL != ctx;
}

void DbWorker::init(DbStore *store, TransMgr *trans_mgr, LockMgr *lock_mgr, 
    int64_t worker_thread_num, int64_t cpu_core_num,MVCCMgr *mvcc) {
  PacketFactory::init();
  lock_mgr->init(worker_thread_num);
  mvcc->init(worker_thread_num);
  if( !init_ ) {
    if( NULL == trans_thread_affinity ) {
      pthread_mutex_lock(&global_lock_);
      trans_thread_affinity = (int64_t *) malloc(sizeof(int64_t) * worker_thread_num);
      for(int64_t i = 0; i < worker_thread_num; ++i)
        trans_thread_affinity[i] = -1;
      pthread_mutex_unlock(&global_lock_);
    }
    trans_mgr->init(worker_thread_num, lock_mgr,mvcc);
    worker_ = new DbWorker(worker_thread_num, cpu_core_num, store, trans_mgr, lock_mgr,mvcc);
    worker_->launch();
  }
  else {
    LOG_ERROR("worker has been inited");
  }
}

DbWorker * DbWorker::worker() {
  return worker_;
}
