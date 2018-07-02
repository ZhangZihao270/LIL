#ifndef RANDOM_H
#define RANDOM_H

#include <random>

class Random {
  public:
    Random() : rd_(), gen_(rd_()) {}

    inline string random_str(int llen, int ulen) {
      int len = random_int(llen, ulen);
      std::uniform_int_distribution<> dis(0, 25);
      string ret (len, '\0');
      for(int i = 0; i < len; ++i) 
        ret[i] = 'a' + dis(gen_);
      return ret;
    }

    inline int random_int(int l, int u) {
      std::uniform_int_distribution<> dis(l, u);
      return dis(gen_);
    }

    inline double random_double(double l, double u) {
      std::uniform_real_distribution<> dis(l, u);
      return dis(gen_);
    }

    inline float  random_float(float l, float u) {
      return random_double(l, u);
    }

    inline int non_uniform_random(int A, int C, int min, int max) {
      return (((random_int(0, A) | random_int(min, max)) + C) % (max - min + 1)) + min;
    }

  private:
    std::random_device rd_;
    std::mt19937 gen_;
};

#endif
