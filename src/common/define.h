#ifndef DEFINE_H
#define DEFINE_H

namespace common {

  enum DmlType {
    DML_INSERT = 0,
    DML_UPDATE,
    DML_REPLACE
  };

  const static int64_t TXN_COMMIT =  0;
  const static int64_t TXN_ABORT  = -1;

  const static int64_t WAREHOUES_NUM = 1;

  const static int64_t REPORT_INTERVAL = 10 * 1000 * 1000;

  //client
  //the mix of different transactions
  const static int PER_NEWORDER = 51; //45;
  const static int PER_PAMENT = 49;   //43;
  const static int PER_STOCK_LEVEL = 4;
  const static int PER_ORDER_STATUS = 4;
  const static int PER_DELIVERY = 4;

  const static char * DUMP_PATH = "./result.txt";

  //server 
  const static char *  SERVER_ADDR = "0.0.0.0:50051";
  const static int64_t IO_THREAD_NUM = 2;
  const static int64_t WORKER_THREAD_NUM = 4;

  const static int64_t NumDistrictsPerWarehouse = 10;
  const static int64_t NumCustomersPerDistrict = 3000;
  const static int64_t NumItem = 100000;

  const static int64_t MAX_BUFFER_SIZE = 4096;
  const static int64_t TRANS_MAP_SIZE = 64 * 1024;
  const static int64_t LOCK_TABLE_SIZE = 1024;

  const static int64_t TRANS_RETRY_TIMES = 128;

  enum StatusCode {
    OK = 0,
    CANCELLED,
    INVALID_ARGUMENT,
    DEADLINE_EXCEEDED,
    NOT_FOUND,
    ALREADY_EXISTS,
    ABORTED,
    LOCK_CONFLICT,
    UNIMPLEMENTED,
    INTERNAL
  };

}

#endif
