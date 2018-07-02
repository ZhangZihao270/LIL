#include "tpcc_workload.h"
#include "common/debuglog.h"
#include "common/global.h"
#include "tpcc_proc.h"
#include <string>
#include <vector>
#include <math.h>
#include <set>
#include <algorithm>
#include <functional>
using namespace std;
using namespace common;
string make_string(int *list, int n) {
  string ret = "[ ";
  for(int i = 1; i <= n; ++i) {
    ret.append(to_string(list[i]));
    if( i != n ) ret.append(", ");
  }
  ret.append(" ]");
  return ret;
}

bool TpccWorkload::run_neworder() {
  int num_items = r_.random_int(5, 15);
  int item_ids[16], supplier[16], quantities[16];
  int all_local = 1;

  int w_id = w_id_;
  int d_id = r_.random_int(1, NumDistrictsPerWarehouse);
  int c_id = get_customer_id();

  for(int i = 1; i <= num_items; ++i) {
    item_ids[i] = get_item_id();
    if( r_.random_int(1, 100) > 1 ) {
      supplier[i] = w_id_;
    }
    else {
      do {
        supplier[i] = r_.random_int(1, w_cnt_);
      }while (supplier[i] == w_id_ && w_cnt_ > 1);
      all_local = 0;
    }
    quantities[i] = r_.random_int(1, 10);
  }
  if( r_.random_int(1, 100) == 1 )
    item_ids[num_items] = -1;

  LOG_DEBUG("NewOrder: w_id: %d, d_id: %d, c_id: %d, num_items: %d, item_ids: %s",
      w_id, d_id, c_id, num_items, make_string(item_ids, num_items).c_str());

  bool ret = false;
  int64_t retries = 0; 
  do {
    ret = proc_.run_neworder(*client_, w_id, d_id, c_id, num_items,
        item_ids, supplier, quantities, all_local);
    proc_.end(*client_, ret);
    ++retries;
  } while( !ret  && StatusCode::ABORTED == client_->error_code() && retries < TRANS_RETRY_TIMES );
  LOG_DEBUG("RETRY NEWORDER: %lld", retries);
  return ret; 
}

bool TpccWorkload::run_payment() {
  int w_id = w_id_;
  int d_id = r_.random_int(1, NumDistrictsPerWarehouse);
  int c_id = get_customer_id(); 

  int x = r_.random_int(1, 100);

  int c_w_id, c_d_id;

  if( x <= 85 ) {
    c_w_id = w_id;
    c_d_id = d_id;
  }
  else {
    c_d_id = r_.random_int(1, NumDistrictsPerWarehouse);
    do {
      c_w_id = r_.random_int(1, w_cnt_);
    } while (c_w_id == w_id && w_cnt_ > 1);
  }

  double amount = r_.random_int(100, 500000) / 100.0; 

  LOG_DEBUG("Payment: w_id: %d, d_id: %d, c_w_id: %d, c_d_id: %d, c_id: %d, amount: %lf",
      w_id, d_id, c_w_id, c_d_id, c_id, amount);
 
  bool ret = false;
  int64_t retries = 0;
  do {
    ret = proc_.run_payment(*client_, w_id, d_id, c_w_id, c_d_id, c_id, amount);
    proc_.end(*client_, ret);
    ++retries;
  } while ( !ret && StatusCode::ABORTED == client_->error_code() && retries < TRANS_RETRY_TIMES);
  LOG_DEBUG("RETRY PAYMENT: %lld", retries);
  return ret;
}

bool TpccWorkload::run_stock_level() {
  return false;
}

bool TpccWorkload::run_order_status() {
  return false;
}

bool TpccWorkload::run_delivery() {
  return false;
}

pair<bool, int64_t> TpccWorkload::run() {
  int num = r_.random_int(1, 100);
  if( num <= PER_NEWORDER ) {
    return make_pair(run_neworder(), NEWORDER);
  }
  else if( num <= PER_NEWORDER + PER_PAMENT ) {
    return make_pair(run_payment(), PAYMENT);
  }
  else if( num <= PER_NEWORDER + PER_PAMENT + PER_STOCK_LEVEL ) {
    return make_pair(run_stock_level(), STOCK_LEVEL);
  }
  else if( num <= PER_NEWORDER + PER_PAMENT + PER_STOCK_LEVEL + PER_ORDER_STATUS ) {
    return make_pair(run_order_status(), ORDER_STATUS);
  }
  else {
    assert(num <= 100);
    return make_pair(run_delivery(), DELIVERY);
  }
  return make_pair(false, -1);
}

/******************************************MicroBenchmark**********************************************/

double MbWorkload::zeta(int64_t n, double theta){
  double sum = 0;
  for(int64_t i = 1; i <= n; i++){
    sum += pow(1.0/i, theta);
  }
  return sum;
}

int64_t MbWorkload::zipf(){
  static double zeta_2_theta = zeta(2, theta_);
  static double denom = zeta(table_size_, theta_);
  double alpha = 1 / (1 - theta_);
  double eta = (1 - pow(2.0 / table_size_, 1 - theta_))/
      (1 - zeta_2_theta / denom);
  double u = r_.random_double(0,1);
  double uz = u * denom;
  //LOG_DEBUG("zipf: u.%lf denom:%lf theta:%lf table_size:%ld",u, denom, theta_, table_size_);
  if (uz < 1) return 1;
  if (uz < (1 + pow(0.5, theta_))) return 2;
  //LOG_DEBUG("zipf: return: %ld", 1 + (int64_t)(table_size_ * pow(eta*u - eta + 1, alpha)));
  return 1 + (int64_t)(table_size_ * pow(eta*u - eta + 1, alpha));
}

void MbWorkload::gen_request(vector<TpccProc::MbRequest> &mb_requesets){
  set<int64_t> all_keys;
  for(int64_t i = 0; i < r_cnt_; i++){
    int64_t key = zipf();
    if(all_keys.find(key) == all_keys.end()){
      all_keys.insert(key);
    }else{
      i--; continue;
    }
    mb_requesets.emplace_back(TpccProc::MbRequest(key,0,0));
    //LOG_DEBUG("generate read request(%d %d)", key, 0);
  }

  for(int64_t i = 0; i < w_cnt_; i++){
    int64_t key = zipf();
    if(all_keys.find(key) == all_keys.end()){
      all_keys.insert(key);
    }else{
      i--; continue;
    }
    int32_t value = r_.random_int(0, 1<<8);
    mb_requesets.emplace_back(TpccProc::MbRequest(key,1,value));
    //LOG_DEBUG("generate write request(%d %d)", key, value);
  } 
  std::sort(mb_requesets.begin(), mb_requesets.end(), 
    [](const TpccProc::MbRequest &a, const TpccProc::MbRequest &b){
    return a.key < b.key;
  });
//  for(auto iter = mb_requesets.begin(); iter != mb_requesets.end(); ++iter) {
//    LOG_INFO("key: %ld, mode: %ld", iter->key, iter->mode);
//  }
//  TpccProc::MbRequest temp(0,0,0);
//  int64_t size = r_cnt_ + w_cnt_;
//  for (int64_t i = 0; i < size; i++){
//    for (int64_t j = 0; j < size - i; j++)
//      if (mb_requesets[j].key < mb_requesets[j+1].key){
//        temp = mb_requesets[j];
//        mb_requesets[j] = mb_requesets[j+1];
//        mb_requesets[j+1] = temp;
//      }
//  }
  //for (int64_t i = 0; i < mb_requesets.size(); i++){
  //  LOG_INFO("generate request(%d %d %d)", mb_requesets[i].mode, mb_requesets[i].key, mb_requesets[i].value);
  //}
}

//generate MicroBenchmark workload as the form of vector<mbrequest>
//run_miniworload()
//proc_.run_miniworkload();proc_.end();
pair<bool, int64_t> MbWorkload::run_miniworkload() {
  vector<TpccProc::MbRequest> mb_requesets;

  //mb_requesets.emplace_back(TpccProc::MbRequest(1,0,1));
  //mb_requesets.emplace_back(TpccProc::MbRequest(1,1,1));

  gen_request(mb_requesets);
  int low_bound = 0, up_bound = table_size_;
  for(auto iter = mb_requesets.begin(); iter != mb_requesets.end(); ++iter) {
    assert(iter->key > low_bound && iter->key <= up_bound);
    low_bound = iter->key;
  }
  bool ret = false;
  int64_t retries = 0;
  do {
    ret = proc_.run_miniworkload(*client_, mb_requesets);
    proc_.end(*client_, ret);
    ++retries;
  } while ( !ret && StatusCode::ABORTED == client_->error_code() );
  //while ( !ret && StatusCode::ABORTED == client_->error_code() && retries < TRANS_RETRY_TIMES );
  LOG_DEBUG("RETRY TIMES: %lld", retries);

  return make_pair(ret, MINIWORKLOAD);
}
