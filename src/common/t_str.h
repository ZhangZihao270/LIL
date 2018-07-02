#ifndef T_STR_H
#define T_STR_H

#include <assert.h>
#include <iostream>
#include <string>
#include <string.h>
#include "serialization.h"

#ifdef PACKED
#undef PACKED
#endif

#ifndef __linux__
#define PACKED __attribute__((packed))
#else
#define PACKED 
#endif

#define min(x, y) ((x) < (y) ? (x) : (y))

using namespace std;
using namespace common;

template <int N> 
class t_str {
  public:

    t_str() : len_(0) {}

    t_str(const char *str, int len) : len_(min(len, N)) {
      memcpy(data_, str, len_ * sizeof(char));
    }

    t_str(const string &str) : len_(min(str.size(), N)) {
      memcpy(data_, str.c_str(), len_ * sizeof(char));
    }

    const t_str & operator = (const string &str) {
      len_ = min(str.size(), N);
      memcpy(data_, str.c_str(), len_ * sizeof(char));
      return *this;
    } 

    const t_str & operator = (const char * str) {
      len_ = min(strlen(str), N);
      memcpy(data_, str, len_ * sizeof(char));
      return *this;
    }

    const char & operator [] (const int pos) const {
      assert(pos >= 0 && pos < len_);
      return data_[pos];
    }

    int size() const {
      return len_;
    }

    const char * c_str() {
      return data_;
    }

    bool encode(char *buf, const int64_t data_len, int64_t &pos) const {
      bool ret = true; 
      ret = ret && (encode_i32(buf, data_len, pos, len_) == SUCCESS);
      ret = ret && (encode_str_fix(buf, data_len, pos, data_, len_) == SUCCESS);
      return ret;
    }

    bool decode(const char *buf, const int64_t data_len, int64_t &pos) {
      bool ret = true;
      int len = 0;
      ret = ret && (decode_i32(buf, data_len, pos, &len) == SUCCESS);
      len_ = len;
      ret = ret && (decode_str_fix(buf, data_len, pos, data_, len) == SUCCESS);
      return ret;
    }

    friend ostream & operator << (ostream &os, const t_str &str) {
      for(int i = 0; i < str.len_; ++i) {
        os << str[i];
      }
      return os;
    }
    
    inline std::string to_string() const {
      return std::string(data_, len_);
    }

  private:
    int  len_;
    char data_[N];
}PACKED;


#undef min
#endif
