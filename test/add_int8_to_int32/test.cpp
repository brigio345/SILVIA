#include <ap_int.h>
#include <cstdlib>
#include <iostream>

const int N = 16;

extern "C" {
void example(ap_int<8> a[N], ap_int<8> b[N], ap_int<32> c[N]) {
#pragma HLS INTERFACE ap_memory port = a
#pragma HLS INTERFACE ap_memory port = b
  for (int i = 0; i < N; i += 1) {
#pragma HLS PIPELINE II = 1
#pragma HLS UNROLL factor = 4
    c[i] = (ap_int<32>)(a[i]) + (ap_int<32>)(b[i]);
  }
}
}

int main() {
  ap_int<8> a[N], b[N];
  ap_int<32> c[N], c_gold[N];

  std::srand(42);
  for (int i = 0; i < N; ++i) {
    a[i] = (std::rand() - (1 << 10));
    b[i] = (std::rand() - (1 << 10));
  }

  example(a, b, c);
  for (int i = 0; i < N; i++)
    c_gold[i] = a[i] + b[i];

  int mismatch = 0;
  for (int i = 0; i < N; i++)
    mismatch += (c[i] != c_gold[i]);

  return (mismatch > 0);
}
