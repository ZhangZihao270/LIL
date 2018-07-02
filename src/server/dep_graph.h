#ifndef DEP_GRAPH_H_
#define DEP_GRAPH_H_

#include <stdint.h>
#include <pthread.h>
#include <vector>
#include <set>
// The denpendency information per thread
using std::vector;
using std::set;

struct DepThd {
    vector<int64_t> adj;    // Pointer to an array containing adjacency lists
    pthread_mutex_t lock;
    volatile int64_t txnid;         // -1 means invalid
};

class DepGraph {
public:
    void init(int64_t thread_num);
    // return values:
    //  0: no deadlocks
    //  1: deadlock exists
    int detect_cycle(int64_t txnid);
    // txn1 (txn_id) dependes on txns (containing cnt txns)
    // return values:
    //  0: succeed.
    //  16: cannot get lock
    int add_dep(int64_t txnid, const vector<int64_t>& txnids);
    // remove all outbound dependencies for txnid.
    // will wait for the lock until acquired.
    void delete_dep(int64_t txnid);
private:
    int V;    // No. of vertices
    DepThd * dependency;
    bool isCyclic(int64_t txnid, bool * detect_data); // return if "thd" is causing a cycle
    inline int get_thdid_from_txnid(int64_t txnid) {
        return txnid % V;
    }
};
#endif // DEADLOCK_DETECT_H
