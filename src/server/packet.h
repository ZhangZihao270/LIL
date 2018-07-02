#ifndef PACKET_H
#define PACKET_H

#include <iostream>
#include <memory>
#include <string>

#include "common/define.h"
#include "common/clock.h"
#include "common/db.pb.h"
#include "common/debuglog.h"
#include "brpc/server.h"
#include "common/fixed_queue.h"
//#include <grpc++/grpc++.h>
//#include "common/db.grpc.pb.h"
using namespace dbinter;
using namespace std;
using common::StatusCode;
//using namespace grpc;

using brpc::Controller;
using google::protobuf::Closure;

/*
* more info can be found in
* https://grpc.io/grpc/cpp
* */

/*
struct Status{
  Status(StatusCode code,
    const std::string &msg) 
    : code_(code), msg_(msg)
    {}
  StatusCode code_;
  std::string msg_;
};
*/


//enum PacketStatus {
//  CREATE,  
//  PROCESS, 
//  FINISH
//};


#define unlikely(A) __builtin_expect((A), 0)

enum PacketType {
  PKT_NULL,
  PKT_START,
  PKT_PUT,
  PKT_GET,
  PKT_COMMIT,
  PKT_BATCH_PUT,
  PKT_GRANT_LOCK
};

class PacketFactory {
public:

  static void init();

  template <typename T>
    static inline T* alloc(Controller* cntl, Closure* done, 
        const typename T::Request* request, typename T::Reply* reply);

  template <typename T>
    static inline void free(T *pkt);
private:
  static __thread common::FixedQueue<void>* pkt_pool_;
  static int64_t pkt_size_;
};

template <typename T>
T* PacketFactory::alloc(Controller* cntl, Closure* done, 
    const typename T::Request* request, typename T::Reply* reply) {
  void *ptr = NULL;
  assert(pkt_size_ == sizeof(T));
  if( unlikely(NULL == pkt_pool_) ) {
    pkt_pool_ = new common::FixedQueue<void>();
    pkt_pool_->init(1024);
  }
  if( !pkt_pool_->pop(ptr) )
    ptr = ::malloc(sizeof(T));
  return new(ptr) T (cntl, done, request, reply, pkt_pool_);
}

template <typename T>
void PacketFactory::free(T *pkt) {
  if( unlikely(!reinterpret_cast<common::FixedQueue<void>*>(pkt->get_pool()) ->push(pkt)) )
    ::free(pkt);
}

template<typename A, typename B, PacketType TYPE>
class Packet {
public:
  typedef A Request;
  typedef B Reply;

  Packet(Controller *cntl, Closure *done,
      const Request* request, Reply* reply, void *pool = NULL) :
    type_(TYPE),
    cntl_(cntl), 
    done_(done), 
    request_(request), 
    reply_(reply), 
    rtime_(common::local_time()),
    pool_(pool)
  {}

  void finish() {
    done_->Run();
    PacketFactory::free(this);
  }

  void finish_with_error(const StatusCode code, const string &err) {
    cntl_->SetFailed(code, err.c_str());
    done_->Run();
    PacketFactory::free(this);
  }

  const Request& get_request() const {
    return *request_;
  }

  Reply& get_reply() {
    return *reply_;
  }

  uint64_t receive_time() const {
    return rtime_; 
  }

  PacketType get_type() const {
    return type_;
  }

  void * get_pool() const {
    return pool_;
  }

protected:
  PacketType      type_;
  Controller*     cntl_;
  Closure*        done_;
  const Request*  request_;
  Reply*          reply_;
  uint64_t        rtime_;
  void*           pool_;
};

typedef Packet<int64_t,       int64_t,     PKT_NULL>   NullPacket;
typedef Packet<StartRequest,  StartReply,  PKT_START>  StartPacket;
typedef Packet<GetRequest,    GetReply,    PKT_GET>    GetPacket;
typedef Packet<PutRequest,    PutReply,    PKT_PUT>    PutPacket;
typedef Packet<CommitRequest, CommitReply, PKT_COMMIT> CommitPacket;
typedef Packet<BatchRequest,  BatchReply,  PKT_BATCH_PUT> BatchPutPacket;

inline PacketType get_type(void *pkt) {
  return reinterpret_cast<NullPacket*>(pkt)->get_type();
}


//typedef DbInterface::AsyncService AsyncService;
/*
class PacketFactory;
class Packet { 
  friend class PacketFactory;
public:
  Packet(PacketType type, AsyncService* service, ServerCompletionQueue* cq) 
    : type_(type), service_(service), cq_(cq), status_(CREATE) {}

  virtual ~Packet() {}

  virtual void wait() = 0;

  virtual void handle();
  
  void Proceed();

  uint64_t receive_time() const { return rtime_; }

  PacketType type() const {
    return type_;
  }

protected:
  PacketType type_;
  DbInterface::AsyncService* service_;
  ServerCompletionQueue* cq_;
  ServerContext ctx_;
  PacketStatus status_;

  uint64_t rtime_; //receive time
};
*/

#endif
