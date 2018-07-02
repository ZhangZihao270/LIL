#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include "tpcc_runner.h"
#include "common/log.h"
#include "common/clock.h"
#include "common/pthread_barrier_t.h"
#include "common/global.h"
#include "common/atomic.h"

using namespace  common;

const string & ClientStat::trans_name(int64_t trans_id) {
  static const string __name[] = {
    "NewOrder", 
    "Payment", 
    "StockLevel",
    "OrderStatus", 
    "Delivery" };
  assert(trans_id >= 0 && trans_id < 5);
  return __name[trans_id];
}

void ClientStat::print_info(ofstream &out) {
  out << "client " << client_index_ << ":" << endl;
  for (int64_t i = 0; i < results_.size(); ++i) {
    out << "id: "  << i << ", "
        << "stime: "  << results_[i].start_time_ << ", "
        << "etime: "  << results_[i].end_time_ <<  ", "
        << "type: "  << trans_name(results_[i].type_) << ", "
        << "result: "  << (results_[i].res_ ? "Success" : "Fail")
        << endl;
  }
  out << endl;
}

void ClientStat::add_info(bool res, int64_t start_time, 
    int64_t end_time, int64_t trans_t) {
  results_.emplace_back(res, start_time, end_time, trans_t);
}

TpccRunner::~TpccRunner() {
  LOG_INFO("end");
  for(int i = 0; i < ctx_.size(); ++i) {
    //LOG_INFO("clear client context %d", i);
    delete ctx_[i];
  }
}

void TpccRunner::launch() {
  int ret = 0;

  running_flag_ = true;

  brpc::ChannelOptions option;
  option.timeout_ms = -1;
  channel_.Init(server_addr_.c_str(), &option);
  __sync_synchronize();

  //pthread_barrier_init(&barrier_, NULL, client_num_);

  for(int64_t i = 0; i < client_num_; i++) {
    int64_t ware_no = (i % (ware_end_no_ - ware_start_no_ + 1)) + ware_start_no_;
    ctx_.emplace_back(new TpccRunnerContext(this, server_addr_, channel_, ware_no, warehouse_num_, i));
  }

  for (int64_t i = 0; i < client_num_; i++){
    if (0 == (ret = pthread_create(&ctx_[i]->thd_, NULL, thread_func, ctx_[i]))) {
    //if ( 0 == (ret = bthread_start_background(&ctx_[i]->thd_, NULL, thread_func, ctx_[i])) ) {
      LOG_INFO("client[%lld] is started, connected to %s", i, ctx_[i]->conn_addr_.c_str());
    }
    else {
      LOG_ERROR("client[%lld] is not started, code: %d", i, ret);
      stop();
    }
  }

  for (int64_t i = 0; i < client_num_; i++) {
    //bthread_join(ctx_[i]->thd_, NULL);
    pthread_join(ctx_[i]->thd_, NULL);
  }

  if( leader_ ) {
    //bthread_usleep(1000000);
    StatManager mgr(STAT_SERVER);
    TpccClient client(channel_);
    if( 0 == client.get_stat(mgr) ) {
      mgr.report();
    }
  }
}

void TpccRunner::stop() {
  running_flag_ = false;
  __sync_synchronize();
}

bool TpccRunner::expired(int64_t expiration_time) {
  if( sim_done_ ) return true;
  if( common::local_time() >= expiration_time ) {
    atomic_compare_exchange(&sim_done_, true, false);
  } 
  return sim_done_;
}

void* TpccRunner::thread_func(void * ptr) {
  TpccRunnerContext * const ctx = reinterpret_cast<TpccRunnerContext *> (ptr);
  TpccWorkload &wrk = ctx->workload_;
  TpccRunner *host = ctx->tpcc_runner_;

  //pthread_barrier_wait(&host->barrier());
  int64_t expiration_time = common::local_time() + host->run_time(), start_time, end_time;

  while ( !host->stop_() && !host->expired(expiration_time) ) {

    start_time = common::local_time();

    pair<bool, int64_t> ret = wrk.run();
    //wrk.run_miniworkload();
    end_time = common::local_time();
    if( ret.first ) {
      STAT_INC(STAT_CLIENT, CLIENT_SUCCESS_COUNT, 1);
      STAT_INC(STAT_CLIENT, CLIENT_LATENCY, end_time - start_time);
    }
    else
      STAT_INC(STAT_CLIENT, CLIENT_FAIL_COUNT, 1);

    ctx->client_stat_.add_info(ret.first, start_time, end_time, ret.second);
    LOG_DEBUG("running... client(%lld) %lld : %lld", ctx->index_, common::local_time(), expiration_time);
  }
  return NULL;
}

void TpccRunner::report() {
  int64_t total_succ_trans = STAT_GET(STAT_CLIENT, CLIENT_SUCCESS_COUNT);
  int64_t total_fail_trans = STAT_GET(STAT_CLIENT, CLIENT_FAIL_COUNT);
  int64_t total_latency_trans = STAT_GET(STAT_CLIENT, CLIENT_LATENCY);
  LOG_INFO("*******************************************************************");
  LOG_INFO("Reporting..........");

  LOG_INFO("Transaction: succuss_count:%lld fail_count:%lld average_latency:%.2lfs", total_succ_trans, total_fail_trans,
           total_latency_trans*1.0/total_succ_trans/1000000);

  LOG_INFO("*******************************************************************");
  LOG_INFO("Dumping.........");
  ofstream out(dump_path_);
  for(int64_t i = 0; i < client_num_; i++){
    ClientStat &cst = ctx_[i]->client_stat_;
    cst.print_info(out);
  }
  LOG_INFO("Dumped success. PATH(%s)", dump_path_.c_str());
}


/*********************************microbenchmark*************************************/

MbRunner::~MbRunner() {
  LOG_INFO("end");
  for(int i = 0; i < ctx_.size(); ++i) {
    delete ctx_[i];
  }
}

void MbRunner::launch() {
  int ret = 0;

  running_flag_ = true;

  brpc::ChannelOptions option;
  option.timeout_ms = -1;
  channel_.Init(server_addr_.c_str(), &option);
  __sync_synchronize();

  //pthread_barrier_init(&barrier_, NULL, client_num_);

  for(int64_t i = 0; i < client_num_; i++) {
    ctx_.emplace_back(new MbRunnerContext(this, server_addr_, channel_,
                                          r_cnt_, w_cnt_, table_size_, theta_, i));
  }

  for (int64_t i = 0; i < client_num_; i++){
    if (0 == (ret = pthread_create(&ctx_[i]->thd_, NULL, thread_func, ctx_[i]))) {
    //if ( 0 == (ret = bthread_start_background(&ctx_[i]->thd_, NULL, thread_func, ctx_[i])) ) {
      LOG_INFO("client[%lld] is started, connected to %s", i, ctx_[i]->conn_addr_.c_str());
    }
    else {
      LOG_ERROR("client[%lld] is not started, code: %d", i, ret);
      stop();
    }
  }

  for (int64_t i = 0; i < client_num_; i++) {
    //bthread_join(ctx_[i]->thd_, NULL);
    pthread_join(ctx_[i]->thd_, NULL);
  }

  if( leader_ ) {
    //bthread_usleep(1000000);
    StatManager mgr(STAT_SERVER);
    TpccClient client(channel_);
    if( 0 == client.get_stat(mgr) ) {
      mgr.report();
    }
  }
}

void MbRunner::stop() {
  running_flag_ = false;
  __sync_synchronize();
}

bool MbRunner::expired(int64_t expiration_time) {
  if( sim_done_ ) return true;
  if( common::local_time() >= expiration_time ) {
    atomic_compare_exchange(&sim_done_, true, false);
  }

  return sim_done_;
}

void* MbRunner::thread_func(void * ptr) {
  MbRunnerContext * const ctx = reinterpret_cast<MbRunnerContext *> (ptr);
  MbWorkload &wrk = ctx->workload_;
  MbRunner *host = ctx->mb_runner_;

  //pthread_barrier_wait(&host->barrier());
  int64_t expiration_time = common::local_time() + host->run_time(), start_time, end_time;

  while ( !host->stop_() && !host->expired(expiration_time) ) {

    start_time = common::local_time();

    pair<bool, int64_t> ret = wrk.run_miniworkload();
    end_time = common::local_time();
    if( ret.first ) {
      STAT_INC(STAT_CLIENT, CLIENT_SUCCESS_COUNT, 1);
      STAT_INC(STAT_CLIENT, CLIENT_LATENCY, end_time - start_time);
    }
    else
      STAT_INC(STAT_CLIENT, CLIENT_FAIL_COUNT, 1);

    ctx->client_stat_.add_info(ret.first, start_time, end_time, ret.second);
    LOG_DEBUG("running... client(%lld) %lld : %lld", ctx->index_, common::local_time(), expiration_time);
  }
  return NULL;
}

void MbRunner::report() {
  int64_t total_succ_trans = STAT_GET(STAT_CLIENT, CLIENT_SUCCESS_COUNT);
  int64_t total_fail_trans = STAT_GET(STAT_CLIENT, CLIENT_FAIL_COUNT);
  int64_t total_latency_trans = STAT_GET(STAT_CLIENT, CLIENT_LATENCY);
  LOG_INFO("*******************************************************************");
  LOG_INFO("Reporting..........");

  LOG_INFO("Transaction: succuss_count:%lld fail_count:%lld average_latency:%.2lfs", total_succ_trans, total_fail_trans,
           total_latency_trans*1.0/total_succ_trans/1000000);

  LOG_INFO("*******************************************************************");
  LOG_INFO("Dumping.........");
  ofstream out(dump_path_);
  for(int64_t i = 0; i < client_num_; i++){
    ClientStat &cst = ctx_[i]->client_stat_;
    cst.print_info(out);
  }
  LOG_INFO("Dumped success. PATH(%s)", dump_path_.c_str());
}
