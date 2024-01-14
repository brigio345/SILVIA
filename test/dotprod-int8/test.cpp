#include <ap_int.h>
#include <cstdlib>
#include <iostream>

constexpr int N = 128;
constexpr int M = 128;

extern "C" {
void example(const ap_int<8> a[M][N], const ap_int<8> b[N], ap_int<58> c[M]) {
#pragma HLS INTERFACE ap_memory port = a
#pragma HLS INTERFACE ap_memory port = b
  for (auto m = 0; m < M; m++) {
#pragma HLS pipeline II = 1
    c[m] = 0;
    for (int n = 0; n < N; n += 1) {
#pragma HLS PIPELINE II = 1
#pragma HLS UNROLL factor = 8
      c[m] += a[m][n] * b[n];
    }
  }
}
}

int main() {
  ap_int<8> a[M][N], b[N];
  ap_int<58> res[M], gold[M];

  std::srand(42);
  for (auto m = 0; m < M; ++m) {
    for (int n = 0; n < N; ++n) {
      a[m][n] = (std::rand() - (1 << 10));
      if (m == 0)
        b[n] = (std::rand() - (1 << 10));
    }
  }

  example(a, b, res);

  for (int m = 0; m < M; m++) {
    gold[m] = 0;
    for (int n = 0; n < N; n++)
      gold[m] += a[m][n] * b[n];
    if (gold[m] != res[m])
      return EXIT_FAILURE;
  }

  return 0;
}
