#include <iostream>
#include <random>

#include "ap_int.h"

#define N 4
#define TESTS 1000

typedef ap_uint<4> A_DATA_TYPE;
typedef ap_int<4> B_DATA_TYPE;
typedef ap_int<10> OUT_DATA_TYPE;

void mul_gold(A_DATA_TYPE a[N], B_DATA_TYPE b, OUT_DATA_TYPE c[N]) {
  for (int i = 0; i < N; ++i)
    c[i] = (a[i] * b);
}

void mul_dut(A_DATA_TYPE a[N], B_DATA_TYPE b, OUT_DATA_TYPE c[N]) {
#pragma HLS array_partition variable=a type=cyclic factor=4
#pragma HLS array_partition variable=c type=cyclic factor=4
  for (int i = 0; i < N; ++i) {
#pragma HLS pipeline
#pragma HLS unroll factor=4
    c[i] = (a[i] * b);
  }
}

int main()
{
  A_DATA_TYPE a[N];
  B_DATA_TYPE b;
  OUT_DATA_TYPE c[N], c_gold[N];

  int errors = 0;
  for (int i = 0; i < TESTS; ++i) {
    std::cout << "Running test " << i << "... " << std::endl;
    for (int j = 0; j < N; ++j) {
      a[j] = std::rand();
    }
    b = std::rand();

    for (int j = 0; j < N; ++j)
      std::cout << "a[" << j << "]=" << a[j] << std::endl;
    std::cout << "b=" << b << std::endl;
  
    mul_dut(a, b, c);
    mul_gold(a, b, c_gold);

    for (int j = 0; j < N; ++j) {
      errors += (c[j] != c_gold[j]);
      std::cout << "c[" << j << "]: dut=" << c[j] << "; gold=" << c_gold[j] << " ";
      std::cout << ((c[j] == c_gold[j]) ? "OK" : "KO") << std::endl;
    }
  }

  if (errors)
    std::cout << "FAIL (" << errors << " errors)" << std::endl;
  else
    std::cout << "PASS" << std::endl;

  return (errors > 0);
}

