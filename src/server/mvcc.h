#ifndef MVCC_H
#define MVCC_H

#include "../common/tpcc.h"
#include "trans_mgr.h"
#include "../common/global.h"
using namespace tpcc;
//struct TransCtx;
//namespace dbserver{
class MVCCMgr{
public:
    MVCCMgr(){
      thread_num_=0;
    }

    void init(int64_t thread_num);

   // template<typename T>
   // RC findReadVersion(TransCtx *ctx,T *name,typename T::Value &value);

   // template<typename T>
   // bool isWriteable(TransCtx *ctx,T *row);

    void end(TransCtx *ctx);

    void commit(TransCtx *ctx,int64_t table_id,void *name);

    template<typename T>
    inline  RC findReadVersion(TransCtx *ctx,T *name,typename T::Value &value){
      typename T::Value *head=NULL;
      typename T::Value *tail=NULL;
      head=name->head;
      tail=name->tail;
      //LOG_INFO("read opeartion:%lld",ctx->trans_id_);
      //LOG_INFO("read table:%lld",name->TID);
      //LOG_INFO("row count:%d",name->count);
      if(head==NULL||tail==NULL){
        //LOG_INFO("have no history version");
       value=name->value;
       return RC_FIND;
      }
     uint64_t trans_time=ctx->trans_id_;
     //LOG_INFO("head:");
     //LOG_INFO("head:%llu",head->modify_time);
     //LOG_INFO("tail:%llu",tail->modify_time);
     //LOG_INFO("value: %s",name->value.to_string());
     //LOG_INFO("trans_id: %lld",ctx->trans_id_);
     uint64_t lock_state=name->header.lock_;
     uint64_t cur_trans=lock_state & 0x7fffffff;
     //LOG_INFO("lock_state:%lld",lock_state);
     //LOG_INFO("cur_trans: %llu",cur_trans);
     //LOG_INFO("trans_time:%llu",trans_time);
     if(cur_trans==trans_time)
       value=name->value;
     else if(trans_time>head->modify_time){
       if(trans_time>tail->modify_time){
         value=*(name->tail);
         //LOG_INFO("read the last:%d",value.modify_time);
        }
       else{
         typename T::Value *next=head->next;
         while(trans_time>next->modify_time){
           head=next;
           next=head->next;
         }
         value=*head;
         //LOG_INFO("head:%llu",name->head->modify_time);
         //LOG_INFO("tail:%llu",name->tail->modify_time);
         //LOG_INFO("trans:%llu",ctx->trans_id_);
         //LOG_INFO("read version time:%d",value.modify_time);
      }
    }
    else{
       return RC_ABORT;
       LOG_INFO("can not find read version");
    }
    return RC_FIND;
  }

  template<typename T>
  inline bool isWriteable(TransCtx *ctx,T *row){
    //LOG_INFO("write opeartion:%lld",ctx->trans_id_);
    //LOG_INFO("write table:%d",row->TID);
    //LOG_INFO("write row count:%d",row->count);
    //printf("tail time:%d\n",row->tail->modify_time);
    if(row->tail==NULL)
      return true;
    else{
      //printf("tail time:%d\n",row->tail->modify_time);
      if(row->tail->modify_time>ctx->trans_id_)
        return false;
      else
        return true;
    }
  }

private:  
    int64_t thread_num_;
};
/*
template<typename T>
void MVCCMgr::releaseOldNode(T::Value *head){
    T::Value *node,*pre;
    node=head;
    for(int i=1;i<=5;i++){
        pre=node;
        node=node->next;
        delete pre;
    }
    head=node;
}
*/
/*
template<typename T>
void MVCCMgr::releaseOldNode(T::Value *head){
    T::Value *node,*pre;
    node=head;
    for(int i=1;i<=5;i++){
        pre=node;
        node=node->next;
        delete pre;
    }
    head=node;
}*/

/*
int findReadVersion(TransCtx *ctx,Header *header,Value *value){//函数返回值类型需要明确
    uint64_t trans_time=ctx->trans_id_;
    CellInfoNode node=value.head;
    if(value->uc_info!=NULL&&trans_time==value->uc_info->trans_id)
        cell=value->uc_info->tail;
    if(trans_time>node.modify_time){
        findVersionInList(node,value->tail,trans_time,cell);
    }
    else{
        UndoNode undoNode=value.undo_list;
        while (trans_time<undoNode.head->modify_time&&undoNode!=NULL) {
            undoNode=undoNode.next;
            if(undoNode==NULL)
                return abort;
        }
        findVersionInList(undoNode.head,undoNode.tail,trans_time,cell);
    }
    return find;  //枚举类型需定义
}
*/

/*
void findVersionInList(CellInfoNode *node,CellInfoNode *tail,uint64_t trans_time,CellInfoNode &cell){
    if(trans_time>tail->modify_time)
        cell=tail;
    CellInfoNode *next=node->next;
    while(trans_time>next->modify_time){
        node=next;
        next=node->next;
    }
    cell=node;
}*/

/*
bool isWriteable(TransCtx *ctx,TEValue *value){
    if(value->tail->modify_time>ctx->trans_id_) //改为head.TEValue
        return false;
    else
        return true;
}*/
/*
 * void write(TransCtx *ctx,TEValue *value,CellInfoNode &cell){
    if(isWriteable(head,ctx)){
        if(value->uc_info==NULL){
            UCNode *ucNode=new UCNode();
            CellInfoNode node=new CellInfoNode();
            ucNode->head=node;
            ucNode->tail=node;
            ucNode->trans_id=ctx->trans_id_;
            ucNode->cell_cnt=1;
            value->uc_info=ucNode;
            cell=node;
        }
        else{
            CellInfoNode node=new CellInfoNode();
            value->uc_info->tail->next=node;
            value->uc_info->tail=node;
            value->uc_info->cell_cnt++;
            cell=node;
        }
    }
}*/
/*
void commit(TransCtx *ctx,TEValue *value){
    if(value->uc_info->tail!=NULL){
        CellInfoNode *pre_tail=value->tail;
        value->uc_info->tail->modify_time=ctx->trans_id_;
        value->tail->next=value->uc_info->tail;
        value->tail=value->uc_info->tail;
        value->cell_cnt++;
        if(value->cell_cnt>5)
            addToUndo(value,pre_tail);
    }
    //释放uc_list
    CellInfoNode *node=value->uc_info->head;
    value->uc_info->head=NULL;
    for(int i=0;i<value->uc_info->cell_cnt-1;i++){
        CellInfoNode *old_node=node;
        node=node->next;
        delete old_node;
    }
    delete value->uc_info;
}*/
/*
 * void addToUndo(TEValue *value,CellInfoNode *tail){
    undo_node->head=value->head;
    undo_node->tail=tail;
    undo_node->next=value->undo_list;
    value->undo_list=undo_node;
    value->undo_cnt++;
    value->head=value->tail;
    if(value->undo_cnt>5)

}
*/
/*
 * void releaseOldNode(TEValue *value){
    UndoNode *undoNode=value->undo_list;
    UndoNode *undoNode3;
    for(int i=0;i<2;i++){
        undoNode=undoNode->next;
        if(i==1)
            undoNode3=undoNode;
    }
    undoNode=undoNode->next;
    undoNode3->next=NULL;
    while(undoNode!=NULL){
        UndoNode *pre=undoNode;
        undoNode=undoNode->next;
        delete pre;
    }
    value->undo_cnt=3;
}

void releaseUndoList(UndoNode *undoNode){
    CellInfoNode *node=undoNode->head;
    while(node!=NULL){
        CellInfoNode *pre=node;
        node=node->next;
        delete pre;
    }
    delete undoNode;
}*/

/*
void MVCCMgr::findVersionInList(Value *head,uint64_t trans_time,Value *tail,Value *value){

}*/
//}
#endif
