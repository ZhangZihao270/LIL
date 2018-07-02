#ifndef LOCK_MGR_H_
#define LOCK_MGR_H_

#include "ilock.h"
#include "lock_table.h"
#include "common/tpcc.h"
#include "common/clock.h"
#include "common/global.h"
using namespace tpcc;

struct TransCtx;

class LockMgr {
public:
  virtual void init(int64_t thread_num) = 0;

  virtual RC lock(TransCtx* trans_ctx, Header * header, Lock_type lock_t = L_WRITE, uint64_t receive_time = 0) = 0;

  virtual bool end(TransCtx* trans_ctx);
  
  virtual void unlock(TransCtx* trans_ctx, Header * header, Lock_type lock_t = L_WRITE) = 0;
};

//Implement the row locker
class RowLockMgr : public LockMgr {
public:
  virtual void init(int64_t thread_num);

  virtual RC lock(TransCtx* trans_ctx, Header *header, Lock_type lock_t = L_WRITE, uint64_t receive_time = 0);

  virtual void unlock(TransCtx* trans_ctx, Header *header, Lock_type lock_t = L_WRITE);

private:
  inline bool timeout(const int64_t end_time) {
    return local_time() > end_time;
  }

  //shared_lock
  bool rd_lock(TransCtx* trans_ctx, Header *header);
 
  //exclusive lock
  bool wr_lock(TransCtx* trans_ctx, Header *header);

  bool rd_locked(TransCtx* trans_ctx, Header *header);

private:
  //Note: OceanBase uses the 10 ms.
  const static int64_t SPIN_TIME_OUT = 20; //20 us
  const static uint64_t MASK_EXCLUSIVE  = 0x80000000;
  const static uint64_t MASK_OWNER = 0x7fffffff;
  const static uint64_t SQL_TIME_OUT = 25 * 1000;//25ms
};

//Implement the lock table
class CentralizedLockMgr : public LockMgr {
public:
  virtual void init(int64_t thread_num);

  virtual RC lock(TransCtx* trans_ctx, Header *header, Lock_type lock_t = L_WRITE, uint64_t receive_time = 0);

  virtual bool end(TransCtx *ctx);

  virtual void unlock(TransCtx* trans_ctx, Header *header, Lock_type lock_t = L_WRITE);

private:
  LockTable lock_table_;
};

//Implement the ilock method
class iLockMgr : public LockMgr {
public:

  virtual void init(int64_t thread_num);

  virtual RC lock(TransCtx* trans_ctx, Header *header, Lock_type lock_t = L_WRITE, uint64_t receive_time = 0);

  virtual void unlock(TransCtx* trans_ctx, Header *header, Lock_type lock_t = L_WRITE);

  void wakeup_candidate(const Lock_type type, uint64_t lock_uid);

  void timeout_trans();

private:
  ILock  ilock_;
};

#endif

