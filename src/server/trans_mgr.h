#ifndef TRANS_MGR_H_
#define TRANS_MGR_H_

#include <vector>
#include <utility>
#include <unordered_set>
#include <pthread.h>
#include <co_routine.h>

#include "common/atomic.h"
#include "common/tpcc.h"
#include "common/hash_table.h"
#include "common/define.h"
#include "common/global.h"

#include "common/fixed_queue.h"
//#include "mvcc.h"

using namespace std;
using namespace tpcc;

class LockMgr;
class MVCCMgr;

struct UndoEntry {
  UndoEntry () :
    cols_(0), len_(0), image_(NULL) {}

  UndoEntry (int64_t cols, int64_t len, char *image) :
    cols_(cols), len_(len), image_(image) {}

  int64_t cols_;    //?
  int64_t len_;
  char    *image_;  //?
};

struct Callback {
  Callback (Header *header) :
    header_(header) {}

  Callback (Header *header, int64_t cols, int64_t len, char *image) :
    header_(header), undo_(cols, len, image)
  {}

  void undo();

  Header*   header_;
  UndoEntry undo_;
};

//the context of a transaction
struct TransCtx {
  TransCtx() : trans_id_(UINT64_MAX), cb_(), lock_(),wr_record(), rd_lock_(), offset_(0), trans_state_(0) {
    pthread_cond_init(&cond_, NULL);
  }

  ~TransCtx() {
    pthread_cond_destroy(&cond_);
  }

  void init();

  inline bool valid() const {
    return UINT64_MAX != trans_id_;
  }

  inline void add_rd_lock(Header *h) {
    rd_lock_.emplace(h);
    lock_.emplace_back(h, L_READ);
  }
 /*
  inline bool has_wr_lock(std::pair<Header *,Lock_type> wr){
      return find(lock_.begin(),lock_.end(),wr)!=lock_.end();
  }*/

  inline void add_wr_lock(Header *h) {
    lock_.emplace_back(h, L_WRITE);
  }

  template<typename T>
  void add_wr_record(int64_t table_id,T *row){
      //wr_record.emplace(row);
      wr_record.emplace_back(table_id,row);
  }

  void add_wr_key(std::string key){
      wr_key.emplace_back(key);
  }
/*
  template<typename T>
  inline bool has_wr_record(T *row){
      return wr_record.find(row)!=wr_record.end();
  }*/

  inline bool has_rd_lock(Header *h) {  //判断该tran是否持有该锁，有返回1，没有返回0
    return rd_lock_.find(h) != rd_lock_.end();
  }

  template<typename T>
    bool save_undo_entry(const T &row, int64_t cols) {
      const int64_t pos = offset_;
      row.value.write(UNDO_BUFFER, MAX_BUFFER_SIZE, offset_, cols);
      cb_.emplace_back(&(const_cast<T &>(row).header), cols, offset_ - pos, UNDO_BUFFER + pos);
      //LOG_INFO("row count after save undo entry:%d",row.count);
      return offset_ < MAX_BUFFER_SIZE;
    }

  uint64_t trans_id_;
  std::vector<Callback> cb_;
  std::vector<std::pair<Header *, Lock_type> > lock_;  //记录该事务持有的锁
  //是否也需要template???
  //template<typename T>
  /*
#define WR_RECORD(name,schema){
  case TABLE(name){
      std::unordered_set<T *> er_record;
      break;
  }*/
  std::vector<std::string> wr_key;
  std::vector<std::pair<int64_t, void *>>wr_record;  //记录事务进行写操作的数据项
  //std::unordered_set<void *> wr_record;
  std::unordered_set<Header*> rd_lock_;  //??记录该事务持有的读锁，为什么要记录？？？
  int64_t offset_;

  int64_t trans_state_;  //表示什么？
  int64_t wait_start_time_;
  stCoRoutine_t *routinue_;

  pthread_cond_t cond_;  //？
private:
  char UNDO_BUFFER[MAX_BUFFER_SIZE];
};

class TransMgr {
public:
  TransMgr() : worker_num_(0),
    trans_map_(),
    lock_mgr_(NULL),
    mvcc_(NULL),
    allocated_trans_id_(0),
    ctx_pool_()
  {};

  void init(int64_t worker_num, LockMgr *lock_mgr,MVCCMgr *mvcc);

  bool start_trans(uint64_t &trans_id, int64_t thread_index);

  bool get_trans(const uint64_t trans_id, TransCtx *&cxt);

  TransCtx* get_trans(const uint64_t trans_id);

  bool end_trans(const uint64_t trans_id, bool to_commit = true);

  TransCtx *alloc();

  void release(TransCtx *ctx);

private:
  inline uint64_t inc(int64_t thread_index) {
    uint64_t next_trans_id, cur_trans_id;
    do {
      cur_trans_id  = allocated_trans_id_;
      next_trans_id = (cur_trans_id / worker_num_ + 1) * worker_num_ + thread_index;
    } while(cur_trans_id != __sync_val_compare_and_swap(&allocated_trans_id_, cur_trans_id, next_trans_id));
    assert(next_trans_id % worker_num_ == thread_index);
    return next_trans_id;
  }
  
  void commit(TransCtx *ctx);

  void abort(TransCtx *ctx);

private:
  //unordered_map<uint64_t, TransCtx*> trans_map_;
  int64_t worker_num_;
  HashTable<uint64_t, TransCtx*> trans_map_;
  LockMgr * lock_mgr_;
  MVCCMgr *mvcc_;
  volatile uint64_t allocated_trans_id_;

  common::FixedQueue<TransCtx> ctx_pool_;
  //const static int64_t TRANS_MAP_SIZE = 64 * 1024;
};

#endif
