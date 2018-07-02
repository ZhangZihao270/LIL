#include "config.h"

map<string, string> Config::value_;

int64_t Config::get_int(const string &key) {
  int64_t ret = -1;
  if( value_.end() != value_.find(key) )
    ret = stoi(value_[key]);
  return ret;
}

const string Config::get_string(const string &key) {
  if( value_.end() != value_.find(key) )
    return value_[key];
  return "";
}

void Config::set(const string key, const string value) {
  value_[key] = value;
}

void Config::set(const string key, const int64_t value) {
  value_[key] = to_string(value);
}
