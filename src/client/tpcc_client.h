#ifndef TPCC_CLIENT_
#define TPCC_CLIENT_

#include <iostream>
#include <memory>
#include <string>
#include <brpc/channel.h>
#include "common/db.pb.h"
#include "common/define.h"
#include "common/global.h"

using namespace std;
using namespace dbinter;
using namespace common;

class StatManager;
namespace tpcc {
  class TpccClient {
    public:
      TpccClient(brpc::Channel &channel)
        : stub_(&channel), log_id_(0) {}

      int start_trans(const uint64_t trans_id, uint64_t &ret);

      int end_trans(uint64_t &trans_id, bool to_commit = true);

      int get(const uint64_t trans_id, const int64_t table_id, 
          const int64_t cols, const string &key, string &value, 
          const Lock_type lock_mode = L_READ);

      int insert(const uint64_t trans_id, const int64_t table_id, 
          const int64_t cols, const string &key, const string &value);

      int update(const uint64_t trans_id, const int64_t table_id, 
          const int64_t cols, const string &key, const string &value);

      int replace(const uint64_t trans_id, const int64_t table_id, 
          const int64_t cols, const string &key, const string &value);

      int batch_put(uint64_t &trans_id, const string &data);

      int get_stat(StatManager &mgr);

      int64_t error_code() const {
        return last_error_code_;
      }

      static void *run(void *task);
    private:
      int put(const DmlType type, const uint64_t trans_id, const int64_t table_id,
          const int64_t cols, const string &key, const string &value);

    private:
      DbService_Stub stub_;
      int64_t        log_id_;
      int64_t        last_error_code_;
  };

  void run_client(int thread_num, int last_time);
}

#endif
