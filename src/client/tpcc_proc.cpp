#include "tpcc_proc.h"
#include "common/tpcc.h"
#include "common/clock.h"
#include "common/debuglog.h"
#include <string>
#include <vector>
#include <set>
using namespace tpcc;
using namespace std;

bool TpccProc::run_payment(TpccClient &client, int w_id, int d_id, int c_w_id,
    int c_d_id, int c_id, double h_amount) {

  Warehouse w(w_id);
  District  d(w_id, d_id);
  Customer  c(c_w_id, c_d_id, c_id);

  //if( trans_id_ != UINT64_MAX ) {
  //  LOG_ERROR("unpexected trans id: %lld", trans_id_);
  //  assert(0);
  //}
  trans_id_ = UINT64_MAX;

  if( 0 != client.start_trans(trans_id_, trans_id_) ) return false;

  if( !read<Warehouse>(client, w.key, COL(Warehouse, w_ytd), w.value, L_WRITE) ) return false;
  w.value.w_ytd += h_amount;
  if( !update<Warehouse>(client, w.key, COL(Warehouse, w_ytd), w.value) ) return false;

  if( !read<Warehouse>(client, w.key,
        COL(Warehouse, w_street_1) |
        COL(Warehouse, w_street_2) |
        COL(Warehouse, w_city) |
        COL(Warehouse, w_state) | 
        COL(Warehouse, w_zip) |
        COL(Warehouse, w_name),
        w.value) )
    return false;

  if( !read<District>(client, d.key, COL(District, d_ytd), d.value, L_WRITE) )
    return false;
  d.value.d_ytd += h_amount;
  if( !update<District>(client, d.key, COL(District, d_ytd), d.value) )
    return false;

  if( !read<District>(client, d.key,
      COL(District, d_street_1) | 
      COL(District, d_street_2) |
      COL(District, d_city) |
      COL(District, d_state) |
      COL(District, d_zip)|
      COL(District, d_name),
      d.value) )
    return false;

  if( !read<Customer>(client, c.key,
      COL(Customer, c_credit) |
      COL(Customer, c_balance) |
      COL(Customer, c_ytd_payment) | 
      COL(Customer, c_payment_cnt),
      c.value, L_WRITE) )
    return false;

  c.value.c_balance -= h_amount;
  c.value.c_ytd_payment += h_amount;
  c.value.c_payment_cnt += 1;

  if( !update<Customer>(client, c.key, 
      COL(Customer, c_balance) |
      COL(Customer, c_ytd_payment) |
      COL(Customer, c_payment_cnt),
      c.value) )
    return false;

  if( c.value.c_credit.to_string() == "bc" ) {
    if( !read<Customer>(client, c.key, COL(Customer, c_data), c.value) ) {
      return false;
    }
    string c_data = to_string(c_id) + " " + 
      to_string(c_d_id) + " " +
      to_string(c_w_id) + " " + 
      to_string(d_id) + " " + 
      to_string(w_id) + " " +
      to_string(h_amount) + " " +
      c.value.to_string();
    if( !update<Customer>(client, c.key, COL(Customer, c_data), c.value) ) {
      return false;
    }
  }

  History h(c_id, c_d_id, c_w_id, d_id, w_id, common::local_time());
  h.value.h_amount = h_amount;
  h.value.h_data = w.value.w_name.to_string().substr(0, 10) +  " " + 
    d.value.d_name.to_string().substr(0, 10);

  if( !insert<History>(client, h.key, COL_ALL, h.value) ) {
    return false;
  }

  return true;
}

bool TpccProc::run_neworder(TpccClient &client, int w_id, int d_id, int c_id,
    int o_ol_cnt, int *items, int *suppliers, int *quantities, int o_all_local) {

  Customer c(w_id, d_id, c_id);
  Warehouse w(w_id);
  District d(w_id, d_id);

  trans_id_ = UINT64_MAX;
  if( 0 != client.start_trans(trans_id_, trans_id_) ) return false;

  if( !read<Customer>(client, c.key, 
        COL(Customer, c_discount) |
        COL(Customer, c_last) |
        COL(Customer, c_credit),
        c.value) ) {
    return false;
  }

  if( !read<Warehouse>(client, w.key, COL(Warehouse, w_tax), w.value) ) {
    return false;
  }

  if( !read<District>(client, d.key, 
        COL(District, d_next_o_id) |
        COL(District, d_tax), 
        d.value, L_WRITE)) {
    return false;
  }

  int o_id = d.value.d_next_o_id;
  d.value.d_next_o_id = d.value.d_next_o_id + 1;

  //LOG_DEBUG("d_next_o_id: %s", d.value.to_string(COL(District, d_next_o_id)).c_str());
  if( !update<District>(client, d.key, COL(District, d_next_o_id), d.value) ) {
    return false;
  }

  Oorder oorder(w_id, d_id, o_id);
  oorder.value.o_c_id = c_id;
  oorder.value.o_carrier_id = 0;
  oorder.value.o_ol_cnt = o_ol_cnt;
  oorder.value.o_all_local = o_all_local;
  oorder.value.o_entry_d = common::local_time();

  if( !insert<Oorder>(client, oorder.key, COL_ALL, oorder.value) ) {
    return false;
  }

  NewOrder neworder(w_id, d_id, o_id);
  neworder.value.no_tmp = 1;
  if( !insert<NewOrder>(client, neworder.key, COL_ALL, neworder.value) ) {
    return false;
  }

  double total_amount = 0, ol_amount = 0;
  for(int i = 1; i <= o_ol_cnt; ++i) {
    int ol_i_id = items[i];
    int ol_supply_w_id = suppliers[i];
    int ol_quantity = quantities[i];

    Item item(ol_i_id);

    if( !read<Item>(client, item.key, 
          COL(Item, i_price) |
          COL(Item, i_name) |
          COL(Item, i_data),
          item.value) ) {
      return false;
    }

    Stock stock(ol_supply_w_id, ol_i_id);
    if( !read<Stock>(client, stock.key, 
          COL(Stock, s_quantity) |
          COL(Stock, s_ytd) |
          COL(Stock, s_order_cnt) |
          COL(Stock, s_remote_cnt) |
          COL(Stock, s_data) |
          COL(Stock, s_dist_01) |
          COL(Stock, s_dist_02) |
          COL(Stock, s_dist_03) |
          COL(Stock, s_dist_04) |
          COL(Stock, s_dist_05) |
          COL(Stock, s_dist_06) |
          COL(Stock, s_dist_07) |
          COL(Stock, s_dist_08) |
          COL(Stock, s_dist_09) |
          COL(Stock, s_dist_10),
          stock.value, L_WRITE) ) {
      return false;
    }

    if( stock.value.s_quantity - ol_quantity >= 10 )
      stock.value.s_quantity -= ol_quantity;
    else 
      stock.value.s_quantity += (91 - ol_quantity);

    if( ol_supply_w_id != w_id )
      stock.value.s_remote_cnt++; 
    
    stock.value.s_ytd += ol_quantity;
    stock.value.s_order_cnt ++;
    
    if( !update<Stock>(client, stock.key, 
          COL(Stock, s_quantity) |
          COL(Stock, s_ytd) | 
          COL(Stock, s_order_cnt) |
          COL(Stock, s_remote_cnt),
          stock.value) ) {
      return false;
    }

    ol_amount = ol_quantity * item.value.i_price;
    total_amount += ol_amount;

    OrderLine orderline(w_id, d_id, o_id, i);
    orderline.value.ol_i_id = ol_i_id;
    orderline.value.ol_delivery_d = 0;
    orderline.value.ol_amount = ol_amount;
    orderline.value.ol_supply_w_id = ol_supply_w_id;
    orderline.value.ol_quantity = ol_quantity;
 
    switch( d_id ) {
      case 1:
        orderline.value.ol_dist_info = stock.value.s_dist_01;
      case 2:
        orderline.value.ol_dist_info = stock.value.s_dist_02;
      case 3:
        orderline.value.ol_dist_info = stock.value.s_dist_03;
      case 4:
        orderline.value.ol_dist_info = stock.value.s_dist_04;
      case 5:
        orderline.value.ol_dist_info = stock.value.s_dist_05;
      case 6:
        orderline.value.ol_dist_info = stock.value.s_dist_06;
      case 7:
        orderline.value.ol_dist_info = stock.value.s_dist_07;
      case 8:
        orderline.value.ol_dist_info = stock.value.s_dist_08;
      case 9:
        orderline.value.ol_dist_info = stock.value.s_dist_09;
      case 10:
        orderline.value.ol_dist_info = stock.value.s_dist_10;
      default:
        orderline.value.ol_dist_info = stock.value.s_dist_01;
    }

    if( !insert<OrderLine>(client, orderline.key, COL_ALL, orderline.value) ) {
      return false;
    }
  }
  total_amount = total_amount * (1 + w.value.w_tax + d.value.d_tax) * (1 - c.value.c_discount);
 
  return true;
}

bool TpccProc::run_miniworkload(TpccClient &client, std::vector<MbRequest > &mb_requsets) {

  //if( trans_id_ != UINT64_MAX ) {
  //  LOG_ERROR("unpexected trans id: %lld", trans_id_);
  //  assert(0);
  //}

  trans_id_ = UINT64_MAX;
  if( 0 != client.start_trans(trans_id_, trans_id_) ) return false;
  //std::set<int32_t> keys_;
  for(int i = 0; i < mb_requsets.size(); i++){
    //if( keys_.find(mb_requsets[i].key) != keys_.end() ) {
    //  assert(0);
    //}
    //keys_.insert(mb_requsets[i].key);
    UserTable u(mb_requsets[i].key);
    if(mb_requsets[i].mode == 0){
      if( !read<UserTable>(client, u.key, COL(UserTable, u_value), u.value, L_READ) ) return false;
      //LOG_INFO("Read UserTable,(%d %d)", u.key, u.value.u_value);
    }
    else{
      if( !read<UserTable>(client, u.key, COL(UserTable, u_value), u.value, L_WRITE) ) return false;
      u.value.u_value += mb_requsets[i].value;
      if( !update<UserTable>(client, u.key, COL(UserTable, u_value), u.value) ) return false;
    }
    //LOG_INFO("run_miniworkload succ(%d %d %d)", mb_requsets[i].key, mb_requsets[i].mode, mb_requsets[i].value);
  }
  return true;
}

void TpccProc::end(TpccClient &client, bool commit) {
  if( trans_id_ == UINT64_MAX )  {}
  else if( !commit ) {
    trans_id_ = UINT64_MAX; 
  }
  else if( batch_buffer_.empty() ) {
    client.end_trans(trans_id_, commit);
  }
  else {
    const string data(batch_buffer_.data(), batch_buffer_.pos());
    client.batch_put(trans_id_, data);
  }
  batch_buffer_.reset();
}
