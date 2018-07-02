#include <iostream>
#include <memory>
#include <string>
#include <pthread.h>

#include "common/define.h"
#include "common/db.pb.h"
#include <brpc/server.h>

using namespace std;
using namespace dbinter;

namespace dbserver {

  /*
  class DbServerAsync final { 
  public:
    ~DbServerAsync () {
      //server_->Shutdown();
      //cq_->Shutdown(); 
    }

    void Run(const string addr, int64_t io_thread_num);

    void wait_for_stop();

    const int64_t get_thread_index() const { 
      return const_cast<DbServerAsync*>(this) -> thread_index();
    }

  private:
    typedef DbInterface::AsyncService AsyncService;

    struct ThreadContext {
      pthread_t                         thd_;
      //unique_ptr<ServerCompletionQueue>  cq_;
      //AsyncService*                 service_;
      DbServerAsync*                   host_;
      string                           addr_;
      int64_t                            id_;
    };

    static void* handle_rpcs(void *);

    int64_t & thread_index() {
      static __thread int64_t index = 0;
      return index;
    }

  private:
    vector<ThreadContext> ctxs_;

    //unique_ptr<ServerCompletionQueue> cq_;
    //AsyncService service_;
    //unique_ptr<Server> server_;

    //const static int64_t IO_THREAD_NUM = 4;
  };
  */

  using google::protobuf::RpcController;
  using google::protobuf::Closure;

  class DbServiceImpl : public DbService {
  public:
    DbServiceImpl() {}
    ~DbServiceImpl() {};

    static int run(const int64_t port, const int64_t io_thread_num);

  private:
    void start(RpcController* cntl_base,
        const StartRequest* request,
        StartReply* reply, 
        Closure* done);

    void get(RpcController* cntl_base,
        const GetRequest* request,
        GetReply* reply,
        Closure* done);

    void put(RpcController* cntl_base,
        const PutRequest* request,
        PutReply* reply,
        Closure* done);

    void commit(RpcController* cntl_base,
        const CommitRequest* request,
        CommitReply* reply,
        Closure* done); 

    void batch_put(RpcController *cntl_base,
        const BatchRequest* request, 
        BatchReply* reply,
        Closure* done);

    void get_stat(RpcController* cntl_base,
        const StatRequest* request,
        StatReply* reply,
        Closure* done);
  };
}
