#include "tpcc_load.h"
#include <vector>
#include <set>
#include <algorithm>
#include <iostream>
#include <assert.h>
#include <stdlib.h>
#include <thread>
using namespace std;

void * TpccLoad::load_func(void *ptr) {
  const LoaderRequest * const r = reinterpret_cast<LoaderRequest*>(ptr);
  TpccLoad load(*(r->store_));
  for(int i = 0; i < r->w_ids_.size(); ++i)
    load.load_warehouse(r->w_ids_[i]);
  return NULL;
}

void TpccLoad::fast_load(int sf, DbStore &db) {
  int loader_num = min(20, sf);
  
  vector<LoaderRequest> requests(loader_num);

  for(int i = 1; i <= sf; ++i) {
    int idx = (i-1) % loader_num;
    requests[idx].w_ids_.push_back(i);
  }

  for(int i = 0; i < requests.size(); ++i)
    requests[i].store_ = &db;

  __sync_synchronize();

  for(int i = 0; i < requests.size(); ++i)
    pthread_create(&requests[i].thd_, NULL, load_func, &requests[i]);

  for(int i = 0; i < requests.size(); ++i)
    pthread_join(requests[i].thd_, NULL);

  TpccLoad item_loader(db);
  item_loader.loadItem();

  LOG_INFO("data is loaded");
}

void TpccLoad::load_warehouse(int w_id) {
  LOG_INFO("loading warehouse %d", w_id);

  Warehouse w(w_id);
  gen_warehouse(w.value);
  /*
  if(w.head!=NULL){
    printf("warehouse has head:%d\n",w.head->modify_time);
  }
  else if(w.tail!=NULL){
    printf("warehouse has tail:%d\n",w.tail->modify_time);
  }
  else{
    printf("warehouse have no haed and tail");
  }*/
  assert(db_.put(w));
  for(int d_id = 1; d_id <= NumDistrictsPerWarehouse; ++d_id) {
    District d(w_id, d_id);
    gen_district(d.value);
    //printf("district tid:%d\n",d.TID);
    assert(db_.put(d));

    //printf("loading cust: %d\n", d_id);
    for(int c_id = 1; c_id <= NumCustomersPerDistrict; ++c_id) {
      Customer c(w_id, d_id, c_id);
      gen_cust(c.value);
      //printf("customer tid:%d\n",c.TID);
      assert(db_.put(c));

      History h(c_id, d_id, w_id, d_id, w_id, common::local_time() / 1000);
      h.value.h_amount = 10;
      h.value.h_data = randomStr(10, 24);
      //printf("history tid:%d\n",h.TID);
      assert(db_.put(h));
    }

    vector<int32_t> c_ids;
    set<int32_t> c_ids_s;
    while( c_ids.size() != NumCustomersPerDistrict ) {
      const auto x = randomInt(1, NumCustomersPerDistrict);
      if( c_ids_s.find(x) != c_ids_s.end() ) 
        continue;
      c_ids_s.insert(x);
      c_ids.emplace_back(x);
    }

    //printf("loading order: %d\n", d_id);
    for(int c = 1; c <= NumCustomersPerDistrict; ++c) {
      Oorder oorder(w_id, d_id, c);

      oorder.value.o_c_id = c_ids[c];
      oorder.value.o_carrier_id = (c < 2101 ? randomInt(1, 10) : 0);
      oorder.value.o_ol_cnt = randomInt(5, 15);
      oorder.value.o_all_local = 1;
      oorder.value.o_entry_d = common::local_time() / 1000;
      //printf("oorder tid:%d\n",oorder.TID);
      assert(db_.put(oorder));

      if( c >= 2101 ) {
        NewOrder norder(w_id, d_id, c);
        norder.value.no_tmp = randomInt(1, 10);
        //printf("neworder tid:%d\n",norder.TID);
        assert(db_.put(norder));
      }

      for(int l = 1; l <= oorder.value.o_ol_cnt; ++l) {
        OrderLine oline(w_id, d_id, c, l);
        OrderLine::Value olvalue = oline.value;

        oline.value.ol_i_id = randomInt(1, NumItem);
        oline.value.ol_delivery_d = (c < 2101 ? oorder.value.o_entry_d : 0);
        oline.value.ol_amount = (c < 2101 ? 0 : (double)(randomInt(1, 999999)/100.0));
        oline.value.ol_supply_w_id = oline.key.ol_w_id;
        oline.value.ol_quantity = 5;
        //olvalue.ol_dist_info = stock[w, ol_i_id].s_dist_infox
        oline.value.ol_dist_info = randomStr(24);
        //printf("orderline tid:%d\n",oline.TID);
        assert(db_.put(oline));
      }
    }
  }

  //printf("loading stock: %d\n", w_id);
  for(int i_id = 1; i_id <= NumItem; ++i_id) {
    Stock st(w_id, i_id);
    gen_stock(st.value);
    assert(db_.put(st));
    //printf("stock tid:%d\n",st.TID);
  }
}

void TpccLoad::loadWarehouse() {
  Warehouse w;
  Warehouse::Key &k = w.key;
  Warehouse::Value &v = w.value;
  for(int i = 1; i <= sf_; ++i) {
    LOG_INFO("warehouse #%d", i);
    k.w_id = i;
    v.w_ytd = 300000;
    v.w_tax = (double) (randomInt(0, 2000) / 10000.0);
    v.w_name = randomStr(6, 10); 
    v.w_street_1 = randomStr(10, 20);
    v.w_street_2 = randomStr(10, 20);
    v.w_city = randomStr(10, 20);
    v.w_state = randomStr(2);
    v.w_zip = "123456789"; 
    db_.put<Warehouse>(w);
  }
  LOG_INFO("load warehouse finished");
}

void TpccLoad::loadDistrict() {
  District d;
  District::Key &k = d.key;
  District::Value &v = d.value;
  for(int w = 1; w <= sf_; ++w) {
    for(int i = 1; i <= NumDistrictsPerWarehouse; ++i) {
      k.d_id = i;
      k.d_w_id = w;

      v.d_ytd = 30000;
      v.d_tax = (double) (randomInt(0, 2000) / 10000.0);
      v.d_next_o_id = 3001;
      v.d_name = randomStr(6, 10);
      v.d_street_1 = randomStr(10, 20);
      v.d_street_2 = randomStr(10, 20);
      v.d_city = randomStr(10, 20);
      v.d_state = randomStr(2);
      v.d_zip = "123456789";

      db_.put<District>(d);
    }
  }
  LOG_INFO("load district finished");
}

void TpccLoad::loadCustomer() {
  Customer c;
  Customer::Key &k = c.key;
  Customer::Value &v = c.value;
  for(int w = 1; w <= sf_; ++w) {
    for(int d = 1; d <= NumDistrictsPerWarehouse; ++d) {
      for(int i = 1; i <= NumCustomersPerDistrict; ++i) {
        k.c_id = i;
        k.c_w_id = w;
        k.c_d_id = d;

        v.c_discount = (double) (randomInt(1, 5000)/ 10000.0);
        v.c_credit = (randomInt(1, 100) <= 10 ? "BC" : "GC");
        v.c_first = randomStr(8, 16);
        v.c_last  = randomStr(8, 16);
        v.c_credit_lim = 50001;

        v.c_balance = -10;
        v.c_ytd_payment = 10;
        v.c_payment_cnt = 1;
        v.c_delivery_cnt = 0;

        v.c_street_1 = randomStr(10, 20);
        v.c_street_2 = randomStr(10, 20);
        v.c_city     = randomStr(2);
        v.c_state    = randomStr(2);
        v.c_zip      = randomStr(4) + "11111";
        v.c_phone    = randomStr(16);
        v.c_since    = common::local_time() / 1000;
        v.c_middle   = "OE";
        v.c_data     = randomStr(1);//randomStr(300, 500);

        db_.put<Customer>(c);

        History h;
        h.key.h_c_id = i;
        h.key.h_c_d_id = d;
        h.key.h_c_w_id = w;
        h.key.h_d_id = d;
        h.key.h_w_id = w;
        h.key.h_date = common::local_time() / 1000;
        h.value.h_amount = 10;
        h.value.h_data = randomStr(10, 24);

        db_.put<History>(h);
      }
      LOG_INFO("load customer[%d %d] finished", w, d);
    }
  }
  LOG_INFO("load customer finished");
}

void TpccLoad::loadItem() {
  //Item item;
  //Item::Key &k = item.key;
  //Item::Value &v = item.value;
  for(int i = 1; i <= NumItem; ++i) {
    Item item(i);
    //k.i_id = i;
    item.value.i_name = randomStr(14, 24);
    item.value.i_price = (double)(randomInt(100, 10000) / 100.0);
    item.value.i_data = randomStr(26, 50);
    item.value.i_im_id = randomInt(1, 10000);
    db_.put<Item>(item);
    //printf("item tid:%d\n",item.TID); 
  }
  LOG_INFO("load item finished");
  //printf("item tid:%d\n",item.TID);
}

void TpccLoad::loadStock() {
  Stock stock;
  Stock::Key &k = stock.key;
  Stock::Value &v = stock.value;
  for(int w = 1; w <= sf_; ++w) {
    for(int i = 1; i <= NumItem; ++i) {
      k.s_i_id = i;
      k.s_w_id = w;

      v.s_quantity = randomInt(10, 100);
      v.s_ytd = 0;
      v.s_order_cnt = 0;
      v.s_remote_cnt = 0;

      v.s_data    = randomStr(26, 50);
      v.s_dist_01 = randomStr(24);
      v.s_dist_02 = randomStr(24);
      v.s_dist_03 = randomStr(24);
      v.s_dist_04 = randomStr(24);
      v.s_dist_05 = randomStr(24);
      v.s_dist_06 = randomStr(24);
      v.s_dist_07 = randomStr(24);
      v.s_dist_08 = randomStr(24);
      v.s_dist_09 = randomStr(24);
      v.s_dist_10 = randomStr(24);
      
      db_.put<Stock>(stock);
    }
    LOG_INFO("load stock [%d] finished", w);
  }
  LOG_INFO("load stock finished");
}

void TpccLoad::loadOrder() {
  Oorder oorder;
  Oorder::Key &k = oorder.key;
  Oorder::Value &v = oorder.value;
  for(int w = 1; w <= sf_; ++w) {
    for(int d = 1; d <= NumDistrictsPerWarehouse; ++d) {
      vector<int32_t> c_ids;
      set<int32_t> c_ids_s;
      while( c_ids.size() != NumCustomersPerDistrict ) {
        const auto x = randomInt(1, NumCustomersPerDistrict);
        if( c_ids_s.find(x) != c_ids_s.end() ) 
          continue;
        c_ids_s.insert(x);
        c_ids.emplace_back(x);
      }
      for(int c = 1; c <= NumCustomersPerDistrict; ++c) {
        //populate oorder
        k.o_w_id = w;
        k.o_d_id = d;
        k.o_id = c;

        v.o_c_id = c_ids[c];

        if( c < 2101 ) 
          v.o_carrier_id = randomInt(1, 10);
        else
          v.o_carrier_id = 0;
        v.o_ol_cnt = randomInt(5, 15);
        v.o_all_local = 1;
        v.o_entry_d = common::local_time() / 1000;

        db_.put<Oorder>(oorder);

        if( c >= 2101 ) {
          //populate neworder
          NewOrder norder;
          NewOrder::Key &nk = norder.key;
          NewOrder::Value &nv = norder.value;
          
          nk.no_w_id = w;
          nk.no_d_id = d;
          nk.no_o_id = c;
          nv.no_tmp = randomInt(1, 10);

          db_.put<NewOrder>(norder);
        }

        //populate order_line
        for(int l = 1; l <= v.o_ol_cnt; ++l) {
          OrderLine oline;
          OrderLine::Key olkey = oline.key;
          OrderLine::Value olvalue = oline.value;

          olkey.ol_w_id = w;
          olkey.ol_d_id = d;
          olkey.ol_o_id = c;
          olkey.ol_number = l;

          olvalue.ol_i_id = randomInt(1, NumItem);
          if( c < 2101 ) {
            olvalue.ol_delivery_d = v.o_entry_d;
            olvalue.ol_amount = 0;
          }
          else {
            olvalue.ol_delivery_d = 0;
            olvalue.ol_amount = (double) (randomInt(1, 999999) / 100.0);
          }

          olvalue.ol_supply_w_id = olkey.ol_w_id;
          olvalue.ol_quantity = 5;
          //olvalue.ol_dist_info = stock[w, ol_i_id].s_dist_infox
          olvalue.ol_dist_info = randomStr(24);
          db_.put<OrderLine>(oline);
        } 
      }
    }
  }
  LOG_INFO("load order finished");
}

string TpccLoad::randomStr(int llen, int ulen) {
  int len = randomInt(llen, ulen);
  std::uniform_int_distribution<> dis(0, 25);
  string ret (len, '\0');
  for(int i = 0; i < len; ++i) 
    ret[i] = 'a' + dis(gen_);
  return ret;
}

int TpccLoad::randomInt(int l, int u) {
  std::uniform_int_distribution<> dis(l, u);
  return dis(gen_);
}

void MbLoad::mb_load(DbStore *db, int start, int end) {
  UserTable u;
  UserTable::Key &k = u.key;
  UserTable::Value &v = u.value;

  for(int i = start; i <= end; ++i) {
    k.u_id = i;
    v.u_value = (i) % 2000;
    db->put<UserTable>(u);
  }
}

void MbLoad::load(){
  UserTable u;
  UserTable::Key &k = u.key;
  UserTable::Value &v = u.value;

  std::vector<std::thread> threads;
  int size = sf_ / 20 + (sf_ % 20 == 0 ? 0 : 1);
  for(int w = 0; w < 20; ++w) {
    int start = size * w + 1;
    int end = size * (w + 1);
    threads.emplace_back(mb_load, &db_, start, end); 
  }
  for(auto iter = threads.begin(); iter != threads.end(); ++iter) {
    iter->join(); 
  }
//  for(int i = 1; i <= sf_; ++i) {
//    k.u_id = i;
//    v.u_value = rand()%2000;
//    db_.put<UserTable>(u);
//    //LOG_INFO("load succ(%d %d)", k.u_id, v.u_value);
//  }
  LOG_INFO("load usertable finished");
}
