#ifndef DB_WORKER_H
#define DB_WORKER_H

#include "packet.h"
#include "db_impl.h"
#include "trans_mgr.h"
#include "lock_mgr.h"
#include "common/worker.h"
#include "common/define.h"
#include "common/tpcc.h"
#include "common/debuglog.h"
#include <assert.h>
#include <queue>
#include "mvcc.h"
namespace dbserver {

  class DbWorker : public Worker {
    public: 
      DbWorker(int worker_num, int core_num, DbStore *store, TransMgr *trans_mgr, LockMgr *lock_mgr,MVCCMgr *mvcc) :
        Worker(worker_num, core_num), store_(store), trans_mgr_(trans_mgr), lock_mgr_(lock_mgr), mvcc_(mvcc){}

      inline bool handle_packet(void *pkt, int64_t trans_id);

      static void init(DbStore *store, TransMgr *trans_mgr, 
          LockMgr *lock_mgr, int64_t worker_thread_num, int64_t cpu_core_num,MVCCMgr *mvcc);

      static DbWorker* worker();

      inline static stCoRoutine_t* alloc_routine();

      inline static void free_routine(stCoRoutine_t *routine);
    private:

      virtual void handle(void *ptr, Buffer &buffer);
      virtual void on_begin();
      virtual void handle_timed_task();

      void process(void *ptr, Buffer &buffer);

      void handle_start(StartPacket *pkt);
      void handle_get(GetPacket *pkt, Buffer &buf);
      void handle_put(PutPacket *pkt, Buffer &buf);
      void handle_commit(CommitPacket *pkt);
      void handle_batch_put(BatchPutPacket *pkt, Buffer &buf);

      int put(const int64_t tid, TransCtx *ctx, const int64_t dml_type,
          const int64_t cols, const string &data, int64_t &pos, Buffer &buf);

      template <typename T>
        int put(TransCtx *ctx, const int64_t dml_type, 
            const int64_t cols, const string &data, int64_t &pos, Buffer &buf,int64_t table_id);

      template <typename T>
      inline void ensure_row(const typename T::Key &key, T *&row);

      bool ensure_transaction(const uint64_t trans_id, TransCtx *&trans_ctx);

    public:
      inline static void attach_thread(int64_t trans_id);
      inline static void detach_thread();
      inline int64_t assign_thread();

    private:

      struct CoExecutionContext {
        CoExecutionContext(DbWorker *host, void *pkt, Buffer *buf) :
          host_(host), pkt_(pkt), buf_(buf) 
        {}

        DbWorker* host_;
        void* pkt_;
        Buffer* buf_;
      };

      void co_handle(CoExecutionContext *ec);

      static void *co_trans_func(void *pdata);

    private:
      DbStore  *store_;
      TransMgr *trans_mgr_;
      LockMgr  *lock_mgr_;
      MVCCMgr *mvcc_;
      static __thread std::queue<stCoRoutine_t*> *routine_pool_;

      static bool init_;
      static DbWorker *worker_;
      static pthread_mutex_t global_lock_;
      static volatile int64_t *trans_thread_affinity;
  };

  inline bool DbWorker::handle_packet(void *pkt, int64_t trans_id) {
    //set affinity
 #ifdef USE_LOCK_TABLE
    if( trans_id != -1 ) {
      push(pkt, trans_id % get_worker_num());
    }
    else {
      //find another available thread
      int64_t dedicated_thread_id = assign_thread();
      if( dedicated_thread_id == -1 ) return false;
      push(pkt, dedicated_thread_id);
    }
#else
    push(pkt);
#endif
    return true;
  }

  inline int64_t DbWorker::assign_thread() {
    assert(NULL != trans_thread_affinity);
    int64_t dedicated_thread_id = -1;
    __sync_synchronize();
    for(int64_t i = 0; i < get_worker_num(); ++i) {
      if( -1 == trans_thread_affinity[i] ) {
        if( -1 == __sync_val_compare_and_swap(&trans_thread_affinity[i], -1, 0) ) {
          dedicated_thread_id = i;
          break;
        }
      }
    }
    return dedicated_thread_id;
  }

  inline void DbWorker::attach_thread(int64_t trans_id) {
#ifdef USE_LOCK_TABLE
    trans_thread_affinity[worker_->get_thread_index()] = trans_id;
    __sync_synchronize(); 
#endif
  }

  inline void DbWorker::detach_thread() {
#ifdef USE_LOCK_TABLE
    int64_t thread_index = worker_->get_thread_index();
    trans_thread_affinity[thread_index] = -1;
    __sync_synchronize();
#endif
  }

  template <typename T>
    void DbWorker::ensure_row(const typename T::Key &key, T *&row) {
      if( !store_->ensure_row<T>(key) ) {
        //a concurrent process has added the entry
      }
      row = NULL;
      assert(store_->get(key, row));
    }

  template <typename T>
    int DbWorker::put(TransCtx *ctx, const int64_t dml_type,
        const int64_t cols, const string &data, int64_t &pos, Buffer &buf,int64_t table_id) {
      int ret = 0;
      bool row_found = false, row_del = false;
      RC lock_secured = RC_OK;
      bool is_writeable=true;
      typename T::Key &r_key = *(new(buf.buff()) typename T::Key);
      T *r_row;
      r_key.read(data.c_str(), data.size(), pos);
      row_found = store_->get(r_key, r_row);
      if( !row_found )
        ensure_row(r_key, r_row);

      row_del = r_row->is_del();
      lock_secured = lock_mgr_->lock(ctx, reinterpret_cast<Header *>(r_row));
      is_writeable=mvcc_->isWriteable(ctx,r_row);
      assert(lock_secured != RC_TIMEOUT);
      if(is_writeable){
      if( RC_OK != lock_secured )
        return StatusCode::LOCK_CONFLICT;
      else if( row_del && DML_UPDATE == dml_type )
        return StatusCode::NOT_FOUND;
      else if( !row_del && DML_INSERT == dml_type )
        return StatusCode::ALREADY_EXISTS;
      else {
        bool undo_ret = false;
        typename T::Value &r_value = *(new(buf.buff()) typename T::Value);
        r_value.read(data.c_str(), data.size(), pos, cols);
        undo_ret = ctx->save_undo_entry(*r_row, cols);
        ctx->add_wr_record(table_id,r_row);
        //ctx->add_wr_key(data);
        if( undo_ret ) {
          //LOG_INFO("write done\n");
          r_row->value.apply(r_value, cols);
          r_row->value.modify_time=ctx->trans_id_;
          //LOG_INFO("row count after just write:%d",r_row->count);
          //LOG_INFO("write done\n");
          //LOG_INFO("row value time after just write:%d",r_row->value.modify_time);
          LOG_DEBUG("write %s", r_row->to_string(cols).c_str());
          //LOG_INFO("row count after just write:%d",r_row->count);
          /*
          if(r_row->head!=NULL){
            LOG_INFO("row has a head");
          }
          else if(r_row->tail!=NULL){
            LOG_INFO("row has a tail");
          }*/
        }
        else {
          LOG_ERROR("undo buffer is full");
          return StatusCode::ABORTED;
        }
      }
      }
      else{
          return StatusCode::ABORTED;
      }
      return StatusCode::OK; 
    }

  stCoRoutine_t *DbWorker::alloc_routine() {
    if( routine_pool_->empty() )
      return NULL;
    else {
      stCoRoutine_t *ret = routine_pool_->front();
      routine_pool_->pop();
      return ret; 
    } 
  }

  void DbWorker::free_routine(stCoRoutine_t *routine) {
    routine_pool_->push(routine);
  }
}

#endif
