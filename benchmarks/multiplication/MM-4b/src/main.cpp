#include "main.h"
#include "dut.h"
#include <ap_int.h>
#include <cstdint>
#include <iostream>
#include <random>

using namespace std;

int main(void) {
  din_t din[N * N];
  weight_t weights[N * N];

  // Generating data.
  mt19937 gen(SEED);
  uniform_int_distribution<int> uniform_dist(-(1 << (NBIT - 1)),
                                             (1 << (NBIT - 1)) - 1);
  for (auto i = 0; i < N; i++) {
    for (auto j = 0; j < N; j++) {
#ifdef BOOL_INPUTS
      din[i * N + j] = uniform_dist(gen) % 2 == 0;
#else
      din[i * N + j] = uniform_dist(gen);
#endif // BOOL_INPUTS
      weights[i * N + j] = uniform_dist(gen);
    }
  }

  dout_t dout[N * N]{0};
  dout_t golden[N * N]{0};
  // for (auto i = 0; i < N*N; i++) golden[i] = 0;

  dut(din, weights, dout);

  // Assumption: the second matrix is already transposed.

  for (auto i = 0; i < N; i++) {
    for (auto j = 0; j < N; j++) {
      for (auto k = 0; k < N; k++) {
        golden[i * N + j] += din[i * N + k] * weights[j * N + k];
      }
      if (golden[i * N + j] != dout[i * N + j]) {
        cerr << "ERROR @ " << i << ". Golden: " << golden[i * N + j]
             << " != dut: " << dout[i * N + j] << "." << endl;
        return EXIT_FAILURE;
      }
    }
  }

  return 0;
}
