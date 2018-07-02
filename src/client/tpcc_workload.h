#ifndef TPCC_WORKLOAD_
#define TPCC_WORKLOAD_

#include "tpcc_proc.h"
#include "common/tpcc.h"
#include "common/random.h"
#include <utility>
using namespace tpcc;
using namespace std;

class TpccWorkload {
public:

  TpccWorkload(TpccClient *client, int w_id, int w_cnt) :
    client_(client), proc_(), w_id_(w_id), w_cnt_(w_cnt), r_()
  {}

  pair<bool, int64_t> run();

  bool run_neworder();

  bool run_payment();

  bool run_stock_level();

  bool run_order_status();

  bool run_delivery();

private:
  inline int get_item_id() {
    return r_.non_uniform_random(8191, 7911, 1, common::NumItem);
  }

  inline int get_customer_id() {
    return r_.non_uniform_random(1023, 258, 1, common::NumCustomersPerDistrict);
  }

private:
  TpccClient *client_;
  TpccProc    proc_;
  int w_id_;
  int w_cnt_;

  Random r_;
};

//use tpccclient
class MbWorkload{
public:
  MbWorkload(TpccClient *client, int64_t r_cnt, int64_t w_cnt, int64_t table_size, double theta) :
    client_(client), proc_(), r_cnt_(r_cnt), w_cnt_(w_cnt), r_(), table_size_(table_size), theta_(theta)
  {
  }

public:
  pair<bool, int64_t> run_miniworkload();

private:

  double zeta(int64_t n, double theta);
  int64_t zipf();
  void gen_request(vector<TpccProc::MbRequest> &mb_requesets);

private:

  TpccClient *client_;
  TpccProc    proc_;
  int64_t r_cnt_;//read count for microbenchmark
  int64_t w_cnt_;//write count

  double theta_;
  int64_t table_size_;
  Random r_;


};

#endif
