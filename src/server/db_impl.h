#ifndef DBIMPL_H
#define DBIMPL_H

#include <string.h>
#include <string>
#include <unistd.h>
#include <stdint.h>
//#include "masstree-beta/masstree_tcursor.hh"
//#include "masstree-beta/masstree.hh"
//#include "masstree-beta/query_masstree.hh"

#include "common/cmbtree/btree_base.h"
#include "common/debuglog.h"
#include "common/tpcc.h"

//using lcdf::String;
//using lcdf::Json;

using namespace std;
using namespace tpcc;
namespace dbserver {

//  struct quick_istr {
//    char buf_[32];
//    char *bbuf_;
//    quick_istr() {
//      set(0);
//    }
//    quick_istr(unsigned long x, int minlen = 0) {
//      set(x, minlen);
//    }
//    void set(unsigned long x, int minlen = 0){
//      bbuf_ = buf_ + sizeof(buf_) - 1;
//      do {
//        *--bbuf_ = (x % 10) + '0';
//        x /= 10;
//      } while (--minlen > 0 || x != 0);
//    }
//    lcdf::Str string() const {
//      return lcdf::Str(bbuf_, buf_ + sizeof(buf_) - 1);
//    }
//    const char *c_str() {
//      buf_[sizeof(buf_) - 1] = 0;
//      return bbuf_;
//    }
//    bool operator==(lcdf::Str s) const {
//      return s.len == (buf_ + sizeof(buf_) - 1) - bbuf_
//        && memcmp(s.s, bbuf_, s.len) == 0;
//    }
//    bool operator!=(lcdf::Str s) const {
//      return !(*this == s);
//    }
//  };

//  class DbImpl {
//    public:
//
//      bool put(const Str &key, const void *row);
//
//      bool get(const Str &key, void * &row);
//
//      void init(); 
//
//    private:
//      void prepare_thread(threadinfo *ti);
//      void* threadfunc(void* x);
//
//    private:
//      // add wangdonghiu [masstree] :b
//      Masstree::default_table *mst_;
//      threadinfo * main_ti_;
//      query<row_type> q_;
//      // all default to the number of cores
//  };

  class RawKey { 
    public:
      RawKey() : table_id_(TableID::TID_END), s_(NULL), len_(0) {}
      RawKey(const uint64_t table_id, const char *start, const int64_t len) 
        : table_id_(table_id), s_(start), len_(len) {}

      bool operator < (const RawKey & other) const {
        return compare(other) < 0;
      }
      
      bool operator <= (const RawKey &other) const {
        return compare(other) <= 0;
      }

      bool operator >= (const RawKey &other) const {
        return compare(other) >= 0;
      }

      bool operator == (const RawKey & other) const {
        return compare(other) == 0 ;
      }

      bool operator > (const RawKey & other) const {
        return compare(other) > 0;
      }

      int64_t operator - (const RawKey &other) const {
        return compare(other);
      }

      //TODO(tzhu): use allocator to create the object
      RawKey deep_copy() const {
        assert(len_ >= 0);
        char * dst = (char *)malloc(sizeof(char) * len_);
        ::memcpy(dst, s_, len_);
        return RawKey(table_id_, dst, len_);
      }

    private:
      int compare(const RawKey &other) const {
        if( table_id_ != other.table_id_ ) return table_id_ < other.table_id_ ? -1 : 1;
        //if( len_ != other.len_ ) return len_ < other.len_ ? -1 : 1;
        else return memcmp(s_, other.s_, len_);
      }

    private:
      uint64_t table_id_;
      const char *s_;
      int64_t len_;
  };

using namespace oceanbase::common::cmbtree;

  class DbStore {
    public:

      void init() {
       btree_.init(); 
      }

      template <typename P> 
        bool put(const P &row) {
          P *value = create<P>();
          *value = row;
          RawKey key(P::TID, (const char *)&row.key, sizeof(typename P::Key));
          key = key.deep_copy();
          int ret = 0;
          do {
            ret = btree_.put(key, (uint64_t)value);
          } while (ERROR_CODE_NEED_RETRY == ret);
          if( ret != ERROR_CODE_OK ) {
            LOG_ERROR("put with error: %d", ret);
          }
          return oceanbase::common::cmbtree::ERROR_CODE_OK == ret;
          //return oceanbase::common::cmbtree::ERROR_CODE_OK ==
          //  btree_.put(key.deep_copy(), (uint64_t)value);
        }

      template <typename P>
        bool ensure_row(const typename P::Key &key) {
          P *value = create<P>();
          value->key = key;
          value->set_del();
          RawKey raw_key(P::TID, (const char *)&key, sizeof(typename P::Key));
          return oceanbase::common::cmbtree::ERROR_CODE_OK ==
            btree_.put(raw_key.deep_copy(), (uint64_t)value);
        }

      template <typename P> 
        bool get(const typename P::Key &key, P *& ptr) {
          RawKey rk(P::TID, (const char*)&key, sizeof(key));
          uint64_t row;
          bool ret = (oceanbase::common::cmbtree::ERROR_CODE_OK == btree_.get(rk, row));
          if( ret )
            ptr = (P *)(row);
          return ret;
        }

    private: 
      template <typename P>
        P * create()  {
          return new P();
        }

      template <typename P>
        void release(P *ptr) {
          delete ptr;
        }

    private:
      typedef oceanbase::common::cmbtree::BtreeBase<RawKey, uint64_t> cmbtree;
      cmbtree btree_;
  }; 
};
#endif
