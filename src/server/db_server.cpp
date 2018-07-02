#include "db_server.h"
#include "packet.h"
#include "common/debuglog.h"
#include "db_worker.h"
#include "common/define.h"

using common::StatusCode;

using namespace dbserver;

int DbServiceImpl::run(const int64_t port, const int64_t io_thread_num) {
  brpc::Server server;

  DbServiceImpl imp;

  if( server.AddService(&imp,
        brpc::SERVER_DOESNT_OWN_SERVICE) != 0 ) {
    LOG_ERROR("Fail to add service");
    return -1;
  }

  brpc::ServerOptions options;

  options.num_threads = io_thread_num;
  //options.max_concurrency = io_thread_num;
  options.internal_port =  -1;

  if( server.Start(port, &options) != 0 ) {
    LOG_ERROR("fail to start db server");
    return -1;
  }

  server.RunUntilAskedToQuit();
  return 0;
}

void DbServiceImpl::start(RpcController* cntl_base,
    const StartRequest* request,
    StartReply* reply,
    Closure* done) {
  //create a start packet, and add into the working queue
  StartPacket *pkt = PacketFactory::alloc<StartPacket>(
      static_cast<brpc::Controller*>(cntl_base),
      done, 
      request, 
      reply);
  if(!DbWorker::worker()->handle_packet(pkt, -1)) {
    pkt->finish_with_error(StatusCode::INTERNAL, "there is no available thread");
  }
}

void DbServiceImpl::get(RpcController* cntl_base,
    const GetRequest* request,
    GetReply* reply,
    Closure* done) {
  GetPacket* pkt = PacketFactory::alloc<GetPacket>(
      static_cast<brpc::Controller*>(cntl_base),
      done,
      request,
      reply);
  DbWorker::worker()->handle_packet(pkt, request->transid());
}

void DbServiceImpl::put(RpcController* cntl_base,
    const PutRequest* request,
    PutReply* reply,
    Closure* done) {
  PutPacket *pkt = PacketFactory::alloc<PutPacket>(
      static_cast<brpc::Controller*>(cntl_base),
      done,
      request,
      reply);
  DbWorker::worker()->handle_packet(pkt, request->transid());
}

void DbServiceImpl::commit(RpcController* cntl_base,
    const CommitRequest* request,
    CommitReply* reply,
    Closure* done) {
  CommitPacket *pkt = PacketFactory::alloc<CommitPacket>(
      static_cast<brpc::Controller*>(cntl_base),
      done,
      request,
      reply);
  DbWorker::worker()->handle_packet(pkt, request->transid());
}

void DbServiceImpl::batch_put(RpcController* cntl_base,
    const BatchRequest* request,
    BatchReply* reply,
    Closure* done) {
  BatchPutPacket *pkt = PacketFactory::alloc<BatchPutPacket>(
      static_cast<brpc::Controller*>(cntl_base),
      done,
      request,
      reply);
  DbWorker::worker()->handle_packet(pkt, request->transid());
}

void DbServiceImpl::get_stat(RpcController* cntl_base,
    const StatRequest* request,
    StatReply* reply,
    Closure* done) {
  char buffer[4096];
  int64_t pos = 0;
  if( SUCCESS == StatManager::instance()->write(buffer, 4096, pos) ) {
    string val(buffer, pos);
    reply->set_value(val);
    done->Run();
  }
  else {
    static_cast<brpc::Controller*>(cntl_base)->SetFailed(INTERNAL, "buffer overflow");
    done->Run();
  }
}

/*
void* DbServerAsync::handle_rpcs(void *data) {

  ThreadContext const * ctx = reinterpret_cast<ThreadContext *>(data);

  ServerBuilder builder;
  AsyncService  service;

  builder.AddListeningPort(ctx->addr_, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  unique_ptr<ServerCompletionQueue> cq =builder.AddCompletionQueue();
  unique_ptr<Server> server = builder.BuildAndStart();

  LOG_INFO("Server listening on: %s", ctx->addr_.c_str());

  DbServerAsync *host = ctx->host_;

  host->thread_index() = ctx->id_;

  StartPacket *p_start = PacketFactory::alloc<StartPacket>(&service, cq.get());
  PutPacket *p_put = PacketFactory::alloc<PutPacket>(&service, cq.get());
  GetPacket *p_get = PacketFactory::alloc<GetPacket>(&service, cq.get());
  CommitPacket *p_commit = PacketFactory::alloc<CommitPacket>(&service, cq.get());

  void *tag;
  bool ok;
  while( true ) {
    GPR_ASSERT(cq->Next(&tag, &ok));
    GPR_ASSERT(ok);
    LOG_DEBUG("io thread [%lld] handle packet", host->get_thread_index());
    static_cast<Packet*>(tag) -> Proceed();
  }

  server->Shutdown();
  cq->Shutdown();
  return NULL;
}

void DbServerAsync::Run(const string addr, int64_t io_thread_num) {

  for(int i = 0; i < io_thread_num; ++i) {
    ctxs_.emplace_back();
    ctxs_.back().addr_ = addr + ":" + to_string(i + 50000);
    ctxs_.back().host_    = this;
    ctxs_.back().id_      = i;
  }

  int ret = 0;
  for(int i = 0; 0 == ret && i < io_thread_num; ++i) {
    ret = pthread_create(&ctxs_[i].thd_, NULL, DbServerAsync::handle_rpcs, &ctxs_[i]);
    if( 0 == ret ) {
      LOG_INFO("io thread [%d] is started", i);
    }
    else {
      LOG_ERROR("io thread [%d] starts with failure", i);
    }
  }
  wait_for_stop();
}

void DbServerAsync::wait_for_stop() {
  int ret = 0;
  void *val = NULL;
  for(int i = 0; i < ctxs_.size(); ++i) {
    ret = pthread_join(ctxs_[i].thd_, &val);
    if( ret == 0 ) {
      LOG_INFO("io thread [%d] is ended", i);
    }
    else {
      LOG_ERROR("io thread [%d] ends with failure", i);
    }
  }
}
*/
