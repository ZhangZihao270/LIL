#ifndef  BIT_LOCK_H_
#define  BIT_LOCK_H_
#include <stdint.h>
#include <string.h>

namespace common {
  static const uint8_t BIT_MASKS[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

  class BitLock {
  public:
    BitLock() : size_(0), bits_(NULL)
    {}
    ~BitLock() {
      destroy();
    }
  public:
    inline bool init(const int64_t size);
    inline void destroy();
    inline bool lock(const uint64_t index);
    inline bool unlock(const uint64_t index);
  private:

    inline int64_t upper_align(int64_t input, int64_t align = 8) {
      int64_t ret = input;
      ret = (input + align - 1) & ~(align - 1);
      return ret;
    }

    int64_t size_;
    uint8_t *volatile bits_;
  };

  class BitLockGuard {
  public:
    BitLockGuard(BitLock &lock, const uint64_t index) : lock_(lock), index_(index) {
      lock_.lock(index_);
    }
    ~BitLockGuard() {
      lock_.unlock(index_);
    }
  private:
    BitLock &lock_;
    const uint64_t index_;
  };

  bool BitLock::init(const int64_t size) {
    bool ret = true;
    if (0 < size_ || NULL != bits_) {
      ret = false;
    }
    else if (0 >= size) {
      ret = false;
    }
    else {
      int64_t byte_size = upper_align(size, 8) / 8;
      if (NULL == (bits_ = (uint8_t*)::malloc(byte_size))) {
        ret = false;
      }
      else {
        ::memset(bits_, 0, byte_size);
        size_ = size;
      }
    }
    return ret;
  }

  void BitLock::destroy() {
    if (NULL != bits_) {
      ::free(bits_);
      bits_ = NULL;
    }
    size_ = 0;
  }

#define ATOMIC_CAS(val, cmpv, newv) __sync_val_compare_and_swap((val), (cmpv), (newv))
  bool BitLock::lock(const uint64_t index) {
    bool ret = true;
    if (0 >= size_ || NULL == bits_) {
      ret = false; 
    }
    else if ((int64_t)index >= size_) {
      ret = false;
    }
    else {
      uint64_t byte_index = index / 8;
      uint64_t bit_index = index % 8;
      while (true) {
        uint8_t ov = bits_[byte_index];
        if (ov & BIT_MASKS[bit_index]) {
          continue;
        }
        if (ov == ATOMIC_CAS(&(bits_[byte_index]), ov, ov | BIT_MASKS[bit_index])) {
          break;
        }
      }
    }
    return ret;
  }

  bool BitLock::unlock(const uint64_t index) {
    bool ret = true;
    if (0 >= size_ || NULL == bits_) {
      ret = false;
    }
    else if ((int64_t)index >= size_) {
      ret = false;
    }
    else {
      uint64_t byte_index = index / 8;
      uint64_t bit_index = index % 8;
      while (true) {
        uint8_t ov = bits_[byte_index];
        if (!(ov & BIT_MASKS[bit_index])) {
          // have not locked
          break;
        }
        if (ov == ATOMIC_CAS(&(bits_[byte_index]), ov, ov & ~BIT_MASKS[bit_index])) {
          break;
        }
      }
    }
    return ret;
  }
#undef ATOMIC_CAS
}

#endif // BIT_LOCK_H_
