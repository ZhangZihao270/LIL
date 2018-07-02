#include "worker.h"
#include "clock.h"
#include "atomic.h"
#include "buffer.h"
#include "define.h"
#include <errno.h>
#include <pthread.h>
using namespace common;

Cond::Cond() : bcond_(false) {
  pthread_mutex_init(&mutex_, NULL);
  pthread_cond_init(&cond_, NULL);
}

Cond::~Cond() {
  pthread_mutex_destroy(&mutex_);
  pthread_cond_destroy(&cond_);
}

void Cond::signal() {
  if( false == common::atomic_compare_exchange((uint8_t *)&bcond_, true, false) ) {
    __sync_synchronize();
    pthread_mutex_lock(&mutex_);
    pthread_cond_signal(&cond_);
    pthread_mutex_unlock(&mutex_); 
  }
}

void Cond::timed_wait() {

  int64_t abs_time = common::local_time() + QUEUE_WATI_TIME;
  struct timespec ts;
  ts.tv_sec = abs_time / 1000000;
  ts.tv_nsec = (abs_time % 1000000) * 1000;

  //have a trial
  if( common::atomic_compare_exchange((uint8_t *)&bcond_, false, true) ) return;

  pthread_mutex_lock(&mutex_);
  while( false == common::atomic_compare_exchange((uint8_t *)&bcond_, false, true) ) {
    int tmp_ret = pthread_cond_timedwait(&cond_, &mutex_, &ts);
    if( ETIMEDOUT == tmp_ret ) {
      break;
    }
  }
  pthread_mutex_unlock(&mutex_);
}

bool Worker::push(void *ptr, int64_t worker_id, const TaskPriority priority) {

  int64_t task_id = 0;
  switch(priority) {
    case PRIORITY_NORMAL:
      {
        task_id = atomic_inc(&np_task_count_);
        worker_id = (worker_id == -1 ? task_id % worker_num_ : worker_id);
        while( !ctxs_[worker_id].np_task_queue_.push(ptr) ) 
          asm("pause\n");
        break;
      }
    case PRIORITY_HIGH:
      {
        task_id = atomic_inc(&hp_task_count_);
        worker_id = (worker_id == -1 ? task_id % worker_num_ : worker_id);
        while ( !ctxs_[worker_id].hp_task_queue_.push(ptr) )
          asm("pause\n");
        break;
      }
    case PRIORITY_LOW:
      {
        task_id = atomic_inc(&lp_task_count_);
        worker_id = (worker_id == -1 ? task_id % worker_num_ : worker_id);
        while ( !ctxs_[worker_id].lp_task_queue_.push(ptr) )
          asm("pause\n");
        break;
      }
  };
  LOG_DEBUG("add a task for thread %lld", worker_id);
  ctxs_[worker_id].cond_.signal();
  return true;
}

void Worker::launch() {
  int ret = 0;

  running_flag_ = true;
  __sync_synchronize();

  for(int64_t i = 0; i < worker_num_; ++i) {
    ctxs_.emplace_back(this, i);
  }

  for(int64_t i = 0; i < worker_num_; ++i) {
    ctxs_[i].hp_task_queue_.init(Worker::QUEUE_SIZE);
    ctxs_[i].np_task_queue_.init(Worker::QUEUE_SIZE);
    ctxs_[i].lp_task_queue_.init(Worker::QUEUE_SIZE);

    //it may be dangeous to use the pointer of ctxs_[i] here.
    //because the modification on the vector would change the pointer.
    if( 0 == (ret = pthread_create(&ctxs_[i].thd_, NULL, thread_func,
            &ctxs_[i])) ) {
      LOG_INFO("worker[%lld] is started", i);
    }
    else {
      LOG_ERROR("worker[%lld] is not started, code: %d", i, ret);
      stop();
    }
  }
}

void Worker::wait_for_stop() {
  int ret = 0;
  void *value_ptr = NULL;
  for(int64_t i = 0; i < worker_num_; ++i) {
    if( 0 == (ret = pthread_join(ctxs_[i].thd_, &value_ptr)) ) { //pthread_join等待一个线程结束
      LOG_INFO("worker[%lld] is ended", i);
    }
    else {
      LOG_ERROR("worker[%lld] is not ended, code %d", i, ret);
    }
  }
}

void* Worker::thread_func(void *ptr) {
  WorkerContext * const ctx = reinterpret_cast<WorkerContext*>(ptr);

  FixedQueue<void> &hp_queue = ctx->hp_task_queue_;
  FixedQueue<void> &np_queue = ctx->np_task_queue_;
  FixedQueue<void> &lp_queue = ctx->lp_task_queue_;

  Worker* wrk = ctx->worker_;
  Cond& cond = ctx->cond_;

  wrk->thread_index() = ctx->worker_id_;

  Buffer *buffer = new Buffer(MAX_BUFFER_SIZE);

  wrk->on_begin();

  int64_t cur_time;
  void *task;
  while ( !wrk->stop_() ||
      hp_queue.total() > 0 ||
      np_queue.total() > 0 ||
      lp_queue.total() > 0 ) {

    task = NULL;
    buffer->reset();

    if( hp_queue.pop(task) )  {
      wrk->handle(task, *buffer);
    }
    else if( np_queue.pop(task) ) {
      wrk->handle(task, *buffer);
    }
    else if( lp_queue.pop(task) ) {
      wrk->handle(task, *buffer);
    }
    else {
      cond.timed_wait();
    }
    wrk->timed_task();
  }
  wrk->on_end();

  return NULL;
}

void Worker::timed_task() {
  if( timed_task_interval_ != -1 ) {
    int64_t cur_time = local_time();
    if( next_wakeup_time_ < cur_time ) {
      handle_timed_task();
      next_wakeup_time_ = cur_time + timed_task_interval_;
    }
  }
}

void Worker::on_begin() {
}

void Worker::on_end() {
}

void Worker::stop() {
  running_flag_ = false;
  __sync_synchronize(); 
}
