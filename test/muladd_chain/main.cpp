#include <iostream>
#include <random>

#include "ap_int.h"

#define N 20
#define TESTS 1000

typedef ap_int<8> DATA_TYPE;

void extract_prods(ap_int<48> M, ap_int<18> &AD, ap_int<18> &BD) {
#pragma HLS inline off
#pragma HLS pipeline II=1
#pragma HLS latency min=0 max=0
  BD = M(17, 0);
  AD = M(18 + 18, 18) + BD(17, 17);
}

void mul_add_inner(ap_int<8> a, ap_int<8> d, ap_int<8> b, ap_int<36> PCIN, ap_int<36> &PCOUT) {
#pragma HLS inline
//#pragma HLS pipeline II=1
//#pragma HLS latency min=2 max=2
//#pragma HLS bind_op variable=PCOUT op=addmuladd impl=dsp latency=3
  ap_int<27> A = ((ap_int<27>)a << 18);
  ap_int<27> D = d;
  ap_int<18> B = b;

  PCOUT = (((A + D) * B) + PCIN);
}

void mul_add(ap_int<8> a, ap_int<8> d, ap_int<8> b, ap_int<36> PCIN, ap_int<36> &PCOUT) {
  DATA_TYPE a_inner = a;
  ap_int<27> d_inner = d;
  ap_int<18> b_inner = b;
  mul_add_inner(a_inner, d_inner, b_inner, PCIN, PCOUT);
}

void muladd(DATA_TYPE a[N], DATA_TYPE d[N], DATA_TYPE b[N], ap_int<48> &r0, ap_int<48> &r1) {
#pragma HLS array_partition variable=a type=complete
#pragma HLS array_partition variable=d type=complete
#pragma HLS array_partition variable=b type=complete
#pragma HLS pipeline
  ap_int<48> r0_tmp = 0;
  ap_int<48> r1_tmp = 0;

  ap_int<36> P;
  for (int i = 0; i < N; ++i) {
    if (i % 7 == 0)
      P = 0;

    mul_add(a[i], d[i], b[i], P, P);

    if ((i + 1) % 7 == 0 || (i + 1) == N) {
      ap_int<18> c0, c1;
      extract_prods(P, c0, c1);
      r0_tmp += c0;
      r1_tmp += c1;
    }
  }

  r0 = r0_tmp;
  r1 = r1_tmp;
}

void muladd_gold(DATA_TYPE a[N], DATA_TYPE d[N], DATA_TYPE b[N], ap_int<48> &r0, ap_int<48> &r1) {
  ap_int<48> r0_tmp = 0;
  ap_int<48> r1_tmp = 0;
  for (int i = 0; i < N; ++i) {
  //#pragma HLS bind_op variable=r0_tmp op=mul impl=fabric
    r0_tmp += (a[i] * b[i]);
    r1_tmp += (d[i] * b[i]);
  }

  r0 = r0_tmp;
  r1 = r1_tmp;
}

void muladd_dut(DATA_TYPE a[N], DATA_TYPE d[N], DATA_TYPE b[N], ap_int<48> &r0, ap_int<48> &r1) {
#pragma HLS array_partition variable=a type=complete
#pragma HLS array_partition variable=d type=complete
#pragma HLS array_partition variable=b type=complete
#pragma HLS pipeline
  ap_int<48> r0_tmp = 0;
  ap_int<48> r1_tmp = 0;
  for (int i = 0; i < N; ++i) {
  //#pragma HLS bind_op variable=r0_tmp op=mul impl=fabric
    r0_tmp += (a[i] * b[i]);
    r1_tmp += (d[i] * b[i]);
  }

  r0 = r0_tmp;
  r1 = r1_tmp;
}

int main()
{
  DATA_TYPE a[N], d[N], b[N];
  ap_int<48> r0, r1, r0_gold, r1_gold;

  int errors = 0;
  for (int i = 0; i < TESTS; ++i) {
    std::cout << "Running test " << i << "... " << std::endl;
    for (int j = 0; j < N; ++j) {
      a[j] = std::rand();
      d[j] = std::rand();
      b[j] = std::rand();
    }

    for (int j = 0; j < N; ++j)
      std::cout << "a[" << j << "]=" << a[j] << std::endl;
    for (int j = 0; j < N; ++j)
      std::cout << "d[" << j << "]=" << d[j] << std::endl;
    for (int j = 0; j < N; ++j)
      std::cout << "b[" << j << "]=" << b[j] << std::endl;
  
    muladd_dut(a, d, b, r0, r1);
    muladd_gold(a, d, b, r0_gold, r1_gold);

    errors += (r0 != r0_gold);
    errors += (r1 != r1_gold);
    std::cout << "r0: " << r0 << "; " << r0_gold << std::endl;
    std::cout << "r1: " << r1 << "; " << r1_gold << std::endl;

    std::cout << ((r0 == r0_gold && r1 == r1_gold) ? "OK" : "KO") << std::endl;
  }

  if (errors)
    std::cout << "FAIL (" << errors << " errors)" << std::endl;
  else
    std::cout << "PASS" << std::endl;

  return (errors > 0);
}

