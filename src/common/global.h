#ifndef GLOBAL_H
#define GLOBAL_H
#include <stdint.h>
#include "gperftools/tcmalloc.h"
#include <pthread.h>
#include "atomic.h"
#include <assert.h>
#include <vector>
#include <string>

using namespace std;

class MemAlloc {
public:
  MemAlloc() {}
  ~MemAlloc() {}

public:
  void * tcmalloc(uint64_t size){return tc_malloc(size);}
  void tcfree(void * block) {tc_free(block);}
};

enum Lock_type {
  L_NONE,
  L_WRITE,
  L_READ
};

enum RC {
  RC_OK,
  RC_WAIT,
  RC_ERROR,
  RC_FINISH,
  RC_TIMEOUT,
  RC_ABORT,
  RC_FIND
};

enum {
  NEWORDER = 0,
  PAYMENT = 1,
  STOCK_LEVEL = 2,
  ORDER_STATUS = 3,
  DELIVERY = 4,
  MINIWORKLOAD = 5
};

enum StatModule {
  STAT_CLIENT = 0,
  STAT_SERVER = 1,
  MAX_MOD_NUMBER = 2
};

#define WAIT_GRAPH 0
#define WAIT_DIE 1

#define DETECT_MOD WAIT_GRAPH

#define X_CLIENT_STAT(x) \
  x(CLIENT_FAIL_COUNT), \
  x(CLIENT_SUCCESS_COUNT), \
  x(CLIENT_LATENCY), \
  x(CLIENT_GET_COUNT), \
  x(CLIENT_GET_TIME), \
  x(CLIENT_PUT_COUNT), \
  x(CLIENT_PUT_TIME)

#define X_SERVER_STAT(x) \
  x(COMMIT_TRANS_COUNT), \
  x(ABORT_TRANS_COUNT), \
  x(ACTIVE_TRANS_COUNT), \
  x(LOCK_SUCCESS_COUNT), \
  x(LOCK_FAIL_COUNT), \
  x(LOCK_WAIT_TIME), \
  x(START_QTIME), \
  x(START_TIME), \
  x(START_COUNT), \
  x(PUT_QTIME), \
  x(PUT_TIME), \
  x(PUT_COUNT), \
  x(GET_QTIME), \
  x(GET_TIME), \
  x(GET_COUNT), \
  x(END_QTIME), \
  x(END_TIME), \
  x(END_COUNT), \
  x(TRANS_COUNT)
  

#define ENUM_STAT_TYEP(name) \
  name 

enum ClientStatType {
  CLIENT_STAT_BEGIN = 0,
  X_CLIENT_STAT(ENUM_STAT_TYEP),
  CLIENT_STAT_END
};

enum ServerStatType {
  SERVER_STAT_BEGIN = 0,
  X_SERVER_STAT(ENUM_STAT_TYEP),
  SERVER_STAT_END
};

#define NAME_STAT_TYPE(name) \
   #name

#define ClientStatName \
  { \
    "client_stat_begin", \
    X_CLIENT_STAT(NAME_STAT_TYPE), \
    "client_stat_end" \
  }

#define ServerStatName \
  { \
    "server_stat_begin", \
    X_SERVER_STAT(NAME_STAT_TYPE), \
    "server_stat_end" \
  }

const int64_t MAX_TYPE_NUMBER = 128;

class StatManager {
public:
  StatManager(int64_t mod, bool enable_report = false);


  ~StatManager() {}

  void inc(const int64_t mod, const int64_t type, const uint64_t val);

  uint64_t get(const int64_t mod, const int64_t type) const;

  const char* type_name(const int64_t type) const;

  void report();

  int read(const char *buffer, const int64_t data_len, int64_t &pos);

  int write(char *buffer, const int64_t len, int64_t &pos);

  static StatManager* instance();
  static void init(const int64_t mod, bool enable_report = false);
  static void release();

private:
 // static void *thread_func(void *ptr);
  void launch();
 // void stop();

 // bool wait_for_stop();

private:
  volatile uint64_t value_[MAX_TYPE_NUMBER];
  const int64_t  mod_;
  vector<string> name_;

  uint64_t last_snapshot_[MAX_TYPE_NUMBER];
  uint64_t last_report_time_;
  bool enable_report_;

//  pthread_t       thd_;
//  pthread_cond_t  cond_;
//  pthread_mutex_t mutex_;
  static StatManager *mgr_;
};

#define STAT_INC(MOD, TYPE, VAL) \
  StatManager::instance()->inc((MOD), (TYPE), (VAL))

#define STAT_GET(MOD, TYPE) \
  StatManager::instance()->get((MOD), (TYPE))

#endif // GLOBAL_H
