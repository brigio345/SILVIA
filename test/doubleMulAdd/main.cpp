#include "doubleMulAdd.h"
#include <ap_int.h>
#include <cstdint>
#include <iostream>
#include <random>

typedef ap_uint<1> clk_t;
typedef ap_int<27> a_t;
typedef ap_int<18> b_t;
typedef ap_int<48> c_t;
typedef ap_int<48> muladd_t;

typedef int8_t din_t;
typedef int32_t dout_t;

constexpr bool DEBUG = false;
constexpr auto N = DEBUG ? 8 : 2048;
constexpr auto SEED = 32;

void dut(clk_t ap_clk2x, const din_t a[], const din_t b[], const dout_t c[],
         dout_t dout[]) {
#pragma HLS interface mode = m_axi port = a depth = N
#pragma HLS interface mode = m_axi port = b depth = N
#pragma HLS interface mode = m_axi port = c depth = N
#pragma HLS interface mode = m_axi port = dout depth = N
#pragma HLS interface mode = s_axilite port = return

  for (auto i = 0; i < N; i += 2) {
#pragma HLS pipeline
#pragma HLS unroll factor = 1
#if 1
    auto res = doubleMulAdd(ap_clk2x, a_t(a[i]), b_t(b[i]), c_t(c[i]),
                            a_t(a[i + 1]), b_t(b[i + 1]), c_t(c[i + 1]));
    dout[i] = dout_t(muladd_t(res(95, 48)));
    dout[i + 1] = dout_t(muladd_t(res(47, 0)));
#else
    mac0[i] = dout_t(a0[i]) * b0[i] + c0[i];
    mac1[i] = dout_t(a1[i]) * b1[i] + c1[i];
#endif
  }
  return;
}

int main(void) {
  din_t a[N];
  din_t b[N];
  dout_t c[N];
  dout_t dout[N];
  clk_t clk;

  // Seed with a real random value, if available
  std::random_device r;
  std::default_random_engine e1(32);
  std::uniform_int_distribution<int> din_dist(-(1 << 7), (1 << 7) - 1);
  std::uniform_int_distribution<int> dout_dist(-(1 << 15), (1 << 15) - 1);

  for (auto i = 0; i < N; i++) {
    a[i] = din_dist(e1);
    b[i] = din_dist(e1);
    c[i] = dout_dist(e1);
    if (DEBUG)
      std::cout << i << "): (" << a[i] << ", " << b[i] << ", " << c[i] << ")."
                << std::endl;
  }

  dut(clk, a, b, c, dout);

  for (auto i = 0; i < N; i++) {
    if (int(dout[i]) != int(a[i]) * b[i] + c[i]) {
      std::cerr << "ERROR (" << i << "): " << dout[i] << " != " << a[i] << " * "
                << b[i] << " + " << c[i] << " (" << int(a[i]) * b[i] + c[i]
                << ")." << std::endl;
      return EXIT_FAILURE;
    }
  }

  return 0;
}
