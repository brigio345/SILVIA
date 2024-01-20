#include "blackbox/muladd2simd/muladd2simd.h"
#include <ap_int.h>
#include <iostream>
#include <random>

typedef ap_int<8> din_t;
typedef ap_int<48> dout_t;
constexpr int N = 16;
constexpr int SEED = 12;

void example(const din_t a[N], const din_t d[N], const din_t b[N],
             dout_t dout[N]) {
  for (auto i = 0; i < N; i++) {
    dout[i] = muladd2simd(a[i], d[i], b[i], 0);
  }
  return;
}

int main(void) {
  std::mt19937 generator(SEED);
  std::uniform_int_distribution<int> distribution(-128, 127);

  din_t a[N], d[N], b[N];
  dout_t dout[N], ab[N], db[N];
  for (auto i = 0; i < N; i++) {
    a[i] = distribution(generator);
    b[i] = distribution(generator);
    d[i] = distribution(generator);
  }
  example(a, d, b, dout);

  // Extracting products.
  bool passed=true;
  for (auto i = 0; i < N; i++) {
    // std::cerr << "dout @ " << i << ": " << int(dout[i]) << "." << std::endl;
    db[i] = ap_int<16>(dout[i].range(15,0));
    ab[i] = (dout[i] >> 18) + dout_t(db[i].range(15, 15));
    if (ab[i] != int(a[i]) * b[i]) {
      std::cerr << "ERROR @ " << i << " on ab: " << int(ab[i])
                << " != " << (int(a[i]) * b[i]) << " (golden)." << std::endl;
      passed = false;
    }
    if (db[i] != int(d[i]) * b[i]) {
      std::cerr << "ERROR @ " << i << " on db: " << int(db[i])
                << " != " << (int(d[i]) * b[i]) << " (golden)." << std::endl;
      passed = false;
    }
    }
    return passed ? 0 : EXIT_FAILURE;
  }
