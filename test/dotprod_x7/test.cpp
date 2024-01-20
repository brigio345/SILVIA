#include "blackbox/dotprod_x7/dotprod_x7.h"
#include <ap_int.h>
#include <cstdint>
#include <iostream>
#include <random>

typedef ap_int<8> din_t;
typedef ap_int<48> dout_t;
constexpr int N = 16;
constexpr int SEED = 12;

void example(const din_t a[N][7], const din_t d[N][7], const din_t b[N][7],
             dout_t dout[N]) {
  for (auto i = 0; i < N; i++) {
    dout[i] = dotprod_x7(a[i][0], a[i][1], a[i][2], a[i][3], a[i][4], a[i][5],
                         a[i][6], d[i][0], d[i][1], d[i][2], d[i][3], d[i][4],
                         d[i][5], d[i][6], b[i][0], b[i][1], b[i][2], b[i][3],
                         b[i][4], b[i][5], b[i][6]);
  }
  return;
}

int main(void) {
  std::mt19937 generator(SEED);
  std::uniform_int_distribution<int> distribution(-128, 127);

  din_t a[N][7], d[N][7], b[N][7];
  dout_t dout[N];
  ap_int<18> ab[N], db[N];
  for (auto i = 0; i < N; i++) {
    for (auto j = 0; j < 7; j++) {
      a[i][j] = distribution(generator);
      b[i][j] = distribution(generator);
      d[i][j] = distribution(generator);
    }
  }
  example(a, d, b, dout);

  // Extracting products.
  bool passed = true;
  for (auto i = 0; i < N; i++) {
    int abGold = 0, dbGold = 0;
    for (auto j = 0; j < 7; j++) {
      abGold += int(a[i][j]) * b[i][j];
      dbGold += int(d[i][j]) * b[i][j];
    }
    db[i] = ap_int<18>(dout[i].range(17, 0));
    ab[i] = ap_int<18>((dout[i] >> 18) + dout_t(dout[i].range(17, 17)));
    if (ab[i] != abGold) {
      std::cerr << "ERROR @ " << i << " on ab: " << int(ab[i])
                << " != " << abGold << " (golden)." << std::endl;
      passed = false;
    }
    if (db[i] != dbGold) {
      std::cerr << "ERROR @ " << i << " on db: " << int(db[i])
                << " != " << dbGold << " (golden)." << std::endl;
      passed = false;
    }
  }
  return passed ? 0 : EXIT_FAILURE;
}
