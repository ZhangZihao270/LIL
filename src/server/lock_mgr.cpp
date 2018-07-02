#include "lock_mgr.h"
#include "lock_table.h"
#include "trans_mgr.h"
#include "common/debuglog.h"

#define ATOMIC_CAS(val, cmpv, newv) __sync_val_compare_and_swap((val), (cmpv), (newv))

bool LockMgr::end(TransCtx *ctx) {
  for(uint64_t i = 0; i < ctx->lock_.size(); ++i) {
    unlock(ctx, ctx->lock_[i].first, ctx->lock_[i].second);
  }
  return true;
}

void RowLockMgr::init(int64_t thread_num){}

RC RowLockMgr::lock(TransCtx* trans_ctx, Header *header, Lock_type lock_t, uint64_t receive_time) {
  if( header == NULL || trans_ctx == NULL ) return RC_ERROR;
  bool ret = false;
  switch(lock_t) {
    case L_WRITE:
      ret = wr_lock(trans_ctx, header);
      break;
    case L_READ:
      ret = rd_lock(trans_ctx, header);
      break;
    case L_NONE:
      ret = true;
      break;
    default:
      break;
  }
  if( !ret ) {
    LOG_DEBUG("%s lock fail, table: %s, txn: %x, state: %x", lock_t == L_WRITE ? "wr" : "rd", 
        table_name(header->table_id_).c_str(), trans_ctx->trans_id_, header->lock_);
    if(local_time() - receive_time > SQL_TIME_OUT ) return RC_ERROR;
    else return RC_TIMEOUT;
  }
  else {
    //LOG_INFO("txn: %x, state: %x, row: %s", trans_ctx->trans_id_,
    //    header->lock_, dump_row_info(header).c_str());
    return RC_OK;
  }
}

#define OWN(A, B) \
  (((A) & (MASK_OWNER)) == ((B) & (MASK_OWNER)))

#define EXCLUSIVE_MODE(A) \
  (0 != ((A) & MASK_EXCLUSIVE))

bool RowLockMgr::rd_lock(TransCtx* trans_ctx, Header *header) {

  bool ret = false;
  uint64_t lock_state = header->lock_, new_state;
  uint64_t trans_id   = trans_ctx->trans_id_;
  int64_t  end_time   = local_time() + SPIN_TIME_OUT;

  //is exclusive locked by the current transaction
  if( EXCLUSIVE_MODE(lock_state) && OWN(lock_state, trans_id) )
    return true;

  //is shared locked by the current transaction
  if( rd_locked(trans_ctx, header) )
    return true;

  while( true ) {
    lock_state = header->lock_;

    if( !EXCLUSIVE_MODE(lock_state) ) {
      new_state = lock_state + 1;
      assert(new_state < MASK_EXCLUSIVE);

      if( lock_state == ATOMIC_CAS(&header->lock_, lock_state, new_state) )  {
         trans_ctx->add_rd_lock(header);
         ret = true;
         break;
      }
    }

    if( timeout(end_time) ) {
      break;
    }
    asm("pause\n");
  }
  LOG_DEBUG("trans [%x, size: %lu], lock state [%x]", 
      trans_id, trans_ctx->cb_.size(), header->lock_);
  return ret;
}

bool RowLockMgr::wr_lock(TransCtx* trans_ctx, Header *header) {
  bool ret = false;
  uint64_t lock_state = header->lock_, new_state;
  uint64_t trans_id   = trans_ctx->trans_id_;
  int64_t  end_time   = local_time() + SPIN_TIME_OUT;

  if( EXCLUSIVE_MODE(lock_state) && OWN(lock_state, trans_id) )
    return true;

  uint64_t expectation = rd_locked(trans_ctx, header) ? 1 : 0;

  while( true ) {
    lock_state = header->lock_;

    if( expectation == lock_state ) {
      new_state = (MASK_EXCLUSIVE | (trans_id & MASK_OWNER));
      if( lock_state == ATOMIC_CAS(&header->lock_, lock_state, new_state) ) {
        trans_ctx->add_wr_lock(header);
        ret = true;
        break;
      }
    }

    if( timeout(end_time) ) {
      break;
    }
    asm("pause\n");
  }
  LOG_DEBUG("trans [%x, size: %lu], lock state [%x]", 
      trans_id, trans_ctx->cb_.size(), header->lock_);
  return ret;
}

bool RowLockMgr::rd_locked(TransCtx* trans_ctx, Header *header) {
  return trans_ctx->has_rd_lock(header);
}

void RowLockMgr::unlock(TransCtx* trans_ctx, Header *header, Lock_type lock_t) {
  if( NULL == trans_ctx || NULL == header ) return;
  uint64_t trans_id = trans_ctx->trans_id_;
  uint64_t lock_state = header->lock_, new_state;

  if( lock_t == L_READ ) {
    //unlock shared lock
    if ( !EXCLUSIVE_MODE(lock_state) && 0 != lock_state ) {
      while(true) {
        lock_state = header->lock_;
        new_state = lock_state - 1; //decrease the reference
        assert(new_state >= 0);
        if( lock_state == ATOMIC_CAS(&header->lock_, lock_state, new_state) )
          break;
        asm("pause\n");
      }
    }
  }
  else {
    //unlock exclusive lock
    if( EXCLUSIVE_MODE(lock_state) && OWN(lock_state, trans_id) ) {
      new_state = 0;
      assert(lock_state == ATOMIC_CAS(&header->lock_, lock_state, new_state));
    }
    else {
      LOG_ERROR("unexpected lock state: %llu, txn_id: %llu", lock_state, trans_id);
    }
  }
  //LOG_INFO("txn: %x, state: %x, row: %s", trans_ctx->trans_id_,
  //    header->lock_, dump_row_info(header).c_str());
}

void CentralizedLockMgr::init(int64_t thread_num)
{
  lock_table_.init(thread_num);
}

RC CentralizedLockMgr::lock(TransCtx *trans_ctx, Header *header, Lock_type lock_t, uint64_t receive_time){
  //uint64_t trans_id = trans_ctx->trans_id_;
  RC rc = RC_OK;
  rc = lock_table_.acquire(lock_t, trans_ctx, (uint64_t)header);
  return rc;
}

void CentralizedLockMgr::unlock(TransCtx *trans_ctx, Header *header, Lock_type lock_t){
  //uint64_t trans_id = trans_ctx->trans_id_;
  assert(RC_OK == lock_table_.release(trans_ctx, (uint64_t)header));
}

bool CentralizedLockMgr::end(TransCtx *ctx) {
  lock_table_.end_trans(ctx);
  return LockMgr::end(ctx);
}

void iLockMgr::init(int64_t thread_num) {
  ilock_.init(thread_num);
}

RC iLockMgr::lock(TransCtx* trans_ctx, Header *header, Lock_type lock_t, uint64_t receive_time) {
  return ilock_.acquire(lock_t, trans_ctx, header);
}

void iLockMgr::unlock(TransCtx* trans_ctx, Header *header, Lock_type lock_t) {
  assert(RC_OK == ilock_.release(lock_t, trans_ctx, header));
}

void iLockMgr::wakeup_candidate(const Lock_type type, uint64_t uid) {
  return ilock_.wakeup_candidate(type, reinterpret_cast<Header *>(uid));
}

void iLockMgr::timeout_trans() {
  ilock_.timeout_trans();
}

