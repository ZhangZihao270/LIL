#include "db_server.h"
#include "db_impl.h"
#include "tpcc_load.h"
#include "db_worker.h"
#include "mvcc.h"
#include "common/global.h"
#include "common/config.h"
#include "common/define.h"
#include <pthread.h>
#include <unistd.h>
using namespace common;

#ifdef USE_LOCK_TABLE
typedef CentralizedLockMgr LockMgrImpl;
#elif defined USE_ILOCK
typedef iLockMgr LockMgrImpl;
#else
typedef RowLockMgr LockMgrImpl;
#endif

dbserver::DbStore db;
TransMgr    trans_mgr;
LockMgrImpl lock_mgr;
MVCCMgr mvcc;
void usage(const char *bin_name, const char *optarg) {
  fprintf(stderr, "invalid argument: %s\n", optarg);
  fprintf(stderr, "Usage: %s -iwsa\n"
      "\t-i io_thread_num, default 2 \n"
      "\t-w worker_thread_num, default 2\n"
      "\t-c cpu_core_num, default -1 (use all cpu cores)\n"
      "\t-s warehouse_num, default 10\n"
      "\t-p port, default 50001\n", 
      "\t-m use micro bench\n",
      bin_name);
}

void set_core_num(int io_thread_num, int trans_thread_num, int perfer_trans_core_num, 
    int &io_core_num, int &trans_core_num) {
  int total_core_num = sysconf(_SC_NPROCESSORS_ONLN);
  io_core_num = io_thread_num;
  trans_core_num = min(trans_thread_num, perfer_trans_core_num);
  if( trans_core_num + io_core_num > total_core_num ) {
    LOG_ERROR("cpu cores [%ld] are not enough for trans [%ld] and io [%ld]", total_core_num, trans_core_num, io_core_num);
    exit(1);
  }
}

void set_io_affinity(int io_core_num) {
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  for(int i = 0; i < io_core_num; ++i)
    CPU_SET(i, &cpu_set);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set);
}

void set_trans_affinity(int trans_core_num) {
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set); //初始化
  int total_core_num = sysconf(_SC_NPROCESSORS_ONLN);
  for(int i = total_core_num - trans_core_num; i < total_core_num; ++i)
    CPU_SET(i, &cpu_set);  //将cpu加cpu集合
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set);
}

int main(int argc, char** argv) {

  int opt;

  int64_t cpu_core_num = sysconf(_SC_NPROCESSORS_ONLN);
  int64_t warehouse_num = WAREHOUES_NUM;
  int64_t io_thread_num = IO_THREAD_NUM;
  int64_t worker_thread_num = WORKER_THREAD_NUM;
  int64_t port          = 50001;
  int bench_type = 0; //0 for micro bench, 1 for tpcc

  while( -1 != (opt = getopt(argc, argv, "i:w:c:s:p:m")) ) {
    switch( opt ) {
      case 'i':
        io_thread_num = atoi(optarg);
        break;
      case 'c':
        cpu_core_num = atoi(optarg);
        break;
      case 'w':
        worker_thread_num = atoi(optarg);
        break;
      case 's':
        warehouse_num = atoi(optarg);
        break;
      case 'p':
        port = atoi(optarg);
      case 'm':
          bench_type = 1;
        break;
      default:
        usage(argv[0], optarg);
        exit(EXIT_FAILURE);
    }
  }

  //DEF("server_addr",       server_addr);
  DEF("io_thread_num",     io_thread_num);
  DEF("worker_thread_num", worker_thread_num);
  DEF("warehouse_num",     warehouse_num);

  int trans_core_num, io_core_num;
  set_core_num(io_thread_num, worker_thread_num, cpu_core_num, io_core_num, trans_core_num);

  db.init();

  if( bench_type == 0 ) {

    LOG_INFO("start loading %lld warehosues", warehouse_num);
    TpccLoad::fast_load(warehouse_num, db);
  }
  else {
    int row_number = 1000*10000;
    LOG_INFO("start loading usertable %lld", row_number);
    MbLoad loader(db, row_number);
    loader.load();
  }

  //set affinity here
 
  StatManager::init(STAT_SERVER, true);

  LOG_INFO("rpc   engine use %ld cores", io_core_num);
  LOG_INFO("trans engine use %ld cores", trans_core_num);

  set_trans_affinity(trans_core_num);
  DbWorker::init(&db, &trans_mgr, &lock_mgr, worker_thread_num, cpu_core_num,&mvcc);

  set_io_affinity(io_core_num);
  DbServiceImpl::run(port, io_thread_num);

  StatManager::release();

  return 0;
}
