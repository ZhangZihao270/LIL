#ifndef WORKER_
#define WORKER_
#include "fixed_queue.h"
#include "buffer.h"
#include "clock.h"
#include <pthread.h>
#include <vector>
using namespace std;

namespace common {

  class Cond {
  public:
    Cond();
    ~Cond();
    void signal();
    
    void timed_wait();
  private:
    volatile bool bcond_;  //is the signal on
    pthread_cond_t cond_; //wake up condition
    pthread_mutex_t mutex_;
    const static int64_t QUEUE_WATI_TIME = 1000;
  };

  enum TaskPriority {
    PRIORITY_HIGH,
    PRIORITY_NORMAL,
    PRIORITY_LOW
  };

  
  class Worker;
  struct WorkerContext {
    WorkerContext(Worker *worker, int64_t id) :
      worker_(worker),
      worker_id_(id)
    {}

    FixedQueue<void> hp_task_queue_;
    FixedQueue<void> np_task_queue_;
    FixedQueue<void> lp_task_queue_;
    Worker *worker_;
    pthread_t thd_;
    int64_t worker_id_;
    Cond cond_;
  };

  class Worker {
  public:
    friend struct WorkerContext;

    Worker(int worker_num = 2, int core_num_ = -1) :
      worker_num_(worker_num),
      core_num_(core_num_),
      ctxs_(),
      hp_task_count_(0),
      np_task_count_(0),
      lp_task_count_(0),
      running_flag_(false),
      timed_task_interval_(-1),
      next_wakeup_time_(local_time())
    {}

    bool push(void *ptr, int64_t worker_id = -1, const TaskPriority priority = PRIORITY_NORMAL);

    void launch();

    void stop();

    void wait_for_stop();

    int64_t get_thread_index() const {
      return const_cast<Worker*>(this)->thread_index();
    }

    int64_t get_worker_num() const { 
      return worker_num_;
    }

    int64_t get_core_num() const {
      return core_num_; 
    }

    void set_timed_task_interval(int64_t interval) {
      timed_task_interval_ = interval; 
    }

  private:

    static void* thread_func(void *ptr);

    virtual void handle(void *ptr, Buffer &buffer) = 0;

    virtual void on_begin();

    virtual void on_end();

    virtual void handle_timed_task() = 0;
    
    void timed_task();

    inline bool stop_() const;

    int64_t & thread_index() {
      static __thread int64_t index = 0;
      return index;
    }

  private:
    int64_t worker_num_;
    int64_t core_num_;
    vector<WorkerContext> ctxs_;
    volatile uint64_t hp_task_count_;
    volatile uint64_t np_task_count_;
    volatile uint64_t lp_task_count_;
    volatile bool running_flag_;

    int64_t timed_task_interval_;
    int64_t next_wakeup_time_;
    const static int64_t QUEUE_SIZE = 1024;
  };

  inline bool Worker::stop_() const {
    return !running_flag_;
  }
}

#endif
