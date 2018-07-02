#include <butil/logging.h>
#include <butil/time.h>
#include <gflags/gflags.h>
#include <butil/logging.h>
#include <butil/time.h>

#include "tpcc_client.h"
#include "common/tpcc.h"
#include "common/debuglog.h"
#include "common/random.h"
#include "common/clock.h"
#include "tpcc_workload.h"
#include "common/global.h"

using namespace tpcc;

void dump(const char *data, const int64_t len) {
  printf(" %lld ", len);
  for(int i = 0; i < len; ++i) 
    printf("%x", (int)(data[i]));
  printf("\n");
}

int TpccClient::start_trans(const uint64_t trans_id, uint64_t &ret) {

  StartRequest request;
  request.set_transid(trans_id);

  StartReply reply;

  brpc::Controller cntl;
  cntl.set_log_id(log_id_ ++);

  stub_.start(&cntl, &request, &reply, NULL);

  if( !cntl.Failed() )  {
    last_error_code_ = 0;
    ret = reply.transid();
    LOG_DEBUG("trans [%lld] start", ret);
    return 0;
  }
  else {
    last_error_code_ = cntl.ErrorCode();
    LOG_ERROR("start trans error: %d, reason: %s", 
        cntl.ErrorCode(), 
        cntl.ErrorText().c_str());
    return -1;
  }
}

int TpccClient::end_trans(uint64_t &trans_id, bool to_commit) {
  
  CommitRequest request;
  request.set_transid(trans_id);
  request.set_decision(to_commit ? TXN_COMMIT : TXN_ABORT);

  CommitReply reply;

  brpc::Controller cntl;

  stub_.commit(&cntl, &request, &reply, NULL);

  if( !cntl.Failed() ) {
    //last_error_code_ = 0;
    LOG_DEBUG("trans [%lld] end", trans_id);
    trans_id = UINT64_MAX;
    return 0;
  }
  else {
    //last_error_code_ = cntl.ErrorCode();
    LOG_ERROR("trans_id [%lld] end trans error: %d, reason: %s", 
        trans_id, 
        cntl.ErrorCode(), 
        cntl.ErrorText().c_str());
    return -1;
  }
}

int TpccClient::get(const uint64_t trans_id, const int64_t table_id, 
    const int64_t cols, const string &key, string &value, const Lock_type lock_mode) {
  GetRequest request;
  request.set_transid(trans_id);
  request.set_tableid(table_id);
  request.set_cols(cols);
  request.set_key(key);
  request.set_lockmode(lock_mode);

  brpc::Controller cntl;

  //dump(key.c_str(), key.size());

  LOG_DEBUG("trans [%lld] send get request", trans_id);
  int64_t start_time = common::local_time();

  GetReply reply;
  stub_.get(&cntl, &request, &reply, NULL);

  STAT_INC(STAT_CLIENT, CLIENT_GET_TIME, common::local_time() - start_time);
  STAT_INC(STAT_CLIENT, CLIENT_GET_COUNT, 1);

  if( !cntl.Failed() ) {
    last_error_code_ = 0;
    value = reply.value();
    return 0;
  }
  else {
    last_error_code_ = cntl.ErrorCode();
    if( cntl.ErrorCode() >= 16 ) {
      LOG_ERROR("trans_id [%lld] get failed, code=%d, msg=%s",
          trans_id,
          cntl.ErrorCode(),
          cntl.ErrorText().c_str());
    }
    else {
      LOG_DEBUG("trans_id [%lld] get failed, code=%d, msg=%s",
          trans_id,
          cntl.ErrorCode(),
          cntl.ErrorText().c_str());
    }
    return -1;
  }
}

int TpccClient::insert(const uint64_t trans_id, const int64_t table_id,
    const int64_t cols, const string &key, const string &value) {
  return put(DML_INSERT, trans_id, table_id, cols, key, value);
}

int TpccClient::update(const uint64_t trans_id, const int64_t table_id,
    const int64_t cols, const string &key, const string &value) {
  return put(DML_UPDATE, trans_id, table_id, cols, key, value);
}

int TpccClient::replace(const uint64_t trans_id, const int64_t table_id,
    const int64_t cols, const string &key, const string &value) {
  return put(DML_REPLACE, trans_id, table_id, cols, key, value);
}

int TpccClient::put(const DmlType type, const uint64_t trans_id, const int64_t
    table_id, const int64_t cols, const string &key, const string &value) {
  PutRequest request;
  request.set_transid(trans_id);
  request.set_tableid(table_id);
  request.set_dmltype(type);
  request.set_cols(cols);
  request.set_key(key);
  request.set_value(value);

  brpc::Controller cntl;

  LOG_DEBUG("trans [%lld] send put request", trans_id);
  int64_t start_time = common::local_time();

  PutReply reply;
  stub_.put(&cntl, &request, &reply, NULL);
  
  STAT_INC(STAT_CLIENT, CLIENT_PUT_TIME, common::local_time() - start_time);
  STAT_INC(STAT_CLIENT, CLIENT_PUT_COUNT, 1);
  
  if( !cntl.Failed() ) {
    last_error_code_ = 0;
    return 0;
  }
  else {
    last_error_code_ = cntl.ErrorCode();
    if( cntl.ErrorCode() >= 16 ) {
      LOG_ERROR("trans_id [%lld], put failed, code=%d, msg=%s",
          trans_id,
          cntl.ErrorCode(),
          cntl.ErrorText().c_str());
    }
    return -1;
  }
}

int TpccClient::batch_put(uint64_t &trans_id, const string &data) {
  BatchRequest request;
  request.set_transid(trans_id);
  request.set_data(data);

  brpc::Controller cntl;
  
  BatchReply reply;
  LOG_DEBUG("trans [%lld] send batch put", trans_id);
  stub_.batch_put(&cntl, &request, &reply, NULL);

  if( !cntl.Failed() ) {
    last_error_code_ = 0;
    trans_id = UINT64_MAX;
    return 0;
  }
  else {
    last_error_code_ = cntl.ErrorCode();
    if( cntl.ErrorCode() >= 16 ) {
      LOG_ERROR("trans [%lld] put failed, code=%d, msg=%s", 
          trans_id,
          cntl.ErrorCode(),
          cntl.ErrorText().c_str());
    }
    trans_id = UINT64_MAX;
    return -1; 
  }
}

int TpccClient::get_stat(StatManager &mgr) {
  StatRequest request;
  request.set_nope(0);

  brpc::Controller cntl;

  StatReply reply;
  stub_.get_stat(&cntl, &request, &reply, NULL);

  if( !cntl.Failed() ) {
    int64_t pos = 0;
    const string &buf = reply.value();
    if( 0 == mgr.read(buf.c_str(), buf.size(), pos) )
      return 0;
    else
      return -1;
  }
  else {
    LOG_ERROR("get_stat failed, code=%d, msg=%s", 
        cntl.ErrorCode(),
        cntl.ErrorText().c_str());
    return -1;
  }
}

void tpcc::run_client(int thread_num, int last_time) {
  brpc::Channel channel;
  if( 0 != channel.Init("localhost:50001", NULL) ) {
    LOG_ERROR("Fail to initialize channel");
  }

  TpccClient tpcc(channel);

  TpccProc proc;
  int item_ids[] = {0, 1, 2};
  int supplier[] = {0, 1, 1};
  int quantities[] = {0, 3, 4};
  Random r;
  bool ret = false;
  if( r.random_int(1, 2) == 1 )
    ret = proc.run_neworder(tpcc, 1, 1, 1, 2, item_ids, supplier, quantities, 1);
  else {
    item_ids[1] = -1;
    ret = proc.run_neworder(tpcc, 1, 1, 1, 2, item_ids, supplier, quantities, 1);
  }
  tpcc.end_trans(proc.transid(), ret);
}
