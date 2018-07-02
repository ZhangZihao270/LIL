#include "mvcc.h"
#include "trans_mgr.h"
#include "db_impl.h"
#include <unistd.h>
#include <sched.h>
using namespace dbserver;

/*
template<typename T>
RC MVCCMgr::findReadVersion(TransCtx *ctx, T *name,typename T::Value &value){
    if(name->head==NULL){
        value=name->value;
        return RC_FIND;
    }
    uint64_t trans_time=ctx->trans_id_;
    typename T::Value *head=NULL;
    head=name->head;
    //需加入是否从uclist中读的判断
    uint64_t lock_state=name->header.lock_;
    uint64_t cur_trans=lock_state & 0x7fffffff;
    //std::pair<T::header *,Lock_type> wr(&(name->header),L_WRITE);
    if(cur_trans==ctx->trans_id_)
        value=name->value;
    //if(trans_time==name->Value.modify_time)
        //value=name->Value;
    else if(trans_time>=head->modify_time){
        if(trans_time>name->tail->modify_time)
            value=*(name->tail);//??
        else{
            typename T::Value *next=head->next;
            while(trans_time>next->modify_time){
                head=next;
                next=head->next;
            }
            value=*head;
        }
    }
    else
        return RC_ABORT;
    return RC_FIND;
}*/
/*
template<typename T>
bool MVCCMgr::isWriteable(TransCtx *ctx, T *row){
    if(row->tail==NULL)
        return true;
    else{
        if(row->tail->modify_time>ctx->trans_id_)
            return false;
        else
            return true;
    }
}*/


//template<typename T>
void MVCCMgr::commit(TransCtx *ctx,int64_t table_id,void *name){
 // LOG_INFO("commit:%lld",table_id);
#define COMMIT_TRANS(T,schema)\
case TID_ ## T : \
  {\
    T *row=reinterpret_cast<T *>(name);\
    T::Value *value=new T::Value;\
    *value=row->value;\
    value->modify_time=ctx->trans_id_;\
    if(row->head==NULL||row->tail==NULL){\
        row->head=value;\
        row->tail=value;\
        row->count=1;\
    }\
    if(row->tail->modify_time!=ctx->trans_id_){\
        row->tail->next=value;\
        row->tail=value;\
        row->count++;\
    }\
    if(row->count>10){\
        T::Value *node,*pre;\
        node=row->head;\
        for(int i=1;i<=5;i++){\
            pre=node;\
            node=node->next;\
            row->head=node;\
            delete pre;\
        }\
        row->count=row->count-5;\
    }\
}\
break;
switch(table_id){
    TABLE_LIST(COMMIT_TRANS)
}
#undef COMMIT_TRANS
//LOG_INFO("commit finished\n");
}

void MVCCMgr::init(int64_t thread_num) {
    thread_num_ = thread_num;
 }

void MVCCMgr::end(TransCtx *ctx){
  //LOG_INFO("start commit:%lld",ctx->trans_id_);
  //LOG_INFO("write operation count:%d",ctx->wr_record.size());
  
  for(int i=0;i<ctx->wr_record.size();i++){
    commit(ctx,ctx->wr_record[i].first,ctx->wr_record[i].second);
  }
}
