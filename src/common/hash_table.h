#ifndef  HASH_TABLE_H
#define  HASH_TABLE_H
#include <stdint.h>
#include <stdlib.h>
#include <algorithm>
#include "bit_lock.h"
#include "atomic.h"
#include "murmur_hash.h"
#include "debuglog.h"
namespace common {

  template <typename Key, typename Value>
    class HashTable {
      struct Node {
        Key key;
        Value value;
        union {
          Node *next;
          int64_t flag;
        };
      };
      static const int64_t EMPTY_FLAG = UINT64_MAX;
    public:
      HashTable();
      ~HashTable();
      bool init(const int64_t bucket_num);
      void destroy();
      bool clear();

      inline bool put(const Key &key, const Value &value, Value *exist = NULL);
      inline bool get(const Key &key, Value &value);
      inline bool erase(const Key &key, Value *value = NULL);
      inline int64_t bucket_using() const;
      inline int64_t size() const;

      inline void dump(const Key &key);

    private:
      inline bool equal_func_(const Key &a, const Key &b) const;
      inline uint64_t hash_func_(const Key &a) const;

    private:
      int64_t bucket_num_;
      Node *buckets_;
      BitLock bit_lock_;

      MurmurHash64A hash_helper_;


      volatile int64_t bucket_using_;
      volatile int64_t size_;
    };

  template <typename Key, typename Value>
    HashTable<Key, Value>::HashTable() : 
      bucket_num_(0), buckets_(NULL), 
      bucket_using_(0), size_(0) {
      }

  template <typename Key, typename Value>
    HashTable<Key, Value>::~HashTable() {
      destroy();
    }

  template <typename Key, typename Value>
    bool HashTable<Key, Value>::init(const int64_t bucket_num) {
      bool ret = true;
      if (NULL != buckets_ || 0 >= bucket_num ) {
        ret = false;
      }
      else if (NULL == (buckets_ = (Node*)::malloc(bucket_num * sizeof(Node)))) {
        ret = false;
      }
      else if (!(ret = bit_lock_.init(bucket_num))) {
        // init bit lock fail
      }
      else {
        bucket_num_ = bucket_num;
        memset(buckets_, -1, sizeof(Node) * bucket_num);
        bucket_using_ = 0;
        size_ = 0;
      }
      if ( !ret )
        destroy();
      return ret;
    }

  template <typename Key, typename Value>
    void HashTable<Key, Value>::destroy() {
      if (NULL != buckets_) {
          for (int64_t i = 0; i < bucket_num_; i++) {
            Node *iter = buckets_[i].next;
            while (EMPTY_FLAG != buckets_[i].flag && NULL != iter) {
              Node *tmp = iter;
              iter = iter->next;
              ::free(tmp);
            }
            buckets_[i].flag = EMPTY_FLAG;
          }
        ::free(buckets_);
        buckets_ = NULL;
      }
      bit_lock_.destroy();
      bucket_num_ = 0;
      bucket_using_ = 0;
      size_ = 0;
    }

  template <typename Key, typename Value>
    bool HashTable<Key, Value>::clear() {
      bool ret = true;
      if (NULL == buckets_) {
        ret = false;
      }
      else {
        for (int64_t i = 0; i < bucket_num_; i++) {
          BitLockGuard guard(bit_lock_, i);
          Node *iter = buckets_[i].next;
          while (EMPTY_FLAG != buckets_[i].flag && NULL != iter) {
            Node *tmp = iter;
            iter = iter->next;
            ::free(tmp);
          }
          buckets_[i].flag = EMPTY_FLAG;
        }
        bucket_using_ = 0;
        size_ = 0;
      }
      return ret;
    }

  template <typename Key, typename Value>
    bool HashTable<Key, Value>::put(const Key &key, const Value &value, Value *exist)
    {
      bool ret = true;
      if (NULL == buckets_) {
        ret = false;
      }
      else {
        uint64_t hash_value = hash_func_(key);
        uint64_t bucket_pos = hash_value % bucket_num_;
        BitLockGuard guard(bit_lock_, bucket_pos);
        if (EMPTY_FLAG == buckets_[bucket_pos].flag) {
          buckets_[bucket_pos].key = key;
          buckets_[bucket_pos].value = value;
          buckets_[bucket_pos].next = NULL;
          atomic_inc((uint64_t*)&bucket_using_);
          atomic_inc((uint64_t*)&size_);
        }
        else {
          Node *iter = &(buckets_[bucket_pos]);
          while (true) {
            if (equal_func_(iter->key, key) ) {
              ret = false;
              if( NULL != exist )
                *exist = iter->value;
              break;
            }
            if (NULL != iter->next) {
              iter = iter->next;
            }
            else {
              break;
            }
          }
          if (ret) {
            Node *node = (Node*) ::malloc(sizeof(Node));
            if(NULL == node) {
              ret = false;
            }
            else {
              node->key = key;
              node->value = value;
              node->next = NULL;
              iter->next = node;
              if( NULL != exist )
                *exist = value;
              atomic_inc((uint64_t*)&size_);
            }
          }
        }
      }
      return ret;
    }

  template <typename Key, typename Value>
    bool HashTable<Key, Value>::get(const Key &key, Value &value)
    {
      bool ret = false;
      if (NULL != buckets_) {
        uint64_t hash_value = hash_func_(key);
        uint64_t bucket_pos = hash_value % bucket_num_;
        BitLockGuard guard(bit_lock_, bucket_pos);
        if (EMPTY_FLAG != buckets_[bucket_pos].flag) {
          Node *iter = &(buckets_[bucket_pos]);
          while (NULL != iter) {
            if (equal_func_(iter->key, key)) {
              value = iter->value;
              ret = true;
              break;
            }
            iter = iter->next;
          }
        }
      }
      return ret;
    }

  template <typename Key, typename Value>
    void HashTable<Key, Value>::dump(const Key &key) {
      if( NULL != buckets_ ) {
        uint64_t hash_value = hash_func_(key);
        uint64_t bucket_pos = hash_value % bucket_num_;
        LOG_INFO("bucket pos: %llu", bucket_pos);
        BitLockGuard guard(bit_lock_, bucket_pos);
        if( EMPTY_FLAG != buckets_[bucket_pos].flag ) {
          Node *iter = &(buckets_[bucket_pos]);
          while ( NULL != iter ) {
            LOG_INFO("key: %lld", key);
            iter = iter->next;
          }
        }
      }
    }

  template <typename Key, typename Value>
    bool HashTable<Key, Value>::erase(const Key &key, Value *value /*= NULL*/)
    {
      bool ret = true;
      if (NULL == buckets_) {
        ret = false;
      }
      else {
        uint64_t hash_value = hash_func_(key);
        uint64_t bucket_pos = hash_value % bucket_num_;
        BitLockGuard guard(bit_lock_, bucket_pos);
        ret = false;
        if (EMPTY_FLAG != buckets_[bucket_pos].flag)
        {
          Node *iter = &(buckets_[bucket_pos]);
          Node *prev = NULL;
          while (NULL != iter) {
            if (equal_func_(iter->key, key)) {
              if (NULL != value) {
                *value = iter->value;
              }
              if (NULL == prev) {
                if( iter->next == NULL )
                  buckets_[bucket_pos].flag = EMPTY_FLAG;
                else {
                  iter = iter->next;
                  buckets_[bucket_pos] = *iter;
                  ::free(iter);
                }
              }
              else {
                prev->next = iter->next;
                ::free(iter);
              }
              ret = true;
              break;
            }
            prev = iter;
            iter = iter->next;
          }
        }
      }
      return ret;
    }

  template <typename Key, typename Value>
    int64_t HashTable<Key, Value>::bucket_using() const {
      return bucket_using_;
    }

  template <typename Key, typename Value>
    int64_t HashTable<Key, Value>::size() const {
      return size_;
    }

  template <typename Key, typename Value>
    bool HashTable<Key, Value>::equal_func_(const Key &a, const Key &b) const {
      return a == b;
    }
  
  template <typename Key, typename Value>
    uint64_t HashTable<Key, Value>::hash_func_(const Key &a) const {
      return hash_helper_(&a, sizeof(a));
    }
}

#endif // HASH_TABLE_H

