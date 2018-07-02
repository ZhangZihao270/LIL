#ifndef TPCC_LOAD_H
#define TPCC_LOAD_H

#include "db_impl.h"
#include "common/tpcc.h"
#include <random>
#include "common/define.h"

using namespace dbserver;
using namespace tpcc;
using namespace std;

class TpccLoad {
  public:
    TpccLoad(DbStore &db, int sf = 1) : db_(db), sf_(sf), rd_(), gen_(rd_()) {}

    void load()  {
      loadWarehouse();
      loadDistrict();
      loadCustomer();
      loadItem();
      loadStock();
      loadOrder();
    }

    void load_warehouse(int w_id);

    void loadWarehouse();

    void loadDistrict();

    void loadCustomer();

    void loadItem();

    void loadStock();

    void loadOrder();

    static void fast_load(int sf, DbStore &db);

  private:

    struct LoaderRequest {
      pthread_t   thd_;
      vector<int> w_ids_;
      DbStore *store_;
    };

    static void* load_func(void *ptr);

    inline string randomStr(int len) { 
      return randomStr(len, len);
    }

    string randomStr(int llen, int ulen);

    int randomInt(int l, int u);

    void gen_warehouse(Warehouse::Value &v) {
      v.w_ytd = 300000;
      v.w_tax = (double) (randomInt(0, 2000) / 10000.0);
      v.w_name = randomStr(6, 10); 
      v.w_street_1 = randomStr(10, 20);
      v.w_street_2 = randomStr(10, 20);
      v.w_city = randomStr(10, 20);
      v.w_state = randomStr(2);
      v.w_zip = "123456789"; 
    }

    void gen_district(District::Value &v) {
      v.d_ytd = 30000;
      v.d_tax = (double) (randomInt(0, 2000) / 10000.0);
      v.d_next_o_id = 3001;
      v.d_name = randomStr(6, 10);
      v.d_street_1 = randomStr(10, 20);
      v.d_street_2 = randomStr(10, 20);
      v.d_city = randomStr(10, 20);
      v.d_state = randomStr(2);
      v.d_zip = "123456789";
    }

    void gen_cust(Customer::Value &v) {
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
    }
    
    void gen_stock(Stock::Value &v) {
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
    }

  private:
    DbStore &db_;
    int sf_;

    std::random_device rd_;
    std::mt19937 gen_;

  public:
//    const static int NumDistrictsPerWarehouse = 10;
//    const static int NumCustomersPerDistrict = 3000;
//    const static int NumItem = 100000;
};

//microbenchmark
class MbLoad{
public:
  MbLoad(DbStore &db, int sf = 1) : db_(db), sf_(sf) {}

  static void mb_load(DbStore *db, int start, int end);
  void load();

private:
  DbStore &db_;
  int sf_;//row number
};

#endif
