#include <string.h>
#include <unistd.h>
#include "global.h"
#include "atomic.h"
#include "define.h"
#include "debuglog.h"
#include "clock.h"
#include "encode.h"
using namespace common;

StatManager* StatManager::mgr_ = NULL;

StatManager::StatManager(int64_t mod, bool enable_report) :
  mod_(mod), 
  last_report_time_(0),
  enable_report_(enable_report) {
  switch(mod_) {
    case STAT_CLIENT:
      name_ = ClientStatName;
      break;
    case STAT_SERVER:
      name_ = ServerStatName;
      break;
    default:
      LOG_ERROR("module does not exist");
      break;
  }
  for(int i = 0; i < MAX_TYPE_NUMBER; ++i) {
    value_[i] = 0;
    last_snapshot_[i] = 0;
  }
}

void StatManager::inc(const int64_t mod, const int64_t type, const uint64_t val) {
  assert(mod == mod_);
  assert(type < MAX_TYPE_NUMBER);
  common::atomic_add(&value_[type], val);
}

uint64_t StatManager::get(const int64_t mod, const int64_t type) const {
  assert(mod == mod_);
  assert(type < MAX_TYPE_NUMBER);
  return value_[type];
}

const char* StatManager::type_name(const int64_t type) const {
  assert(type >= 0 && (uint64_t)type < name_.size());
  return name_[type].c_str();
}

void StatManager::report() {
  LOG_INFO("Stat Manager Report Begin");
  uint64_t cur_time = common::local_time();
  uint64_t interval = 1;//last_report_time_ == 0 ? 1 : (cur_time - last_report_time_) / (1000*1000);
  for(uint32_t i = 1; i < name_.size() - 1; ++i) {
    uint64_t val = value_[i];
    //LOG_INFO("%25s:\t%lld", name_[i].c_str(), (val - last_snapshot_[i]) / interval) ;
    LOG_INFO("%25s:\t%lld", name_[i].c_str(), val);
    last_snapshot_[i] = val;
  }
  LOG_INFO("Stat Manager Report End");
  last_report_time_ = cur_time;
}

void StatManager::launch() {
  /*
  pthread_mutex_init(&mutex_, NULL);
  pthread_cond_init(&cond_, NULL);

  pthread_create(&thd_, NULL, thread_func, this);
  */
}
/*
void StatManager::stop() {

  pthread_mutex_lock(&mutex_);
  pthread_cond_signal(&cond_); 
  pthread_mutex_unlock(&mutex_);

  pthread_join(thd_, NULL);
}

bool StatManager::wait_for_stop() {
  uint64_t abs_time = common::local_time() + common::REPORT_INTERVAL;
  struct timespec ts;
  ts.tv_sec = abs_time / 1000000;
  ts.tv_nsec = (abs_time % 1000000) * 1000;
  return 0 == pthread_cond_timedwait(&cond_, &mutex_, &ts);
}
*/
void StatManager::init(const int64_t mod, bool enable_report) {
  if( mgr_ != NULL ) {
    LOG_ERROR("stat manager is inited");
    return;
  }
  mgr_ = new StatManager(mod, enable_report);
  mgr_->launch();
}
/*
void* StatManager::thread_func(void *ptr) {
  StatManager* const mgr = reinterpret_cast<StatManager*>(ptr);
  const bool enable_report = mgr->enable_report_;

  mgr->last_report_time_ = common::local_time();

  pthread_mutex_lock(&mgr->mutex_);

  while( !mgr->wait_for_stop() ) {
    if( enable_report )
      mgr->report();
  }

  pthread_mutex_unlock(&mgr->mutex_);
  mgr->report();
  return NULL;
}
*/

StatManager* StatManager::instance() {
  assert(mgr_ != NULL);
  return mgr_;
}

void StatManager::release() {

  //mgr_->stop();
  if( mgr_ != NULL)
    delete mgr_;
  mgr_  = NULL;
}

int StatManager::read(const char *buffer, const int64_t data_len, int64_t &pos) {
  int ret = SUCCESS;
  int64_t count = 0, val = 0;
  ret = decode<int64_t>(count, buffer, data_len, pos);
  for(int64_t i = 0; i < count && SUCCESS == ret; ++i)  {
    ret = decode<int64_t>(val, buffer, data_len, pos);
    value_[i] = val;
  }
  return ret;
}

int StatManager::write(char *buffer, const int64_t len, int64_t &pos) {
  int ret = SUCCESS;
  int64_t count = name_.size(), val = 0;
  ret = encode<int64_t>(count, buffer, len, pos);
  for(int64_t i = 0; i < count && SUCCESS == ret; ++i) { 
    val = value_[i] - last_snapshot_[i];
    ret = encode<int64_t>(val, buffer, len, pos);
    last_snapshot_[i] = value_[i];
  }
  return ret;
}
