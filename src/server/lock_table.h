#ifndef LOCK_TABLE_H
#define LOCK_TABLE_H
#include <stdint.h>
#include <pthread.h>
#include <queue>
#include "common/fixed_queue.h"
#include "common/murmur_hash.h"
#include "common/global.h"
#include "common/tpcc.h"
#include "dep_graph.h"
#include "common/debuglog.h"
using namespace tpcc;

struct TransCtx;

inline bool conflict_lock(Lock_type l1, Lock_type l2);

struct LockEntry {
  Lock_type type_;
  TransCtx* ctx_;
  bool      active_;

  LockEntry * next_;   //？
  LockEntry * prev_;   //？

  void init(Lock_type type, TransCtx *ctx) {
    type_ = type;
    ctx_  = ctx;
    active_ = false;
    next_ = NULL;
    prev_ = NULL;
  }
};

struct RecordLock {

  RecordLock(const uint64_t uid) : uid_(uid) {
    owner_ = NULL;
    waiter_head_ = waiter_tail_ = NULL;
    owner_cnt_ = 0;
    waiter_cnt_ = 0;
  }

  int add_ownner(LockEntry *entry);

  int add_waiter(LockEntry *entry);

  int add_waiter_sorted(LockEntry *entry);

  int remove_waiter(LockEntry *entry);

  int remove_owner(TransCtx *ctx, LockEntry *&ret);

  const uint64_t uid_;  //？
  int64_t owner_cnt_;
  int64_t waiter_cnt_;

  LockEntry* owner_;
  LockEntry* waiter_head_;
  LockEntry* waiter_tail_;
};

class LockTable {
public:
  void init(int64_t thread_num);

  RC acquire(const Lock_type type, TransCtx *ctx, uint64_t lock_uid);

  RC release(TransCtx *ctx, uint64_t lock_uid);

  void end_trans(TransCtx *ctx);  //和abort的区别
private: 

  class LockGuard {
  public:
    LockGuard(pthread_mutex_t *latch) : latch_(latch) {
      pthread_mutex_lock(latch_);
    }

    ~LockGuard() {
      pthread_mutex_unlock(latch_);
    }

  private:
    pthread_mutex_t *latch_; 
  };

  struct BucketEntry {
    BucketEntry(uint64_t uid) : record_lock_(uid), next_(NULL)
    {}

    RecordLock record_lock_;
    BucketEntry *next_;
  };

  struct Bucket {
    BucketEntry*    head_;
    pthread_mutex_t latch_;
  };

  RC acquire_graph(Bucket &bkt, RecordLock *record_lock, 
    const Lock_type type, TransCtx *ctx, uint64_t lock_uid);

  RC acquire_wait_die(Bucket &bkt, RecordLock *record_lock, 
    const Lock_type type, TransCtx *ctx, uint64_t lock_uid);

  RC acquire_timeout(Bucket &bkt, RecordLock *record_lock, 
    const Lock_type type, TransCtx *ctx, uint64_t lock_uid);

  void ensure(Bucket &bucket, uint64_t uid, RecordLock *&ret);  //添加recordlock到bucket

  void remove(Bucket &bucket, uint64_t uid);

  bool detect_deadlock(RecordLock *record_lock, int64_t trans_id);

  bool granted_lock(LockEntry *entry, TransCtx *ctx, Lock_type type);  //？

  inline uint64_t hash_func(uint64_t lock_uid) const;

  bool detect(RecordLock *record_lock, int64_t trans_id, const Lock_type type);

  struct LockAllocator {
    static void* alloc_lock_entry();
    static void* alloc_bucket_entry();

    static void release(LockEntry *lock_entry);
    static void release(BucketEntry *bucket_entry);

    static common::FixedQueue<void> lock_entry_pool_;
    static common::FixedQueue<void> bucket_entry_pool_;
  };

private:
  Bucket   *buckets_;
  int64_t  bucket_num_;
  //MemAlloc   allocator_;
  DepGraph detector_;
  
  const int64_t WAIT_TIMEOUT = 25 * 1000;
};

inline uint64_t LockTable::hash_func(uint64_t lock_uid) const {  //判断锁落入哪个bucket
  return common::murmurhash64A(&lock_uid, sizeof(lock_uid), 0) % bucket_num_;
}

inline bool conflict_lock(Lock_type l1, Lock_type l2) {
  if (l1 == L_NONE || l2 == L_NONE) return false;
  if (l1 == L_WRITE || l2 == L_WRITE) return true;
  return false;
}

#endif // ROW_LOCK_H
