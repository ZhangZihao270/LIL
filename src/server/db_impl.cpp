//#define LOG_LEVEL LOG_LEVEL_INFO
#include "db_impl.h"
#include <string>
#include <cstdlib>
using namespace std;
using namespace dbserver;

//static int udpthreads = 0;
//static int tcpthreads = 0;
//static int nckthreads = 0;
//static int testthreads = 0;
//static int nlogger = 0;
//static std::vector<int> cores;
//static int port = 2117;
//static void prepare_thread(threadinfo *ti);
//static void* threadfunc(void* x);
//static bool logging = true;
//static bool pinthreads = false;
//static logset* logs;

//bool DbImpl::put(const Str &key, const void *row) {
//  uint64_t value = (uint64_t)(row);
//
//  //Json json[2] = {Json(col), Json(String::make_stable(value.string()))};
//  //q.run_put(mst_->table(), key.string(), &json[0], &json[2], *ti);
//  Json json[2] = {Json(1), Json(to_string(value))};
//  bool ret = q_.run_insert(mst_->table(), key, &json[0], &json[2], *main_ti_);
//  LOG_INFO("put ret code: %d, value: %p", ret, row);
//  return ret;
//}

//bool DbImpl::get(const Str &key, void * &row) {
//  Str val;
//  char *end = NULL;
//  bool ret = q_.run_get1(mst_->table(), key, 1, val, *main_ti_);
//  row = (void *) (strtoull(val.data(), &end, 10));
//  LOG_INFO("get ret code: %d, value %p", ret, row);
//  return ret;
//}

//int DbImpl::get(const string &key, string &value) const {
//  int ret = 0;
//  Str val;
//  quick_istr key(1, 10);
//  q.run_get1(mst_->table(), key.string(), 0, val, *main_ti_);
//  cout << "WDH_TEST:: val=" << val.data() << endl;
//  return ret;
//}

//void DbImpl::init() {
//  udpthreads = tcpthreads = nckthreads = sysconf(_SC_NPROCESSORS_ONLN);
//  mst_ = new Masstree::default_table;
//  main_ti_ = threadinfo::make(threadinfo::TI_MAIN, -1);
//  main_ti_->pthread() = pthread_self();
//  mst_->initialize(*main_ti_);
//  std::cout << mst_->name() << ", " << row_type::name() << std::endl;
//
//  if (udpthreads == 0) {
//    LOG_INFO("0 udp threads\n");
//  }
//  else if (udpthreads == 1) {
//    LOG_INFO("1 udp thread (port %d)\n", port);
//  }
//  else {
//    LOG_INFO("%d udp threads (ports %d-%d)\n", udpthreads, port, port + udpthreads - 1);
//  }
//
//  /*for(int i = 0; i < udpthreads; i++){
//    threadinfo *ti = threadinfo::make(threadinfo::TI_PROCESS, i);
//    ret = pthread_create(&ti->pthread(), 0, threadfunc, ti);
//    cout << "ret = " << ret << "i = " << i << endl;
//    always_assert(ret == 0);
//    }*/
//
//  //add :e
//}

//void * DbImpl::threadfunc(void *x) {
//  threadinfo *ti = reinterpret_cast<threadinfo*>(x);
//  ti->pthread() = pthread_self();
//  prepare_thread(ti);
//
//  query<row_type> q;
//  int col=1;
//  quick_istr key(1, 10), value(2);
//  Json json[2] = {Json(col), Json(String::make_stable(value.string()))};
//  q.run_put(mst_->table(), key.string(), &json[0], &json[2], *ti);
//  Str val;
//  q.run_get1(mst_->table(), key.string(), 0, val, *ti);
//  cout << "WDH_TEST:: val=" << val.data() << endl;
//  return NULL;
//}

//void DbImpl::prepare_thread(threadinfo *ti) {
//#if __linux__
//  if (pinthreads) {
//    cpu_set_t cs;
//    CPU_ZERO(&cs);
//    CPU_SET(cores[ti->index()], &cs);
//    always_assert(sched_setaffinity(0, sizeof(cs), &cs) == 0);
//  }
//#else
//  always_assert(!pinthreads && "pinthreads not supported\n");
//#endif
//  if (logging)
//    ti->set_logger(&logs->log(ti->index() % nlogger));
//}

//bool TpccDb::put(const Warehouse &w) {
//  Warehouse *nw = createWarehouse();
//  *nw = w;
//  Str key((const char *)&nw->key, sizeof(Warehouse::Key));
//  return DbImpl::put(key, (const void *)nw);
//}
//
//bool TpccDb::get(const Warehouse::Key &key, Warehouse *& w) {
//  Str strKey((const char *)&key, sizeof(key));
//  void* p = NULL;
//  bool ret = DbImpl::get(strKey, p);
//  if( ret ) {
//    w = (Warehouse *)(p);
//  }
//  return ret;
//}
