#ifndef CONFIG_H_
#define CONFIG_H_

#include <string>
#include <map>
using namespace std;

#define DEF(A, B) \
  Config::set(A,B);

class Config {
public:
  static int64_t get_int(const string &key);

  static const string get_string(const string &key);

  static void set(const string k, const string v);

  static void set(const string k, const int64_t v);

private:
  static map<string, string> value_;
};

#endif
