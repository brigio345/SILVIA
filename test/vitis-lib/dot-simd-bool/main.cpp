#include "main.h"
#include "dut.h"
#include <ap_int.h>
#include <cstdint>
#include <iostream>
#include <random>

using namespace std;

void gen_random_vec(din_t *din) {
  mt19937 gen(SEED);
  uniform_int_distribution<int> uniform_dist(-(1 << (NBIT - 1)),
                                             (1 << (NBIT - 1)) - 1);
  for (auto i = 0; i < N; i++) {
    din[i] = uniform_dist(gen);
  }
  return;
}

int main(void) {
  din_t din0[N], din1[N], din2[N];

  gen_random_vec(din0);
  gen_random_vec(din1);
  gen_random_vec(din2);
  dout_t dout0, dout1;
#ifdef BOOL_INPUTS
  bool __din0[N], __din1[N];
  for (auto i = 0; i < N; i++) {
    __din0[i] = din0[i] % 2 == 0;
    __din1[i] = din1[i] % 2 == 0;
  }
  dut(__din0, __din1, din2, dout0, dout1);
#else 
  dut(din0, din1, din2, dout0, dout1);
#endif // BOOL_INPUTS


  int64_t golden0 = 0, golden1=0;
  for (auto i = 0; i < N; i++) {
#ifdef BOOL_INPUTS
    golden0 += __din0[i] * din2[i];
    golden1 += __din1[i] * din2[i];
#else
    golden0 += din0[i] * din2[i];
    golden1 += din1[i] * din2[i];
#endif // BOOL_INPUTS
  }

  if (golden0 != dout0) {
    cerr << "Golden: " << golden0 << " != dut: " << dout0 << "." << endl;
    return EXIT_FAILURE;
  }
  if (golden1 != dout1) {
    cerr << "Golden: " << golden1 << " != dut: " << dout1 << "." << endl;
    return EXIT_FAILURE;
  }
  return 0;
}
