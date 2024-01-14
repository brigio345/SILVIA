#include <ap_int.h>
#include <cstdlib>
#include <iostream>

constexpr int N = 128;
constexpr int M = 128;
constexpr int PARALLELIZATION = 8;

void example(const ap_int<8> a[M][N], const ap_int<8> b[N], ap_int<58> c[M]) {

#pragma HLS INTERFACE ap_memory port = a
#pragma HLS INTERFACE ap_memory port = b

  ap_int<8> aLoc[M][N], bLoc[N], cLoc[M];
#pragma HLS array_reshape variable = aLoc type = cyclic dim = 2 factor =       \
    PARALLELIZATION
#pragma HLS array_reshape variable = bLoc type = cyclic dim = 1 factor =       \
    PARALLELIZATION
#pragma HLS array_partition variable = cLoc type = cyclic dim = 1 factor =     \
    PARALLELIZATION

  for (auto m = 0; m < M; m++) {
    cLoc[m] = 0;
    for (auto n = 0; n < N; n++)
      aLoc[m][n] = a[m][n];
  }

  for (auto n = 0; n < N; n++)
    bLoc[n] = b[n];

  for (auto m = 0; m < M; m++) {
#pragma HLS pipeline II = M / PARALLELIZATION
#pragma HLS unroll factor = PARALLELIZATION
    for (int n = 0; n < N; n++) {
#pragma HLS UNROLL factor = PARALLELIZATION
      cLoc[m] += aLoc[m][n] * bLoc[n];
    }
  }

  for (auto m = 0; m < M; m++)
    c[m] = cLoc[m];
}

int main() {
  ap_int<8> a[M][N], b[N]{0};
  ap_int<58> res[M]{0}, gold[M]{0};

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
