#include "trans_mgr.h"
#include "common/global.h"

#include "lock_mgr.h"
#include "db_worker.h"
#include <assert.h>

void TransCtx::init() {
  trans_id_ = UINT64_MAX;
  cb_.clear();
  lock_.clear();
  wr_record.clear();
  rd_lock_.clear();
  offset_ = 0;
  trans_state_ = 0;
  wait_start_time_ = 0;
  routinue_ = NULL;
}

void TransMgr::init(int64_t worker_num, LockMgr *lock_mgr, MVCCMgr *mvcc) {
  worker_num_ = worker_num;
  lock_mgr_ = lock_mgr;
  mvcc_=mvcc;
  trans_map_.init(TRANS_MAP_SIZE);
  //at most buffer 1024 trans ctx
  ctx_pool_.init(1024);
}

bool TransMgr::start_trans(uint64_t &trans_id, int64_t thread_index) {
  TransCtx *ctx = alloc();
  trans_id = inc(thread_index);
  ctx->trans_id_ = trans_id;
  STAT_INC(STAT_SERVER, ACTIVE_TRANS_COUNT, 1);
  STAT_INC(STAT_SERVER, TRANS_COUNT, 1);
  dbserver::DbWorker::attach_thread(trans_id);
  return trans_map_.put(trans_id, ctx);
}

bool TransMgr::end_trans(const uint64_t trans_id, bool to_commit) {
  TransCtx *ctx = NULL;

  if( !trans_map_.get(trans_id, ctx) ) {
    LOG_ERROR("unexpected trans id: %lld", trans_id);
    trans_map_.dump(trans_id);
    assert(0);
    return false;
  }
  assert(ctx->trans_id_ == trans_id);
  if( to_commit)
    commit(ctx);
  else 
    abort(ctx);
  trans_map_.erase(trans_id);
  release(ctx);
  STAT_INC(STAT_SERVER, ACTIVE_TRANS_COUNT, -1);
  if( to_commit ) 
    STAT_INC(STAT_SERVER, COMMIT_TRANS_COUNT, 1);
  else 
    STAT_INC(STAT_SERVER, ABORT_TRANS_COUNT, 1);
  dbserver::DbWorker::detach_thread();
  return true;
}

void TransMgr::commit(TransCtx *ctx) {
      mvcc_->end(ctx);
  lock_mgr_->end(ctx);
}

void Callback::undo() {
  if( undo_.cols_ == 0 ) return;
  int64_t pos = 0;

#define UNDO(name, schema) \
  case TABLE(name): \
    reinterpret_cast<name*>(header_)->value. \
      read(undo_.image_, undo_.len_, pos, undo_.cols_); \
  break;

  switch(header_->table_id_) {
    TABLE_LIST(UNDO)
  }
#undef UNDO
}

void TransMgr::abort(TransCtx *ctx) {  //调用callback进行回滚，进行undo操作

  std::vector<Callback> &cb = ctx->cb_;
  for(uint64_t i = 0; i < cb.size(); ++i)
    cb[i].undo();
 
  lock_mgr_->end(ctx);
}

bool TransMgr::get_trans(const uint64_t trans_id, TransCtx *&ctx) {
  ctx = get_trans(trans_id);
  return NULL == ctx;
}

TransCtx* TransMgr::get_trans(const uint64_t trans_id) {
  TransCtx *ret = NULL;
  if( !trans_map_.get(trans_id, ret) )
    ret = NULL;
  return ret;
}

TransCtx* TransMgr::alloc() {
  TransCtx *ret = NULL;
  if( ctx_pool_.pop(ret) )
    ret->init();
  else {
    void *ptr = ::malloc(sizeof(TransCtx));
    ret = new(ptr) TransCtx();
  }
  return ret;
}

void TransMgr::release(TransCtx *ctx) {
  if( !ctx_pool_.push(ctx) ) {
    //only free when the pool is full
    ctx->~TransCtx();
    ::free(ctx);
  }
}
