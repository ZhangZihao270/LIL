#include "dep_graph.h"
#include <assert.h>
#include <string.h>
#include "common/define.h"
#include "common/log.h"
using namespace common;

void DepGraph::init(int64_t thread_num) {
  assert(thread_num > 0);
  dependency = new DepThd[thread_num];

  for(int64_t i = 0; i < thread_num; ++i) {
    dependency[i].adj.clear();
    dependency[i].lock = PTHREAD_MUTEX_INITIALIZER;
    dependency[i].txnid = -1;
  }

  V = thread_num;
}

int DepGraph::add_dep(int64_t txnid, const vector<int64_t> &txnids){
  int thd1 = get_thdid_from_txnid(txnid);
  int cnt = txnids.size();
  pthread_mutex_lock( &dependency[thd1].lock );
  dependency[thd1].txnid = txnid;

  for (int i = 0; i < cnt; i++){
    dependency[thd1].adj.push_back(txnids[i]);
  }

  pthread_mutex_unlock( &dependency[thd1].lock );
  return 0;
}

void DepGraph::delete_dep(int64_t txnid){
  int thd1 = get_thdid_from_txnid(txnid);
  pthread_mutex_lock( &dependency[thd1].lock );

  dependency[thd1].adj.clear();
  dependency[thd1].txnid = -1;

  pthread_mutex_unlock( &dependency[thd1].lock );
}

bool DepGraph::isCyclic(int64_t txnid, bool *visited){
    int thd = get_thdid_from_txnid(txnid);
    pthread_mutex_lock( &dependency[thd].lock );

    int txnid_num = dependency[thd].adj.size();
    int64_t txnids[txnid_num];
    int n = 0;

    if (dependency[thd].txnid != txnid) {
        pthread_mutex_unlock( &dependency[thd].lock );
        return false;
    }
    for(vector<int64_t>::iterator i = dependency[thd].adj.begin();
        i != dependency[thd].adj.end(); ++i) {
        txnids[n++] = *i;
    }
    pthread_mutex_unlock( &dependency[thd].lock );

    if( visited[thd] ) return true;
    visited[thd] = true;

    for (int i = 0; i < n; i++){
      int nextthd = get_thdid_from_txnid(txnids[i]);

      if(visited[nextthd] && dependency[nextthd].txnid == nextthd){
        return true;
      }
      LOG_DEBUG("visit from %lld [%lld] to %lld [%lld]", txnid, thd, txnids[i], get_thdid_from_txnid(txnids[i]));
      if (!visited[nextthd] && dependency[nextthd].txnid == nextthd && isCyclic(txnids[i], visited)) {
        return true;
      }
    }
    visited[thd] = false;
    return false;
}

int DepGraph::detect_cycle(int64_t txnid){
  bool deadlock = false;
  bool visited[V];
  memset(visited, false, sizeof(visited));
  if (isCyclic(txnid, visited)){
    deadlock = true;
  }
  return deadlock;
}
