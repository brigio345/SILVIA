#include <ap_int.h>
#include <random>
#include <iostream>

#define N 128
#define TESTS 100
//#define DEBUG

void mul_8b(ap_int<8> a[N], ap_int<8> b[N], ap_int<8> c[N], ap_int<16> p0[N],
    ap_int<16> p1[N]) {
#pragma HLS interface ap_memory port=a
#pragma HLS interface ap_memory port=b
#pragma HLS interface ap_memory port=c
#pragma HLS interface ap_memory port=p0
#pragma HLS interface ap_memory port=p1

  for (int i = 0; i < N; ++i) {
#pragma HLS pipeline II=1
    p0[i] = a[i] * c[i];
    p1[i] = b[i] * c[i];
  }
}

int main() {
  ap_int<8> a[N], b[N], c[N];
  ap_int<16> p0[N], p1[N], p0_gold[N], p1_gold[N];

  int errs = 0;
  for (int i = 0; i < TESTS; ++i) {
    for (int j = 0; j < N; ++j) {
      a[j] = std::rand();
      b[j] = std::rand();
      c[j] = std::rand();
      p0_gold[j] = a[j] * c[j];
      p1_gold[j] = b[j] * c[j];
    }

    mul_8b(a, b, c, p0, p1);

    for (int j = 0; j < N; ++j) {
#ifdef DEBUG
      std::cout << "a[" << j << "]=" << a[j] << " * c[" << j << "]=" << c[j] <<
	      " = " << p0[j] << "(" << p0_gold[j] << "): " <<
	      (p0[j] == p0_gold[j] ?  "OK" : "KO") << std::endl;
      std::cout << "b[" << j << "]=" << b[j] << " * c[" << j << "]=" << c[j] <<
	      " = " << p1[j] << "(" << p1_gold[j] << "): " <<
	      (p1[j] == p1_gold[j] ?  "OK" : "KO") << std::endl;
#endif /* DEBUG */
      errs += (p0[j] != p0_gold[j]);
      errs += (p1[j] != p1_gold[j]);
    }
  }

  return (errs > 0);
}
