#ifndef TPCC_RUNNER_H
#define TPCC_RUNNER_H
#include <stdint.h>
#include <pthread.h>
#include <bthread/bthread.h>
#include <fstream>
#include <string>
#include "common/pthread_barrier_t.h"
#include "tpcc_client.h"
#include "tpcc_workload.h"
#include "common/global.h"
#include "common/define.h"

using namespace tpcc;
using namespace std;

class TpccRunner;

struct ClientStat {

  ClientStat(int64_t idx = 0) : client_index_(idx) {}

  void add_info(bool res, int64_t start_time, 
      int64_t end_time, int64_t trans_t);

  void print_info(ofstream &out);

  static const string & trans_name(int64_t trans_id);

  struct TransRes {
    TransRes(bool res,
        int64_t start_time,
        int64_t end_time,
        int64_t trans_t) :
      res_(res),
      start_time_(start_time),
      end_time_(end_time),
      type_(trans_t)
    {}

    bool    res_; //succ or fail
    int64_t start_time_;
    int64_t end_time_;
    int64_t type_; //payment neworder stock orderstatus delivery
  };

  vector<TransRes> results_;
  int64_t client_index_;
};

struct TpccRunnerContext {

  TpccRunnerContext(
      TpccRunner * tpcc_runner, 
      const string &addr,
      brpc::Channel &channel,
      const int64_t ware_no,
      const int64_t warehouse_num,
      const int64_t id) :
    index_(id), 
    tpcc_runner_(tpcc_runner),
    conn_addr_(addr),
    rpc_stub_(channel),
    workload_(&rpc_stub_, ware_no,  warehouse_num),
    client_stat_(id),
    running_flag_(false) 
  {}

  ~TpccRunnerContext(){}

  bthread_t    thd_;
  //pthread_t    thd_;
  int64_t      index_;
  TpccRunner*  tpcc_runner_;
  string       conn_addr_;

  TpccClient   rpc_stub_;
  TpccWorkload workload_;

  ClientStat client_stat_;

  volatile bool running_flag_;
};


class TpccRunner {
public:
  TpccRunner(
      const std::string &server_addr, 
      int64_t client_num, 
      int64_t ware_start_no,
      int64_t ware_end_no,
      int64_t warehouse_num, 
      int64_t run_time,
      bool    leader,
      const std::string dump_path = "tpcc.log") : 
    server_addr_(server_addr),
    client_num_(client_num), 
    ware_start_no_(ware_start_no),
    ware_end_no_(ware_end_no),
    warehouse_num_(warehouse_num),
    run_time_(run_time),
    leader_(leader),
    running_flag_(false), 
    sim_done_(false), 
    ctx_(),
    dump_path_(dump_path)
    {}
 
  ~TpccRunner();

  void launch();

  void stop();

  void report();

  int64_t run_time() const { return run_time_ * 1000000; }

  const string & get_addr() const { return server_addr_; }

  pthread_barrier_t & barrier() { return barrier_; }

private:
  static void* thread_func(void* ptr);

  inline bool stop_() const{ return !running_flag_; }

  bool expired(int64_t expiration_time);

private:
  std::string server_addr_;
  const int64_t client_num_;
  const int64_t ware_start_no_;
  const int64_t ware_end_no_;
  const int64_t warehouse_num_;
  const int64_t run_time_;
  const bool    leader_;
  volatile bool running_flag_;
  volatile bool sim_done_;

  brpc::Channel channel_;
  vector<TpccRunnerContext*> ctx_;

  pthread_barrier_t barrier_;

  std::string dump_path_;
};
class MbRunner;
struct MbRunnerContext {

  MbRunnerContext(
      MbRunner * mb_runner,
      const string &addr,
      brpc::Channel &channel,
      const int64_t r_cnt,
      const int64_t w_cnt,
      const int64_t table_size,
      const double theta,
      const int64_t id) :
    index_(id),
    mb_runner_(mb_runner),
    conn_addr_(addr),
    rpc_stub_(channel),
    workload_(&rpc_stub_, r_cnt,  w_cnt, table_size, theta),
    client_stat_(id),
    running_flag_(false)
  {}

  ~MbRunnerContext(){}

  bthread_t    thd_;
  //pthread_t    thd_;
  int64_t      index_;
  MbRunner*  mb_runner_;
  string       conn_addr_;

  TpccClient   rpc_stub_;
  MbWorkload workload_;

  ClientStat client_stat_;

  volatile bool running_flag_;
};


class MbRunner {
public:
  MbRunner(
      const std::string &server_addr,
      int64_t client_num,
      int64_t r_cnt,
      int64_t w_cnt,
      int64_t table_size,
      double theta,
      int64_t run_time,
      bool    leader,
      const std::string dump_path = "microbenchmark.log") :
    server_addr_(server_addr),
    client_num_(client_num),
    r_cnt_(r_cnt),
    w_cnt_(w_cnt),
    table_size_(table_size),
    theta_(theta),
    run_time_(run_time),
    leader_(leader),
    running_flag_(false),
    sim_done_(false),
    ctx_(),
    dump_path_(dump_path)
    {}

  ~MbRunner();

  void launch();

  void stop();

  void report();

  int64_t run_time() const { return run_time_ * 1000000; }

  const string & get_addr() const { return server_addr_; }

  pthread_barrier_t & barrier() { return barrier_; }

private:
  static void* thread_func(void* ptr);

  inline bool stop_() const{ return !running_flag_; }

  bool expired(int64_t expiration_time);

private:
  std::string server_addr_;
  const int64_t r_cnt_;
  const int64_t w_cnt_;
  const int64_t client_num_;
  const int64_t table_size_;
  const double theta_;
  const int64_t run_time_;
  const bool    leader_;
  volatile bool running_flag_;
  volatile bool sim_done_;

  brpc::Channel channel_;
  vector<MbRunnerContext*> ctx_;

  pthread_barrier_t barrier_;

  std::string dump_path_;
};

#endif // TPCC_RUNNER_H
