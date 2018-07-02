#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "tpcc_client.h"
#include "tpcc_runner.h"
#include "common/config.h"
#include "common/define.h"
#include "common/global.h"
#include "common/debuglog.h"

void usage(const char *bin_name, const char *optarg) {
  fprintf(stderr, "invalid argument: %s\n", optarg);
  fprintf(stderr, "Usage: %s -tsca\n"
      "\t-t run time, default 30 seconds \n"
      "\t-m use micro_bench\n",
      "\t-s warehouse_num, default 2\n"
      "\t-c client_num, default 2\n"
      "\t-a server_addr, default 0.0.0.0:50051\n", 
      "\t-d dump_path\n",
      "\t-l leader/follower\n",
      bin_name);
}

int main(int argc, char **argv) {

  int opt = -1;

  int64_t client_num = 5;
  int64_t run_time = 30; //seconds
  int64_t warehouse_num = 1;
  int64_t ware_start_no = 1, ware_end_no = 1;
  bool leader = false;

  int bench_type = 0;

  int64_t r_cnt = 2;
  int64_t w_cnt = 3;
  int64_t table_size = 1000*1000;
  double theta = 0.87;

  std::string server_addr("0.0.0.0:50001");
  std::string dump_path("tpcc.log");
  StatManager::init(STAT_CLIENT);

  while( -1 != (opt = getopt(argc, argv, "mt:s:p:c:a:d:l")) ) {
    switch( opt ) {
      case 't':
        run_time = atoi(optarg);
        break;
      case 'm':
          bench_type = 1;
        break;
      case 's':
        sscanf(optarg, "%d:%d:%d", &ware_start_no, &ware_end_no, &warehouse_num);
        assert(ware_start_no >= 0);
        assert(ware_start_no <= ware_end_no);
        assert(ware_end_no <= warehouse_num);
        break;
      case 'p':
        sscanf(optarg, "%lld:%lld:%lld:%lf", &r_cnt, &w_cnt, &table_size, &theta);
        assert(r_cnt >= 0);
        assert(w_cnt >= 0);
        assert(table_size >= 0);
        break;
      case 'c':
        client_num = atoi(optarg);
        break;
      case 'a':
        server_addr = optarg;
        break;
      case 'd':
        dump_path = optarg;
        break;
      case 'l':
        leader = true;
        break;
      default:
        usage(argv[0], optarg);
        exit(EXIT_FAILURE);
    }
  }

  if( 0 == bench_type ) {
    //test tpccrunner
    TpccRunner tpcc_runner(server_addr, client_num, ware_start_no,
        ware_end_no, warehouse_num, run_time, leader, dump_path);
    tpcc_runner.launch();
    //tpcc_runner.report();
  }
  else {
    //test mb_runner
    MbRunner mb_runner(server_addr, client_num, r_cnt, w_cnt, table_size, theta,
        run_time, leader, dump_path);
    mb_runner.launch();
  }

  //LOG_INFO("get_latency: %lld", STAT_GET(STAT_CLIENT, CLIENT_GET_TIME) / STAT_GET(STAT_CLIENT, CLIENT_GET_COUNT));
  //LOG_INFO("put_latency: %lld", STAT_GET(STAT_CLIENT, CLIENT_PUT_TIME) / STAT_GET(STAT_CLIENT, CLIENT_PUT_COUNT));
  //LOG_INFO("trans_latency: %lld", STAT_GET(STAT_CLIENT, CLIENT_LATENCY) / STAT_GET(STAT_CLIENT, CLIENT_SUCCESS_COUNT));
  //LOG_INFO("throughput: %lld", STAT_GET(STAT_CLIENT, CLIENT_SUCCESS_COUNT) / run_time);
  int64_t total_succ_trans = STAT_GET(STAT_CLIENT, CLIENT_SUCCESS_COUNT);
  int64_t total_fail_trans = STAT_GET(STAT_CLIENT, CLIENT_FAIL_COUNT);
  int64_t total_latency_trans = STAT_GET(STAT_CLIENT, CLIENT_LATENCY);

  if( total_succ_trans == 0 )  
    total_succ_trans = 1;

  LOG_INFO("latency:       %lld", total_latency_trans / total_succ_trans);
  LOG_INFO("throughput:    %lld", total_succ_trans    / run_time);
  LOG_INFO("failure rate:  %.2lf\%", total_fail_trans * 100.0 / (total_succ_trans + total_fail_trans));

  StatManager::instance()->release();
  return 0;
}
