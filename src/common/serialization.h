/*
 * serialization.h
 *
 *      Author: tzhu
 */

#ifndef SERIALIZATION_H_
#define SERIALIZATION_H_

const int SUCCESS = 0;
const int ERROR = -1;

namespace common {
  /**
   * encode methods here, using little endian, small bytes on the small position
   */
  inline int encode_i8(char *buf, const int64_t buf_len, int64_t &pos,
      int8_t val) {

    int ret =
      ((NULL != buf)
       && (buf_len - pos >= static_cast<int64_t>(sizeof(val)))) ?
      SUCCESS : ERROR;

    if (SUCCESS == ret) {
      *(buf + pos++) = val;
    }
    return ret;
  }

  inline int decode_i8(const char *buf, const int64_t buf_len, int64_t &pos,
      int8_t *val) {

    int ret = (NULL != buf && buf_len - pos >= 1) ? SUCCESS : ERROR;
    if (SUCCESS == ret) {
      *val = *(buf + pos++);
    }
    return ret;
  }

  inline int encode_i16(char * buf, const int64_t buf_len, int64_t& pos,
      int16_t val) {
    int ret =
      ((NULL != buf)
       && ((buf_len - pos) >= static_cast<int64_t>(sizeof(val)))) ?
      SUCCESS : ERROR;
    if (SUCCESS == ret) {
      *(buf + pos++) = static_cast<char>((((val) >> 8)) & 0xff);
      *(buf + pos++) = static_cast<char>((val) & 0xff);
    }
    return ret;
  }

  inline int decode_i16(const char * buf, const int64_t data_len, int64_t& pos,
      int16_t* val) {
    int ret = (NULL != buf && data_len - pos >= 2) ? SUCCESS : ERROR;
    if (SUCCESS == ret) {
      *val = static_cast<int16_t>(((*(buf + pos++)) & 0xff) << 8);
      *val = static_cast<int16_t>(*val | (*(buf + pos++) & 0xff));
    }
    return ret;
  }

  inline int encode_i32(char *buf, const int64_t buf_len, int64_t& pos,
      int32_t val) {
    int ret =
      ((NULL != buf)
       && ((buf_len - pos) >= static_cast<int64_t>(sizeof(val)))) ?
      SUCCESS : ERROR;
    if (SUCCESS == ret) {
      *(buf + pos++) = static_cast<char>(((val) >> 24) & 0xff);
      *(buf + pos++) = static_cast<char>(((val) >> 16) & 0xff);
      *(buf + pos++) = static_cast<char>(((val) >> 8) & 0xff);
      *(buf + pos++) = static_cast<char>((val) & 0xff);
    }
    return ret;
  }

  inline int decode_i32(const char * buf, const int64_t data_len, int64_t& pos,
      int32_t* val) {

    int ret = (NULL != buf && data_len - pos >= 4) ? SUCCESS : ERROR;
    if (SUCCESS == ret) {
      *val = ((static_cast<int32_t>(*(buf + pos++))) & 0xff) << 24;
      *val |= ((static_cast<int32_t>(*(buf + pos++))) & 0xff) << 16;
      *val |= ((static_cast<int32_t>(*(buf + pos++))) & 0xff) << 8;
      *val |= ((static_cast<int32_t>(*(buf + pos++))) & 0xff);
    }
    return ret;
  }

  inline int encode_i64(char *buf, const int64_t buf_len, int64_t& pos,
      int64_t val) {
    int ret =
      ((NULL != buf)
       && ((buf_len - pos) >= static_cast<int64_t>(sizeof(val)))) ?
      SUCCESS : ERROR;
    if (SUCCESS == ret) {
      *(buf + pos++) = static_cast<char>(((val) >> 56) & 0xff);
      *(buf + pos++) = static_cast<char>(((val) >> 48) & 0xff);
      *(buf + pos++) = static_cast<char>(((val) >> 40) & 0xff);
      *(buf + pos++) = static_cast<char>(((val) >> 32) & 0xff);
      *(buf + pos++) = static_cast<char>(((val) >> 24) & 0xff);
      *(buf + pos++) = static_cast<char>(((val) >> 16) & 0xff);
      *(buf + pos++) = static_cast<char>(((val) >> 8) & 0xff);
      *(buf + pos++) = static_cast<char>((val) & 0xff);
    }
    return ret;
  }

  inline int decode_i64(const char * buf, const int64_t data_len, int64_t& pos,
      int64_t* val) {
    int ret = (NULL != buf && data_len - pos >= 8) ? SUCCESS : ERROR;
    if (SUCCESS == ret) {
      *val = ((static_cast<int64_t>((*(buf + pos++))) & 0xff)) << 56;
      *val |= ((static_cast<int64_t>((*(buf + pos++))) & 0xff)) << 48;
      *val |= ((static_cast<int64_t>((*(buf + pos++))) & 0xff)) << 40;
      *val |= ((static_cast<int64_t>((*(buf + pos++))) & 0xff)) << 32;
      *val |= ((static_cast<int64_t>((*(buf + pos++))) & 0xff)) << 24;
      *val |= ((static_cast<int64_t>((*(buf + pos++))) & 0xff)) << 16;
      *val |= ((static_cast<int64_t>((*(buf + pos++))) & 0xff)) << 8;
      *val |= ((static_cast<int64_t>((*(buf + pos++))) & 0xff));
    }
    return ret;
  }

  inline int encode_bool(char *buf, const int64_t buf_len, int64_t& pos,
      bool val) {
    int ret =
      ((NULL != buf)
       && ((buf_len - pos) >= static_cast<int64_t>(sizeof(val)))) ?
      SUCCESS : ERROR;
    if (SUCCESS == ret)
      *(buf + pos++) = (val) ? 1 : 0;
    return ret;
  }

  inline int decode_bool(const char *buf, const int64_t buf_len, int64_t& pos,
      bool *val) {
    int ret = ERROR;
    int8_t v = 0;
    if ((ret = decode_i8(buf, buf_len, pos, &v)) == 0) {
      *val = (v != 0);
    }
    return ret;
  }

  inline int encode_str_fix(char *buf, const int64_t buf_len, int64_t &pos, const char *str, int32_t size){

    int ret =
      ((NULL != buf)
       && ((buf_len - pos) >= static_cast<int64_t>(size))) ?
      SUCCESS : ERROR;
    if( SUCCESS == ret ) {
      for (int32_t i = 0; i < size; ++i) {

        *(buf + pos++) = str[i];
      }
    }
    return ret;
  }

  inline int decode_str_fix(const char * buf, const int64_t data_len, int64_t& pos, char *str, int32_t size){

    int ret = (NULL != buf && data_len - pos >= static_cast<int64_t>(size)) ? SUCCESS : ERROR;

    if( SUCCESS == ret ){
      for(int32_t i = 0;i<size; ++i){
        str[i] = *(buf + pos++);
      }
    }
    return ret;
  }

}

#endif /* SERIALIZATION_H_ */
