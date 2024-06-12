#include <iostream>
#include <random>

#include "ap_int.h"

#define N 128
#define TESTS 1000

const unsigned WAYS = 8;

typedef ap_uint<4> A_DATA_TYPE;
typedef ap_uint<4> B_DATA_TYPE;
typedef ap_uint<16> OUT_DATA_TYPE;

OUT_DATA_TYPE muladd_gold(A_DATA_TYPE a[N], B_DATA_TYPE b) {
  OUT_DATA_TYPE res = 0;
  for (int i = 0; i < N; ++i) {
  //#pragma HLS bind_op variable=r0_tmp op=mul impl=fabric
    res += (a[i] * b);
  }

  return res;
}

OUT_DATA_TYPE muladd_dut(A_DATA_TYPE a[N], B_DATA_TYPE b) {
#pragma HLS array_partition variable=a type=cyclic factor=WAYS
  OUT_DATA_TYPE res = 0;
  for (int i = 0; i < N; ++i) {
#pragma HLS pipeline
#pragma HLS unroll factor=WAYS
    res += (a[i] * b);
  }

  return res;
}

int main()
{
  A_DATA_TYPE a[N];
  B_DATA_TYPE b;
  OUT_DATA_TYPE res, res_gold;

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
  
    res = muladd_dut(a, b);
    res_gold = muladd_gold(a, b);

    errors += (res != res_gold);

    std::cout << "res: dut=" << res << "; gold=" << res_gold << std::endl;

    std::cout << ((res == res_gold) ? "OK" : "KO") << std::endl;
  }

  if (errors)
    std::cout << "FAIL (" << errors << " errors)" << std::endl;
  else
    std::cout << "PASS" << std::endl;

  return (errors > 0);
}

