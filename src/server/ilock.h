#ifndef ILOCK_H_
#define ILOCK_H_
#include <unordered_map>
#include <list>
#include <co_routine.h>
#include "packet.h"
#include "common/tpcc.h"
#include "common/global.h"
#include "common/worker.h"

using namespace tpcc;
struct TransCtx;

class GrantLockPacket : NullPacket {
public:
  GrantLockPacket(uint64_t uid, const Lock_type lock_type) : 
    NullPacket(NULL, NULL, NULL, NULL), uid_(uid), lock_type_(lock_type) {
    type_ = PKT_GRANT_LOCK;
  }

  uint64_t get_uid() const {
    return uid_;
  }

  const Lock_type get_lock_type() const {
   return lock_type_;
  }
 
protected:
  uint64_t uid_;
  Lock_type lock_type_;
};

class ThreadLockTable {
public:
  inline int add_write_waiter(uint64_t uid, TransCtx *ctx);
  inline int add_read_waiter(uint64_t uid, TransCtx *ctx);

  int get_write_waiter(uint64_t uid, stCoRoutine_t* &routinue);
  int get_read_waiter(uint64_t uid,  vector<stCoRoutine_t*> &routinue_list);

  void timeout_trans();

private:
  typedef std::unordered_map<uint64_t, std::list<TransCtx*> >  WaitList;   
  //即wait-map,记录线程上阻塞的Txn，uid是lock_header,应该即为一条record
  void timeout_trans(WaitList &wait_list, const int64_t bound);
private:
  WaitList wr_wait_list_;
  WaitList rd_wait_list_;
};

inline int ThreadLockTable::add_write_waiter(uint64_t uid, TransCtx *ctx) {
  wr_wait_list_[uid].emplace_back(ctx);
  return 0;
}

inline int ThreadLockTable::add_read_waiter(uint64_t uid, TransCtx *ctx) {
  rd_wait_list_[uid].emplace_back(ctx);
  return 0;
}

class ILock {
public:
  void init(const int64_t thread_num);

  RC acquire(const Lock_type type, TransCtx *ctx, Header *lock_header);

  RC release(const Lock_type type, TransCtx *ctx, Header *lock_header);

  void wakeup_candidate(const Lock_type type, Header *lock_header);

  void timeout_trans();

  static void init_thread_context(Worker *wrk);

private:

  RC wr_lock(TransCtx *ctx, Header *header);

  RC rd_lock(TransCtx *ctx, Header *header);

  void release_rd_lock(const uint64_t trans_id, Header *header);

  void release_wr_lock(const uint64_t trans_id, Header *header);

  void send_read_locks(Header *header, const uint64_t wait_bits);

  void send_write_lock(Header *header, const uint64_t wait_bits);

  bool yield_for_write_lock(TransCtx *ctx, Header *header, 
      const uint64_t expection, const uint64_t new_state);

  bool yield_for_read_lock(TransCtx *ctx, Header *header);

  bool set_wait_bit(volatile uint64_t &wait_bits);

  void clear_wait_bit(volatile uint64_t &wait_bits);

private:
  int64_t    thread_num_;

  static __thread ThreadLockTable *lock_table_;  //一个thread一个ilock实例？
  static __thread int64_t         thread_index_;
  static Worker                   *host_;

  const static uint64_t MASK_EXCLUSIVE = 0x80000000;     //100000000 相当于一个写锁，但是没有持有者
  const static uint64_t MASK_CANDIDATE = 0x80000000;     //dummy write lock
  const static uint64_t MASK_OWNER     = 0x7fffffff;     //011111111
public:
  const static uint64_t MAX_WAIT_TIME  = 25*1000;
};

#endif
