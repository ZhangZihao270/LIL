#ifndef TPC_NEW_H_
#define TPC_NEW_H_

#ifndef __linux__
#define PACKED __attribute__((packed))
#else
#define PACKED
#endif

#include <stdint.h>
#include <iostream>
#include <string.h>
#include <string>
#include "t_str.h"
#include "encode.h"
using namespace std;

template <typename T> 
inline std::string as_str(const T &value) {
  return value.to_string();
}

#define AS_STR(TYPE) \
  template<> \
  inline std::string as_str(const TYPE &value) { \
    return std::to_string(value); \
  }

AS_STR(int64_t);
AS_STR(int32_t);
AS_STR(int16_t);
AS_STR(int8_t);
AS_STR(float);
AS_STR(double);
AS_STR(bool);
AS_STR(char);

#define add_col(x, t, y) (x = x | (1 << ( t :: C_ ## y)))

#define has_col(x, y) (0 != ((x) & (1 << (y))))

#define WRITE_KEY(TYPE, NAME) \
  encode<TYPE>(NAME, buf, data_len, pos); 

#define READ_KEY(TYPE, NAME) \
  decode<TYPE>(NAME, buf, data_len, pos); 

#define BODY(TYPE, NAME) \
  TYPE NAME;

#define PARAMETER_FIRST(TYPE, NAME) \
  const TYPE in_ ## NAME

#define PARAMETER(TYPE, NAME) \
  , const TYPE in_ ## NAME

#define PP_FIRST(TYPE, NAME) \
 in_ ## NAME

#define PP(TYPE, NAME) \
  , in_ ## NAME

#define INIT_LIST_FIRST(TYPE, NAME) \
  NAME( in_ ## NAME )

#define INIT_LIST(TYPE, NAME) \
  , NAME( in_ ## NAME )

#define COL(TNAME, CNAME) \
  (1 << TNAME :: C_ ## CNAME)

#define COL_ALL -1

#define FIRST(TYPE, NAME) \
  C_ ## NAME = 0, 

#define NEXT(TYPE, NAME) \
  C_ ## NAME, 

#define WRITE_VALUE(TYPE, NAME) \
  if( has_col(cols, (C_ ## NAME)) ) \
    encode<TYPE>(NAME, buf, data_len, pos); 

#define READ_VALUE(TYPE, NAME) \
  if( has_col(cols, (C_ ## NAME)) ) \
    decode<TYPE>(NAME, buf, data_len, pos);

#define APPLY_VALUE(TYPE, NAME) \
  if( has_col(cols, (C_ ## NAME)) ) \
    NAME = other.NAME;

#define APPLY(members, expandMacros) \
  members(expandMacros, expandMacros) 

#define APPLY2(members, expandFirst, expandNext) \
  members(expandFirst, expandNext)

#define FILTER_OUTPUT1(TYPE, NAME) \
  if( has_col(cols, (C_ ## NAME)) ) \
    str.append(as_str(NAME));

#define FILTER_OUTPUT2(TYPE, NAME) \
  str.append(", "); \
  if( has_col(cols, (C_ ## NAME)) ) \
    str.append(as_str(NAME));

#define OUTPUT1(TYPE, NAME) \
  str.append(as_str(NAME));

#define OUTPUT2(TYPE, NAME) \
  str.append(", "); \
  str.append(as_str(NAME)); 

#define DO_STRUCT(tablename, keyfields, valuefields)\
  struct tablename {\
      struct Key { \
        Key() {} \
        Key(APPLY2(keyfields, PARAMETER_FIRST, PARAMETER)) :  \
          APPLY2(keyfields, INIT_LIST_FIRST, INIT_LIST) {} \
        APPLY(keyfields, BODY) \
        void read(const char *buf, const int64_t data_len, int64_t &pos) { \
          APPLY(keyfields, READ_KEY) \
        } \
        void write(char *buf, const int64_t data_len, int64_t &pos) const { \
          APPLY(keyfields, WRITE_KEY) \
        } \
        std::string to_string() { \
          std::string str = "[ "; \
          APPLY2(keyfields, OUTPUT1, OUTPUT2) \
          str.append(" ]"); \
          return str; \
        } \
      } PACKED; \
      struct Value { \
        APPLY(valuefields, BODY)  \
        void read(const char *buf, const int64_t data_len, int64_t &pos, uint64_t cols = -1) { \
          APPLY(valuefields, READ_VALUE) \
        } \
        void write(char *buf, const int64_t data_len, int64_t &pos, uint64_t cols = -1) const { \
          APPLY(valuefields, WRITE_VALUE) \
        } \
        void apply(const Value &other, uint64_t cols = -1)  { \
          APPLY(valuefields, APPLY_VALUE) \
        } \
        std::string to_string() { \
          std::string str = "[ "; \
          APPLY2(valuefields, OUTPUT1, OUTPUT2) \
          str.append(" ]"); \
          return str; \
        } \
        std::string to_string(uint64_t cols) { \
          std::string str = "[ "; \
          APPLY2(valuefields, FILTER_OUTPUT1, FILTER_OUTPUT2) \
          str.append(" ]"); \
          return str; \
        } \
        void set_modify_time(uint64_t time){\
            modify_time=time;\
        }\
        Value *next;\
        uint64_t modify_time;\
      } PACKED; \
    enum { \
      APPLY2(valuefields, FIRST, NEXT) \
      C_end \
    }; \
    tablename() : header(TID),head(NULL),tail(NULL),count(0) {} \
    tablename(APPLY2(keyfields, PARAMETER_FIRST, PARAMETER)) : header(TID), \
        key(APPLY2(keyfields, PP_FIRST, PP)),head(NULL),tail(NULL),count(0){} \
    std::string to_string() { \
      std::string ret = #tablename; \
      ret.append(key.to_string()); \
      ret.append(" : "); \
      ret.append(value.to_string()); \
      return ret; \
    } \
    std::string to_string(uint64_t cols) { \
      std::string ret = #tablename; \
      ret.append(key.to_string()); \
      ret.append(" : "); \
      ret.append(value.to_string(cols)); \
      return ret; \
    } \
    bool is_del() const { \
      return 1 == header.del_flag_; \
    } \
    void set_del() { \
      header.del_flag_ = 1; \
    } \
    Header header;\
    Key    key;\
    Value value;\
    int count;\
    Value *head;\
    Value *tail;\
    const  static int64_t TID = TID_ ## tablename ; \
  } PACKED; 

#define ENABLE_USE_ILOCK
namespace tpcc {
struct Header {
  volatile uint64_t lock_; //row lock, lock table, ilock
#ifdef ENABLE_USE_ILOCK
  volatile uint64_t wr_wait_bits_;
  volatile uint64_t rd_wait_bits_; 
#endif
  uint64_t table_id_;
  int8_t   del_flag_;

  Header(uint64_t tid) : 
    lock_(0), 
#ifdef ENABLE_USE_ILOCK
    wr_wait_bits_(0),
    rd_wait_bits_(0),
#endif
    table_id_(tid), 
    del_flag_(0) {}
} PACKED;


#define WAREHOUSE_KEY_FIELDS(x, y)  \
    x(int32_t, w_id) 

#define WAREHOUSE_VALUE_FIELDS(x, y) \
    x(double, w_ytd) \
    y(double, w_tax) \
    y(t_str<10>, w_name) \
    y(t_str<20>, w_street_1) \
    y(t_str<20>, w_street_2) \
    y(t_str<20>, w_city) \
    y(t_str<2>, w_state) \
    y(t_str<9>, w_zip) 

//DO_STRUCT(Warehouse, WAREHOUES_KEY_FIELDS, WAREHOUSE_VALUE_FIELDS)

#define DISTRICT_KEY_FIELDS(x, y) \
  x(int32_t, d_w_id) \
  y(int32_t, d_id) 

#define DISTRICT_VALUE_FIELDS(x, y) \
  x(double, d_ytd) \
  y(double, d_tax) \
  y(int32_t, d_next_o_id) \
  y(t_str<10>,  d_name) \
  y(t_str<20>,  d_street_1) \
  y(t_str<20>,  d_street_2) \
  y(t_str<20>,  d_city) \
  y(t_str<2>,  d_state) \
  y(t_str<9>,  d_zip) 

//DO_STRUCT(District, DISTRICT_KEY_FIELDS, DISTRICT_VALUE_FIELDS)

#define CUSTOMER_KEY_FIELDS(x, y) \
  x(int32_t, c_w_id) \
  y(int32_t, c_d_id) \
  y(int32_t, c_id) 

#define CUSTOMER_VALUE_FIELDS(x, y) \
      x(double ,  c_discount) \
      y(t_str<4>  ,  c_credit) \
      y(t_str<16>  ,  c_first) \
      y(t_str<16>  ,  c_last) \
      y(double ,  c_credit_lim) \
      y(double ,  c_balance) \
      y(double ,  c_ytd_payment) \
      y(int32_t,  c_payment_cnt) \
      y(int32_t,  c_delivery_cnt) \
      y(t_str<20>  ,  c_street_1) \
      y(t_str<20>  ,  c_street_2) \
      y(t_str<20>  ,  c_city) \
      y(t_str<2>  ,  c_state) \
      y(t_str<9>  ,  c_zip) \
      y(t_str<16>  ,  c_phone) \
      y(int64_t,  c_since) \
      y(t_str<2>  ,  c_middle) \
      y(t_str<500>  ,  c_data) 

//DO_STRUCT(Customer, CUSTOMER_KEY_FIELDS, CUSTOMER_VALUE_FIELDS)

#define ITEM_KEY_FIELDS(x, y) \
  x(int32_t, i_id) \

#define ITEM_VALUE_FIELDS(x, y) \
  x(t_str<24>, i_name) \
  y(double, i_price) \
  y(t_str<50>, i_data) \
  y(int32_t, i_im_id) 

//DO_STRUCT(Item, ITEM_KEY_FIELDS, ITEM_VALUE_FIELDS)

#define STOCK_KEY_FIELDS(x, y) \
  x(int32_t, s_w_id) \
  y(int32_t, s_i_id) 

#define STOCK_VALUE_FIELDS(x, y) \
  x(int16_t, s_quantity) \
  y(double,  s_ytd) \
  y(int32_t, s_order_cnt) \
  y(int32_t, s_remote_cnt) \
  y(t_str<50>, s_data) \
  y(t_str<24>, s_dist_01) \
  y(t_str<24>, s_dist_02) \
  y(t_str<24>, s_dist_03) \
  y(t_str<24>, s_dist_04) \
  y(t_str<24>, s_dist_05) \
  y(t_str<24>, s_dist_06) \
  y(t_str<24>, s_dist_07) \
  y(t_str<24>, s_dist_08) \
  y(t_str<24>, s_dist_09) \
  y(t_str<24>, s_dist_10) 

//DO_STRUCT(Stock, STOCK_KEY_FIELDS, STOCK_VALUE_FIELDS)

#define NEW_ORDER_KEY_FIELDS(x, y) \
  x(int32_t, no_w_id) \
  y(int32_t, no_d_id) \
  y(int32_t, no_o_id) 

#define NEW_ORDER_VALUE_FIELDS(x, y) \
  x(int32_t, no_tmp) 

//DO_STRUCT(NewOrder, NEW_ORDER_KEY_FIELDS, NEW_ORDER_VALUE_FIELDS)

#define OORDER_KEY_FIELDS(x, y) \
  x(int32_t, o_w_id) \
  y(int32_t, o_d_id) \
  y(int32_t, o_id) 

#define OORDER_VALUE_FIELDS(x, y) \
  x(int32_t, o_c_id) \
  y(int32_t, o_carrier_id) \
  y(int8_t, o_ol_cnt) \
  y(bool, o_all_local) \
  y(int64_t, o_entry_d) 

//DO_STRUCT(Oorder, OORDER_KEY_FIELDS, OORDER_VALUE_FIELDS)

#define ORDER_LINE_KEY_FIELDS(x, y) \
  x(int32_t, ol_w_id) \
  y(int32_t, ol_d_id) \
  y(int32_t, ol_o_id) \
  y(int32_t, ol_number)

#define ORDER_LINE_VALUE_FIELDS(x, y) \
  x(int32_t, ol_i_id) \
  y(int64_t, ol_delivery_d) \
  y(double,  ol_amount) \
  y(int32_t, ol_supply_w_id) \
  y(int8_t,  ol_quantity) \
  y(t_str<24>, ol_dist_info) 

//DO_STRUCT(OrderLine, ORDER_LINE_KEY_FIELDS, ORDER_LINE_VALUE_FIELDS)

#define HISTORY_KEY_FIELDS(x, y) \
  x(int32_t, h_c_id) \
  y(int32_t, h_c_d_id) \
  y(int32_t, h_c_w_id) \
  y(int32_t, h_d_id) \
  y(int32_t, h_w_id) \
  y(int64_t, h_date) 
 
#define HISTORY_VALUE_FIELDS(x, y) \
  x(double, h_amount) \
  y(t_str<24>, h_data) 


//DO_STRUCT(History, HISTORY_KEY_FIELDS, HISTORY_VALUE_FIELDS)

//usertable is used for Microbenchmark
#define USERTABLE_KEY_FIELDS(x, y)  \
    x(int32_t, u_id)

#define USERTABLE_VALUE_FIELDS(x, y) \
    x(int32_t, u_value)

#define CREATE_TABLE(name, schema) \
  DO_STRUCT(name, schema ## _KEY_FIELDS, schema ## _VALUE_FIELDS) \

#define TABLE(name) \
  TID_ ## name

#define REGISTER_TABLE(name, schema) \
  TID_ ## name,

#define REGISTER_ALL(list) \
  enum TableID{ \
    TID_START = 16, \
    list(REGISTER_TABLE) \
    TID_END \
  }; \

#define CREATE_ALL(list)  \
  list(CREATE_TABLE) 

#define TABLE_LIST(x) \
  x(Warehouse, WAREHOUSE) \
  x(District, DISTRICT) \
  x(Customer, CUSTOMER) \
  x(Item, ITEM) \
  x(Stock, STOCK) \
  x(History, HISTORY) \
  x(Oorder, OORDER) \
  x(OrderLine, ORDER_LINE) \
  x(NewOrder, NEW_ORDER) \
  x(UserTable, USERTABLE)

REGISTER_ALL(TABLE_LIST)

CREATE_ALL(TABLE_LIST)

inline const string table_name(uint64_t table_id) {
#define EXPAND_TABLE_NAME(name, schema) \
  case TABLE(name): \
      return #name;

  switch(table_id) {
    TABLE_LIST(EXPAND_TABLE_NAME)
  }
#undef EXPAND_TABLE_NAME
}

template <typename R>
const string row_info(R &r) {
  string ret = table_name(R::TID);
  ret.push_back(' ');
  ret.append(r.key.to_string());
  return ret;
}

inline const string dump_row_info(Header *h) {
#define DUMP_ROW_KEY(name, schema) \
  case TABLE(name): \
    return row_info(reinterpret_cast<name &>(*h));

  switch(h->table_id_) {
    TABLE_LIST(DUMP_ROW_KEY)
  }
#undef DUMP_ROW_KEY
}

}
#endif
