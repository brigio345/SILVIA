#ifndef HLS4NM_DSP_ADD_SIMD_H_
#define HLS4NM_DSP_ADD_SIMD_H_

#include "ap_int.h"

// Pipeline 4-SIMD DSPs with different latencies.
ap_int<48> dsp_add_4simd_pipe_l0(ap_int<48> a, ap_int<48> b);

ap_int<48> dsp_add_4simd_pipe_l1(ap_int<12> a0, ap_int<12> a1, ap_int<12> a2,
                                 ap_int<12> a3, ap_int<12> b0, ap_int<12> b1,
                                 ap_int<12> b2, ap_int<12> b3);

ap_int<48> dsp_add_4simd_pipe_l2(ap_int<12> a0, ap_int<12> a1, ap_int<12> a2,
                                 ap_int<12> a3, ap_int<12> b0, ap_int<12> b1,
                                 ap_int<12> b2, ap_int<12> b3);

// Sequential 4-SIMD DSPs with different latencies.
void dsp_add_4simd_seq_l0(ap_int<12> a0, ap_int<12> a1, ap_int<12> a2,
                          ap_int<12> a3, ap_int<12> b0, ap_int<12> b1,
                          ap_int<12> b2, ap_int<12> b3, ap_int<12>& dout0,
                          ap_int<12>& dout1, ap_int<12>& dout2,
                          ap_int<12>& dout3);

void dsp_add_4simd_seq_l1(ap_int<12> a0, ap_int<12> a1, ap_int<12> a2,
                          ap_int<12> a3, ap_int<12> b0, ap_int<12> b1,
                          ap_int<12> b2, ap_int<12> b3, ap_int<12>& dout0,
                          ap_int<12>& dout1, ap_int<12>& dout2,
                          ap_int<12>& dout3);

void dsp_add_4simd_seq_l2(ap_int<12> a0, ap_int<12> a1, ap_int<12> a2,
                          ap_int<12> a3, ap_int<12> b0, ap_int<12> b1,
                          ap_int<12> b2, ap_int<12> b3, ap_int<12>& dout0,
                          ap_int<12>& dout1, ap_int<12>& dout2,
                          ap_int<12>& dout3);

// Pipeline 2-SIMD DSPs with different latencies.
ap_int<48> dsp_add_2simd_pipe_l0(ap_int<24> a0, ap_int<24> a1, ap_int<24> b0,
                                 ap_int<24> b1);

ap_int<48> dsp_add_2simd_pipe_l1(ap_int<24> a0, ap_int<24> a1, ap_int<24> b0,
                                 ap_int<24> b1);

ap_int<48> dsp_add_2simd_pipe_l2(ap_int<24> a0, ap_int<24> a1, ap_int<24> b0,
                                 ap_int<24> b1);

// Sequential 2-SIMD DSPs with different latencies.
void dsp_add_2simd_seq_l0(ap_int<24> a0, ap_int<24> a1, ap_int<24> b0,
                          ap_int<24> b1, ap_int<24>& dout0, ap_int<24>& dout1);

void dsp_add_2simd_seq_l1(ap_int<24> a0, ap_int<24> a1, ap_int<24> b0,
                          ap_int<24> b1, ap_int<24>& dout0, ap_int<24>& dout1);

void dsp_add_2simd_seq_l2(ap_int<24> a0, ap_int<24> a1, ap_int<24> b0,
                          ap_int<24> b1, ap_int<24>& dout0, ap_int<24>& dout1);

//////////////////////////////////
/// 4-way SIMD
/////////////////////////////////

//////////////////////////////////
/// 2-way SIMD
/////////////////////////////////

template <typename a_t, typename b_t, typename dout_t, unsigned LATENCY = 1,
          bool PIPELINE = true>
void add_2simd(a_t a[2], b_t b[2], dout_t dout[2]) {
  static_assert(PIPELINE == true, "ERROR: Sequential mode not available.");
  static_assert(LATENCY <= 2, "ERROR: Latency cannot be greater than 2.");
#pragma HLS inline
  if constexpr (PIPELINE) {
    ap_int<48> tmp;
    if (LATENCY == 0) {
      tmp = dsp_add_2simd_pipe_l0(ap_int<24>(a[0]), ap_int<24>(a[1]),
                                  ap_int<24>(b[0]), ap_int<24>(b[1]));
    } else if (LATENCY == 1) {
      tmp = dsp_add_2simd_pipe_l1(ap_int<24>(a[0]), ap_int<24>(a[1]),
                                  ap_int<24>(b[0]), ap_int<24>(b[1]));
    } else if (LATENCY == 2) {
      tmp = dsp_add_2simd_pipe_l2(ap_int<24>(a[0]), ap_int<24>(a[1]),
                                  ap_int<24>(b[0]), ap_int<24>(b[1]));
    }
    dout[0] = tmp(47, 24);
    dout[1] = tmp(23, 0);
  } else {
    ap_int<24> tmp0, tmp1;
    if (LATENCY == 0) {
      dsp_add_2simd_seq_l0(ap_int<24>(a[0]), ap_int<24>(a[1]), ap_int<24>(b[0]),
                           ap_int<24>(b[1]), tmp0, tmp1);
    } else if (LATENCY == 1) {
      dsp_add_2simd_seq_l1(ap_int<24>(a[0]), ap_int<24>(a[1]), ap_int<24>(b[0]),
                           ap_int<24>(b[1]), tmp0, tmp1);
    } else if (LATENCY == 2) {
      dsp_add_2simd_seq_l2(ap_int<24>(a[0]), ap_int<24>(a[1]), ap_int<24>(b[0]),
                           ap_int<24>(b[1]), tmp0, tmp1);
    }
    dout[0] = tmp0;
    dout[1] = tmp1;
  }
}

#endif  // HLS4NM_DSP_ADD_SIMD_H_
