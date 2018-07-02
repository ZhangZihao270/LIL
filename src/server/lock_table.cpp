#include <vector>
#include "lock_table.h"
#include "trans_mgr.h"
#include "common/define.h"
#include "common/clock.h"

//#define USING_WAIT_DIE
#define USING_GRAPH

#ifdef USING_GRAPH
  #define acquire_impl acquire_graph
#elif defined USING_WAIT_DIE
  #define acquire_impl acquire_wait_die
#else
  #define acquire_impl acquire_timeout
#endif

common::FixedQueue<void> LockTable::LockAllocator::lock_entry_pool_;
common::FixedQueue<void> LockTable::LockAllocator::bucket_entry_pool_;

inline bool is_older(const TransCtx *old, const TransCtx *young) {
  return old->trans_id_ < young->trans_id_;
}

int RecordLock::add_ownner(LockEntry *entry) {
  entry->next_ = owner_;
  entry->active_ = true;
  owner_ = entry;
  if( entry->type_ == L_WRITE ) {
    entry->ctx_->add_wr_lock((Header*)uid_);
  }
  else {
    entry->ctx_->add_rd_lock((Header*)uid_);
  }
  return ++owner_cnt_;
}

int RecordLock::add_waiter(LockEntry *entry) {
  if( NULL != waiter_tail_ ) {
    entry->prev_ = waiter_tail_;
    waiter_tail_->next_ = entry;
    waiter_tail_ = entry;
  }
  else {
    waiter_head_ = entry;
    waiter_tail_ = entry;
    entry->prev_ = NULL;
    entry->next_ = NULL;
  }
  return ++waiter_cnt_;
}

int RecordLock::add_waiter_sorted(LockEntry *entry) {
  if( NULL != waiter_head_ ) {
    LockEntry *prev, *cur;
    for(cur = waiter_head_; cur != NULL; cur = cur->next_) {
      if( is_older(cur->ctx_, entry->ctx_) )
        break;
    }
    prev = NULL != cur ? cur->prev_ : waiter_tail_; 

    entry->prev_ = prev;
    if( NULL != prev)
      prev->next_ = entry;
    else
      waiter_head_ = entry;

    entry->next_ = cur;
    if( NULL != cur )
      cur->prev_ = entry;
    else
      waiter_tail_ = entry;
  }
  else {
    waiter_head_ = entry;
    waiter_tail_ = entry;
    entry->prev_ = NULL;
    entry->next_ = NULL;
  }
  return ++waiter_cnt_;
}

int RecordLock::remove_waiter(LockEntry *entry) {
  if( entry->prev_ != NULL )
    entry->prev_->next_ = entry->next_;
  else
    waiter_head_ = entry->next_;

  if( entry->next_ != NULL )
    entry->next_->prev_ = entry->prev_;
  else
    waiter_tail_ = entry->prev_;

  return --waiter_cnt_;
}

int RecordLock::remove_owner(TransCtx *ctx, LockEntry *&ret) {
  LockEntry *cur = owner_, *prev = NULL;
  LOG_DEBUG("remove txn [%lld] lock", ctx->trans_id_);
  for(; NULL != cur && cur->ctx_->trans_id_ != ctx->trans_id_; cur = cur->next_) {
    LOG_DEBUG("find txn [%lld]", cur->ctx_->trans_id_);
    prev = cur;
  }
  assert(NULL != cur);
  ret = cur;
  if( prev == NULL )
    owner_ = cur->next_;
  else
    prev->next_ = cur->next_;
  return --owner_cnt_;
}

void LockTable::init(int64_t thread_num) {
  bucket_num_ = LOCK_TABLE_SIZE;
  buckets_ = (Bucket *) ::malloc(LOCK_TABLE_SIZE*sizeof(Bucket));
  for(int i = 0; i < LOCK_TABLE_SIZE; ++i) {
    buckets_[i].head_ = NULL;
    pthread_mutex_init(&buckets_[i].latch_, NULL);
  }
  LockAllocator::lock_entry_pool_.init(8192);
  LockAllocator::bucket_entry_pool_.init(8192);
  detector_.init(thread_num);
}

void LockTable::ensure(Bucket &bkt, uint64_t uid, RecordLock *&ret) {  //添加recordlock到bucket
  BucketEntry *cur;
  for(cur = bkt.head_; cur != NULL; cur = cur ->next_)
    if( cur->record_lock_.uid_ == uid ) break;
  if( NULL == cur ) {
    LOG_DEBUG("create new record lock, uid: %lld", uid);
    cur = new (LockAllocator::alloc_bucket_entry()) BucketEntry(uid);
    //cur = new (allocator_.tcmalloc(sizeof(BucketEntry))) BucketEntry(uid);
    cur->next_ = bkt.head_;
    bkt.head_ = cur;
  }
  ret = &cur->record_lock_;
}

void LockTable::remove(Bucket &bkt, uint64_t uid) {
  BucketEntry *cur = NULL, *prev = NULL;
  for(cur = bkt.head_; cur != NULL; cur = cur ->next_) {
    if( cur->record_lock_.uid_ == uid ) break;
    prev = cur; 
  }
  assert(cur != NULL);
  if( prev == NULL )
    bkt.head_ = cur->next_;
  else
    prev->next_ = cur->next_;

  cur->~BucketEntry();
  LockAllocator::release(cur);
  //allocator_.tcfree(cur);
}

bool LockTable::granted_lock(LockEntry *entry, TransCtx *ctx, Lock_type type) {
  if( entry->ctx_->trans_id_ == ctx->trans_id_ ) {
    if( entry->type_ == L_WRITE || (entry->type_ == type) )
      return true;
    else {
      LOG_ERROR("update the lock mode is not supported, lock_mode: %d, request_mode: %d", entry->type_, type);
      assert(0);
    }
  }
  return false;
}

RC LockTable::acquire(const Lock_type type, TransCtx *ctx, uint64_t lock_uid) {   //事务获取锁
  Bucket &bkt = buckets_[hash_func(lock_uid)];
  RecordLock *record_lock = NULL;
  LockGuard guard(&bkt.latch_);
  LOG_DEBUG("trans[%lld] tries to acquire lock[%lld]", ctx->trans_id_, lock_uid);
  ensure(bkt, lock_uid, record_lock);

  return acquire_impl(bkt, record_lock, type, ctx, lock_uid);
}

RC LockTable::acquire_timeout(Bucket &bkt, RecordLock *record_lock,
    const Lock_type type, TransCtx *ctx, uint64_t lock_uid) {
  Lock_type cur_type = record_lock->owner_ == NULL ? L_NONE : record_lock->owner_->type_;
  bool conflict = conflict_lock(cur_type, type);

  for(LockEntry *entry = record_lock->owner_;
      NULL != entry; entry = entry->next_) {
    if( granted_lock(entry, ctx, type))
      return RC_OK;
  }
 
  LockEntry *entry = (LockEntry*) LockAllocator::alloc_lock_entry();
  //LockEntry *entry = (LockEntry*) allocator_.tcmalloc(sizeof(LockEntry));
  entry->init(type, ctx);

  if( !conflict && record_lock->waiter_cnt_ == 0 ) {
    record_lock->add_ownner(entry);
    LOG_DEBUG("trans[%lld] get lock[%lld]", ctx->trans_id_, lock_uid);
  }
  else {

    if( WAIT_TIMEOUT == 0 ) {
      LockAllocator::release(entry);
      return RC_ABORT;
    }

    record_lock->add_waiter(entry);

    //LOG_INFO("trans[%lld] is blocked", ctx->trans_id_);

    int64_t abs_time = common::local_time() + WAIT_TIMEOUT;
    struct timespec ts;
    ts.tv_sec = abs_time / 1000000;
    ts.tv_nsec = (abs_time % 1000000) * 1000;

    if( ETIMEDOUT == pthread_cond_timedwait(&ctx->cond_, &bkt.latch_, &ts) ) {
      if( !entry->active_ ) {
        LOG_DEBUG("trans [%lld] waiting is timeout, conflict with %lld",
            ctx->trans_id_,
            record_lock->owner_->ctx_->trans_id_);
        record_lock->remove_waiter(entry);
        LockAllocator::release(entry);
        return RC_ABORT;
      }
      else {
        LOG_INFO("unexpected, timeout but still acquired");
        return RC_OK;
      }
    }
    //LOG_INFO("trans[%lld] wake up", ctx->trans_id_);
  }
  return RC_OK;
}

RC LockTable::acquire_wait_die(Bucket &bkt, RecordLock *record_lock, 
    const Lock_type type, TransCtx *ctx, uint64_t lock_uid) {
  Lock_type cur_type = record_lock->owner_ == NULL ? L_NONE : record_lock->owner_->type_;
  bool granted = false, die = false;
  bool conflict = conflict_lock(cur_type, type);

  for(LockEntry *entry = record_lock->owner_; 
      NULL != entry; entry = entry->next_) {
    if( granted_lock(entry, ctx, type)) {
      granted = true;
      break;
    }
    if ( is_older(entry->ctx_, ctx) )
      //entry->ctx is older than ctx
      die = true;
  }

  if( granted ) return RC_OK;
  if( (conflict || 0 != record_lock->waiter_cnt_) && die ) return RC_ABORT;

  LockEntry *entry = (LockEntry*) LockAllocator::alloc_lock_entry();
  //LockEntry *entry = (LockEntry*) allocator_.tcmalloc(sizeof(LockEntry));
  entry->init(type, ctx);
 
  if( !conflict ) {
    record_lock->add_ownner(entry);
    LOG_DEBUG("trans[%lld] get lock[%lld]", ctx->trans_id_, lock_uid);
  }
  else {

    record_lock->add_waiter_sorted(entry);

    //LOG_INFO("trans[%lld] is blocked", ctx->trans_id_);
    pthread_cond_wait(&ctx->cond_, &bkt.latch_);
    //LOG_INFO("trans[%lld] wake up", ctx->trans_id_);
  }
  return RC_OK;
}

RC LockTable::acquire_graph(Bucket &bkt, RecordLock *record_lock,
    const Lock_type type, TransCtx *ctx, uint64_t lock_uid) {
  Lock_type cur_type = record_lock->owner_ == NULL ? L_NONE : record_lock->owner_->type_;
  bool granted = false;
  bool conflict = conflict_lock(cur_type, type);

  for(LockEntry *entry = record_lock->owner_;
      NULL != entry; entry = entry->next_) {
    if( granted_lock(entry, ctx, type) ) {
      granted = true;
      break;
    } 
  }

  if( granted ) return RC_OK;

  LockEntry *entry = (LockEntry*) LockAllocator::alloc_lock_entry();
  //LockEntry *entry = (LockEntry*) allocator_.tcmalloc(sizeof(LockEntry));
  entry->init(type, ctx);

  if( !conflict && 0 == record_lock->waiter_cnt_ ) {
    record_lock->add_ownner(entry); 
    LOG_DEBUG("trans[%lld] get lock[%lld]", ctx->trans_id_, lock_uid);  
  }
  else {
    //check deadlock cycle
    if( detect(record_lock, ctx->trans_id_, type)) {
      LOG_WARN("find deadlock, transid: %lld", ctx->trans_id_);
      LockAllocator::release(entry);
      return RC_ABORT;
    }
    //LOG_INFO("Waiting...%ld", ctx->trans_id_);
    record_lock->add_waiter(entry);
    pthread_cond_wait(&ctx->cond_, &bkt.latch_);
  }
  return RC_OK;
}


RC LockTable::release(TransCtx *ctx, uint64_t lock_uid) {
  Bucket &bkt = buckets_[hash_func(lock_uid)];
  RecordLock *record_lock = NULL;

  LOG_DEBUG("trans [%lld] release lock[%lld]", ctx->trans_id_, lock_uid);
  LockGuard guard(&bkt.latch_);

  ensure(bkt, lock_uid, record_lock);

  LockEntry* entry;
  int64_t owner_cnt = record_lock->remove_owner(ctx, entry); 

  LockAllocator::release(entry);
  //allocator_.tcfree(entry);

  if( 0 == owner_cnt ) {
    LockEntry *waiter = record_lock->waiter_head_;
    if( NULL != waiter && L_WRITE == waiter->type_ ) {
      record_lock->remove_waiter(waiter);
      record_lock->add_ownner(waiter);
      pthread_cond_signal(&waiter->ctx_->cond_);
    }
    else if( NULL != waiter ) {
      LockEntry *nxt;
      for(waiter = record_lock->waiter_head_;
          NULL != waiter && waiter->type_ != L_WRITE;
          waiter = nxt) {
        nxt = waiter->next_;
        record_lock->remove_waiter(waiter);
        record_lock->add_ownner(waiter);
        pthread_cond_signal(&waiter->ctx_->cond_);
      }
    }
    else { //0 waiter, 0 owner
      LOG_DEBUG("deallocation record lock[%lld]", lock_uid);
      remove(bkt, lock_uid);
    }
  }
  return RC_OK;
}

void LockTable::end_trans(TransCtx *ctx) {
#ifdef USING_GRAPH
  detector_.delete_dep(ctx->trans_id_);    //需研究
#endif
}

bool LockTable::detect(RecordLock *record_lock, int64_t trans_id, const Lock_type type) {
  std::vector<int64_t> txnids;
   
  if(NULL != record_lock->waiter_tail_) {
    if(type == L_WRITE){//add all reads after the last write
      for(LockEntry *entry = record_lock->waiter_tail_;
          NULL != entry; entry = entry->prev_){
        if(entry->type_ == L_READ ) {
          txnids.emplace_back(entry->ctx_->trans_id_);
        }
        else{
          if( txnids.size() == 0 ) //the last waited writer
            txnids.emplace_back(entry->ctx_->trans_id_);
          break;
        }
      }
    }
    else if(type == L_READ){//add the last write
      for(LockEntry *entry = record_lock->waiter_tail_;
          NULL != entry; entry = entry->prev_){
        if(entry->type_ == L_WRITE){
          txnids.emplace_back(entry->ctx_->trans_id_);
          break;
        }
      }
      if( 0 == txnids.size() ){//no write in the wait list
        for(LockEntry *entry = record_lock->owner_;
            NULL != entry; entry = entry->next_){
          txnids.emplace_back(entry->ctx_->trans_id_);
        }
      }
    }
  }
  else{//no waiters and conflic with owner
    for(LockEntry *entry = record_lock->owner_;
        NULL != entry; entry = entry->next_){
      txnids.emplace_back(entry->ctx_->trans_id_);
    }
  }
  assert(txnids.size() > 0);
  detector_.add_dep(trans_id, txnids);//delete when unlock

  if(detector_.detect_cycle(trans_id)) return true;
  return false;
}

void* LockTable::LockAllocator::alloc_lock_entry() {
  void *ret = NULL;
  if( !lock_entry_pool_.pop(ret) )
    ret = ::malloc(sizeof(LockEntry));
  return ret;
}

void* LockTable::LockAllocator::alloc_bucket_entry() {
  void *ret = NULL;
  if( !bucket_entry_pool_.pop(ret) )
    ret = ::malloc(sizeof(BucketEntry));
  return ret;
}

void LockTable::LockAllocator::release(LockEntry *lock_entry) {
  if( !lock_entry_pool_.push(lock_entry) )
    ::free(lock_entry);
}

void LockTable::LockAllocator::release(BucketEntry *bucket_entry) {
  if( !bucket_entry_pool_.push(bucket_entry) )
    ::free(bucket_entry);
}
