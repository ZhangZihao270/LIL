#ifndef FIXED_QUEUE_
#define FIXED_QUEUE_

#include "atomic.h"
#include "debuglog.h"
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#define CACHE_ALIGNED __attribute__((aligned(64)))

namespace common {

  template <typename T>
    class FixedQueue {

      struct ArrayItem {
        volatile uint64_t pos_;
        T * volatile  val_;
      };

    public:
      FixedQueue();
      ~FixedQueue();

      void init(const int64_t max_num);
      void destory();

      bool push(T *ptr);
      bool pop(T *&ptr);

      inline int64_t total() const;
      inline int64_t free() const;

    private:
      inline int64_t total_(const uint64_t consumer, const uint64_t producer) const;
      inline int64_t free_(const uint64_t consumer, const uint64_t producer) const;

    private:
      int64_t max_num_;
      ArrayItem *data_;
      volatile uint64_t consumer_ CACHE_ALIGNED;
      volatile uint64_t producer_ CACHE_ALIGNED; 
    } CACHE_ALIGNED;

  template <typename T>
    FixedQueue<T>::FixedQueue() : 
      max_num_(0), 
      data_(NULL),
      consumer_(0),
      producer_(0) {}

  template <typename T>
    FixedQueue<T>::~FixedQueue() {
      destory();
    }

  template <typename T>
    void FixedQueue<T>::init(const int64_t max_num) {
      data_ = (ArrayItem *) ::malloc(sizeof(ArrayItem) * max_num);
      assert(data_ != NULL);
      memset(data_, 0, sizeof(ArrayItem) * max_num);
      data_[0].pos_ = UINT64_MAX;
      max_num_ = max_num;
      consumer_ = 0;
      producer_ = 0;
    }

  template <typename T>
    void FixedQueue<T>::destory() {
      if( data_ != NULL )
        ::free(data_);
      data_ = NULL;
      max_num_ = 0;
      consumer_ = 0;
      producer_ = 0;
    }

  template <typename T>
    inline int64_t FixedQueue<T>::total() const {
      return total_(consumer_, producer_);
    }

  template <typename T>
    inline int64_t FixedQueue<T>::free() const {
      return free_(consumer_, producer_);
    }

  template <typename T>
    inline int64_t FixedQueue<T>::total_(const uint64_t consumer, 
        const uint64_t producer) const {
      return (producer - consumer); 
    }

  template <typename T>
    inline int64_t FixedQueue<T>::free_(const uint64_t consumer,
        const uint64_t producer) const {
      return max_num_ - total_(consumer, producer);
    }

  template <typename T> 
    bool FixedQueue<T>::push(T *ptr) {
      if( NULL == ptr ) return false;
      uint64_t old_pos = 0, new_pos = 0;
      while( true ) {
        old_pos = producer_;
        new_pos = old_pos;

        if( 0 >= free_(consumer_, old_pos)) {
          LOG_ERROR("queue is full, producer_: %lld, consumer_: %lld", producer_, consumer_);
          return false;
        }

        new_pos++;
        if( old_pos == common::atomic_compare_exchange(&producer_, new_pos,
              old_pos) ) {
          break;
        }
      }
      uint64_t index = old_pos % max_num_;
      data_[index].val_ = ptr;
      data_[index].pos_ = old_pos;
      return true; 
    }

  template <typename T>
    bool FixedQueue<T>::pop(T *&ptr) {
      T *tmp_ptr = NULL;
      uint64_t old_pos = 0, new_pos = 0;
      while( true ) {
        old_pos = consumer_;
        new_pos = old_pos;

        if( 0 >= total_(old_pos, producer_) )
          return false;

        uint64_t index = old_pos % max_num_;
        if( old_pos != data_[index].pos_ )  //producer is filling the slot
          continue;
        tmp_ptr = data_[index].val_;

        ++new_pos;
        if( old_pos == common::atomic_compare_exchange(&consumer_, new_pos,
              old_pos) )
          break;
      }
      ptr = tmp_ptr;
      return true;
    }
};
#endif
