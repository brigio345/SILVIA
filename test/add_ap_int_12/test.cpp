#include <ap_int.h>
#include <cstdlib>
#include <iostream>

const int N = 1024;

extern "C" {
void example(ap_int<12> a[N], ap_int<12> b[N], ap_int<12> c[N]) {
#pragma HLS INTERFACE ap_memory port = a
#pragma HLS INTERFACE ap_memory port = b
  for (int i = 0; i < N; i += 1) {
#pragma HLS PIPELINE II = 1
#pragma HLS UNROLL factor = 4
    c[i] = a[i] + b[i];
  }
}
}

int main() {
  ap_int<12> a[N], b[N], res[N], res_gold[N];

  std::srand(42);
  for (int i = 0; i < N; ++i) {
    a[i] = (std::rand() - (1 << 10));
    b[i] = (std::rand() - (1 << 10));
  }

  example(a, b, res);
  for (int i = 0; i < N; i++)
    res_gold[i] = a[i] + b[i];

  int mismatch = 0;
  for (int i = 0; i < N; i++) {
    mismatch += (res[i] != res_gold[i]);
    std::cout << i << ": " << res_gold[i] << " " << res[i];
    if (res[i] != res_gold[i])
      std::cout << " MISMATCH";
    std::cout << std::endl;
  }

  return (mismatch > 0);
}
