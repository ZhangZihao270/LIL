#ifndef TPCC_PROC_
#define TPCC_PROC_

#include <vector>
#include "tpcc_client.h"
#include "common/encode.h"
#include "common/buffer.h"
#include "common/debuglog.h"

namespace tpcc {
#define MAX_BUF_SIZE  1024
#define MAX_WRITE_BUF_SIZE 1024*1024

#define CLIENT_WRITE_BUFFER

  class TpccProc {
    //used to generate miniworkload
  public:
    struct MbRequest{
      MbRequest(
          int32_t key_,
          int32_t mode_,
          int32_t value_):
        key(key_), mode(mode_), value(value_)
      {}
      int32_t key;
      int32_t mode;//0 for read and 1 for write
      int32_t value;//equals -1 when mode is read

      const MbRequest& operator=(const MbRequest& temp)
      {
        key = temp.key;
        mode = temp.mode;
        value = temp.value;
        return *this;
      }
    };
  public:
    TpccProc() : trans_id_(UINT64_MAX), batch_buffer_(MAX_WRITE_BUF_SIZE) {}

    bool run_payment(TpccClient &client, int w_id, int d_id, int c_w_id, int c_d_id, 
        int c_id, double h_amount);

    bool run_neworder(TpccClient &client, int w_id, int d_id, int c_id, int o_ol_cnt,
        int *items, int *suppliers, int *quantities, int o_all_local);

    bool run_miniworkload(TpccClient &clien, std::vector<MbRequest> &mb_requsets);

    uint64_t& transid() { return trans_id_; }

    void end(TpccClient &client, bool commit);

  private:
    template <typename T>
      bool read(TpccClient &client, const typename T::Key &key, 
          const int64_t cols, typename T::Value &value, const Lock_type lock_mode = L_READ) {
        int64_t pos = 0;
        key.write(buff_, MAX_BUF_SIZE, pos);
        string in(buff_, pos), ret;
        if( 0 == client.get(trans_id_, T::TID, cols, in, ret, lock_mode) ) {
          pos = 0;
          value.read(ret.c_str(), ret.size(), pos, cols);
          return true;
        }
        return false;
      }

    template <typename T>
      bool update(TpccClient &client, const typename T::Key &key, 
          const int64_t cols, const typename T::Value &value) {
#ifdef CLIENT_WRITE_BUFFER
        return buffer_write<T>(DML_UPDATE, key, cols, value);
#endif
        int64_t pos = 0;
        key.write(buff_, MAX_BUF_SIZE, pos);
        string s_key(buff_, pos);
        
        pos = 0;
        value.write(buff_, MAX_BUF_SIZE, pos, cols);
        string s_value(buff_, pos);

        return (0 == client.update(trans_id_, T::TID, cols, s_key, s_value));
      }

    template <typename T>
      bool insert(TpccClient &client, const typename T::Key &key, 
          const int64_t cols, const typename T::Value &value) {
#ifdef CLIENT_WRITE_BUFFER
        return buffer_write<T>(DML_INSERT, key, cols, value);
#endif
        int64_t pos = 0;
        key.write(buff_, MAX_BUF_SIZE, pos);
        string s_key(buff_, pos);
        
        pos = 0;
        value.write(buff_, MAX_BUF_SIZE, pos, cols);
        string s_value(buff_, pos);

        return (0 == client.insert(trans_id_, T::TID, cols, s_key, s_value));
      }

    template <typename T>
      bool buffer_write(const int64_t dml_type, const typename T::Key &key, 
          const int64_t cols, const typename T::Value &value) {
        int64_t tid = T::TID;
        int64_t npos = batch_buffer_.pos();
        encode(dml_type, batch_buffer_.buff(), batch_buffer_.size(), batch_buffer_.pos());
        encode(tid,      batch_buffer_.buff(), batch_buffer_.size(), batch_buffer_.pos());
        encode(cols,     batch_buffer_.buff(), batch_buffer_.size(), batch_buffer_.pos());
        key.write(       batch_buffer_.buff(), batch_buffer_.size(), batch_buffer_.pos());
        value.write(     batch_buffer_.buff(), batch_buffer_.size(), batch_buffer_.pos(), cols);
        return batch_buffer_.pos() < batch_buffer_.size();
      }

  private:
    uint64_t trans_id_;
    Buffer batch_buffer_;

    char buff_[MAX_BUF_SIZE];
  };
}

#endif
