#ifndef ENCODE_H
#define ENCODE_H

#include "serialization.h"

template<typename T> 
inline int encode(const T &value, char *buf, const int64_t data_len, int64_t &pos) {
  return (value.encode(buf, data_len, pos) ? SUCCESS : ERROR);
}

#define ENCODE(TYPE)  \
  template<> \
  inline int encode<TYPE>(const TYPE &value, char *buf, const int64_t data_len, int64_t &pos)

ENCODE(int64_t) 
{
  return common::encode_i64(buf, data_len, pos, value);
}

ENCODE(int32_t)
{ 
  return common::encode_i32(buf, data_len, pos, value);
}

ENCODE(int16_t)
{
  return common::encode_i16(buf, data_len, pos, value);
}

ENCODE(int8_t)
{
  return common::encode_i8(buf, data_len, pos, value);
}

ENCODE(bool)
{
  return common::encode_bool(buf, data_len, pos, value);
}

ENCODE(float)
{
  //int32_t tmp = *reinterpret_cast<const int32_t*>(&value);
  return common::encode_i32(buf, data_len, pos, reinterpret_cast<const int32_t &>(value));
}

ENCODE(double)
{
  //int64_t tmp = *reinterpret_cast<const int64_t*>(&value);
  return common::encode_i64(buf, data_len, pos, reinterpret_cast<const int64_t &>(value));
}

///////////////////////////// decode ////////////////////////////

template<typename T> 
inline int decode(T &value, const char *buf, const int64_t data_len, int64_t &pos) {
  return value.decode(buf, data_len, pos) ? SUCCESS : ERROR;
}

#define DECODE(TYPE) \
  template<> \
  inline int decode(TYPE &value, const char *buf, const int64_t data_len, int64_t &pos)

DECODE(int64_t) 
{
  int64_t tmp;
  int ret = common::decode_i64(buf, data_len, pos, &tmp);
  value = tmp;
  return ret;
}

DECODE(int32_t) 
{
  int32_t tmp;
  int ret = common::decode_i32(buf, data_len, pos, &tmp);
  value = tmp;
  return ret;
}

DECODE(int16_t) 
{
  int16_t tmp;
  int ret = common::decode_i16(buf, data_len, pos, &tmp);
  value = tmp;
  return ret;
}

DECODE(int8_t) 
{
  int8_t tmp;
  int ret = common::decode_i8(buf, data_len, pos, &tmp);
  value = tmp;
  return ret;
}

DECODE(bool) 
{
  bool tmp;
  int ret = common::decode_bool(buf, data_len, pos, &tmp);
  value = tmp;
  return ret;
}

DECODE(float) 
{
  int32_t tmp;
  int ret = common::decode_i32(buf, data_len, pos, &tmp);
  value = reinterpret_cast<const float &>(tmp);
  return ret;
}

DECODE(double)
{
  int64_t tmp;
  int ret = common::decode_i64(buf, data_len, pos, &tmp);
  value = reinterpret_cast<const double &>(tmp);
  return ret;
}

#endif
