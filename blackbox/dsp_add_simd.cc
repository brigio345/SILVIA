#include "dsp_add_simd.h"

#include <iostream>

// Pipeline 4-SIMD DSPs with different latencies.
ap_int<48> dsp_add_4simd_pipe_l0(ap_int<96> inputs) {
#pragma HLS inline off
  ap_int<48> dout;
  auto dout0 = inputs(11,  0) + inputs(23, 12);
  auto dout1 = inputs(35, 24) + inputs(47, 36);
  auto dout2 = inputs(59, 48) + inputs(71, 60);
  auto dout3 = inputs(83, 72) + inputs(95, 84);
  dout(11,  0) = dout0;
  dout(23, 12) = dout1;
  dout(35, 24) = dout2;
  dout(47, 36) = dout3;
  return dout;
}

ap_int<48> dsp_add_4simd_pipe_l1(ap_int<12> a0, ap_int<12> a1, ap_int<12> a2,
                              ap_int<12> a3, ap_int<12> b0, ap_int<12> b1,
                              ap_int<12> b2, ap_int<12> b3) {
#pragma HLS inline off
  ap_int<48> dout;
  auto dout0 = a0 + b0;
  auto dout1 = a1 + b1;
  auto dout2 = a2 + b2;
  auto dout3 = a3 + b3;
  dout(47, 36) = dout0;
  dout(35, 24) = dout1;
  dout(23, 12) = dout2;
  dout(11, 0) = dout3;
  return dout;
}

ap_int<48> dsp_add_4simd_pipe_l2(ap_int<12> a0, ap_int<12> a1, ap_int<12> a2,
                              ap_int<12> a3, ap_int<12> b0, ap_int<12> b1,
                              ap_int<12> b2, ap_int<12> b3) {
#pragma HLS inline off
  ap_int<48> dout;
  auto dout0 = a0 + b0;
  auto dout1 = a1 + b1;
  auto dout2 = a2 + b2;
  auto dout3 = a3 + b3;
  dout(47, 36) = dout0;
  dout(35, 24) = dout1;
  dout(23, 12) = dout2;
  dout(11, 0) = dout3;
  return dout;
}

// Sequential 4-SIMD DSPs with different latencies.
void dsp_add_4simd_seq_l0(ap_int<12> a0, ap_int<12> a1, ap_int<12> a2,
                       ap_int<12> a3, ap_int<12> b0, ap_int<12> b1,
                       ap_int<12> b2, ap_int<12> b3, ap_int<12>& dout0,
                       ap_int<12>& dout1, ap_int<12>& dout2,
                       ap_int<12>& dout3) {
#pragma HLS inline off
  dout0 = a0 + b0;
  dout1 = a1 + b1;
  dout2 = a2 + b2;
  dout3 = a3 + b3;
}

void dsp_add_4simd_seq_l1(ap_int<12> a0, ap_int<12> a1, ap_int<12> a2,
                       ap_int<12> a3, ap_int<12> b0, ap_int<12> b1,
                       ap_int<12> b2, ap_int<12> b3, ap_int<12>& dout0,
                       ap_int<12>& dout1, ap_int<12>& dout2,
                       ap_int<12>& dout3) {
#pragma HLS inline off
  dout0 = a0 + b0;
  dout1 = a1 + b1;
  dout2 = a2 + b2;
  dout3 = a3 + b3;
}

void dsp_add_4simd_seq_l2(ap_int<12> a0, ap_int<12> a1, ap_int<12> a2,
                       ap_int<12> a3, ap_int<12> b0, ap_int<12> b1,
                       ap_int<12> b2, ap_int<12> b3, ap_int<12>& dout0,
                       ap_int<12>& dout1, ap_int<12>& dout2,
                       ap_int<12>& dout3) {
#pragma HLS inline off
  dout0 = a0 + b0;
  dout1 = a1 + b1;
  dout2 = a2 + b2;
  dout3 = a3 + b3;
}

// Pipeline 2-SIMD DSPs with different latencies.
ap_int<48> dsp_add_2simd_pipe_l0(ap_int<24> a0, ap_int<24> a1, ap_int<24> b0,
                              ap_int<24> b1) {
#pragma HLS inline off
  ap_int<48> dout;
  auto dout0 = a0 + b0;
  auto dout1 = a1 + b1;
  dout(47, 24) = dout0;
  dout(23, 0) = dout1;
  return dout;
}

ap_int<48> dsp_add_2simd_pipe_l1(ap_int<24> a0, ap_int<24> a1, ap_int<24> b0,
                              ap_int<24> b1) {
#pragma HLS inline off
  ap_int<48> dout;
  auto dout0 = a0 + b0;
  auto dout1 = a1 + b1;
  dout(47, 24) = dout0;
  dout(23, 0) = dout1;
  return dout;
}

ap_int<48> dsp_add_2simd_pipe_l2(ap_int<24> a0, ap_int<24> a1, ap_int<24> b0,
                              ap_int<24> b1) {
#pragma HLS inline off
  ap_int<48> dout;
  auto dout0 = a0 + b0;
  auto dout1 = a1 + b1;
  dout(47, 24) = dout0;
  dout(23, 0) = dout1;
  return dout;
}

// Sequential 2-SIMD DSPs with different latencies.
void dsp_add_2simd_seq_l0(ap_int<24> a0, ap_int<24> a1, ap_int<24> b0,
                       ap_int<24> b1, ap_int<24>& dout0, ap_int<24>& dout1) {
#pragma HLS inline off
  dout0 = a0 + b0;
  dout1 = a1 + b1;
}

void dsp_add_2simd_seq_l1(ap_int<24> a0, ap_int<24> a1, ap_int<24> b0,
                       ap_int<24> b1, ap_int<24>& dout0, ap_int<24>& dout1) {
#pragma HLS inline off
  dout0 = a0 + b0;
  dout1 = a1 + b1;
}

void dsp_add_2simd_seq_l2(ap_int<24> a0, ap_int<24> a1, ap_int<24> b0,
                       ap_int<24> b1, ap_int<24>& dout0, ap_int<24>& dout1) {
#pragma HLS inline off
  dout0 = a0 + b0;
  dout1 = a1 + b1;
}
