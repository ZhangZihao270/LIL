#ifndef BUFFER_H_
#define BUFFER_H_

#include <string.h>
#include <assert.h>

class Buffer {
public:
  Buffer() : buffer_(NULL), size_(0), pos_(0) {
  }

  Buffer(int64_t size) {
    buffer_ = (char *)::malloc(size * sizeof(char));
    size_ = size;
    pos_ = 0;
  }

  ~Buffer() {
    if( buffer_ != NULL )
      ::free(buffer_);
    buffer_ = NULL;
    size_ = 0;
    pos_ = 0;
  }

  char * buff() {
    assert(buffer_ != NULL);
    return buffer_;
  }

  const char * data() const {
    return buffer_;
  }

  const int64_t size() const { 
    return size_;
  }

  int64_t & pos() {
    return pos_;
  }

  void reset() {
    pos_ = 0;
  }

  bool end() const {
    return pos_ >= size_;
  }

  bool empty() const {
    return pos_ == 0;
  }

private:
  //disallow copy and assign
  Buffer(const Buffer &a);
  const Buffer & operator = (const Buffer &a);
  
private:
  char *buffer_;
  int64_t size_;
  int64_t pos_;
};


#endif
