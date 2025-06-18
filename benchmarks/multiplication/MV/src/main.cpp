#include "main.h"
#include "dut.h"
#include <ap_int.h>
#include <cstdint>
#include <iostream>
#include <random>

using namespace std;

int main(void) {
  din_t din[N];
  weight_t weights[N * N];

  // Generating data.
  mt19937 gen(SEED);
  uniform_int_distribution<int> uniform_dist(-(1 << (NBIT - 1)),
                                             (1 << (NBIT - 1)) - 1);
  for (auto i = 0; i < N; i++) {
#ifdef BOOL_INPUTS
    din[i] = uniform_dist(gen) % 2 == 0;
#else
    din[i] = uniform_dist(gen);
#endif // BOOL_INPUTS
    for (auto j = 0; j < N; j++) {
      weights[i * N + j] = uniform_dist(gen);
    }
  }

  dout_t dout[N]{0};
  int64_t golden[N]{0};

  dut(din, weights, dout);

  for (auto i = 0; i < N; i++) {
    for (auto j = 0; j < N; j++) {
      golden[i] += din[j] * weights[i * N + j];
    }
    if (golden[i] != dout[i]) {
      cerr << "ERROR @ " << i << ". Golden: " << golden[i]
           << " != dut: " << dout[i] << "." << endl;
      return EXIT_FAILURE;
    }
  }

  return 0;
}
