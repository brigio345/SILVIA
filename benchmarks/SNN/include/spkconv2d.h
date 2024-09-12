#ifndef SILVIA_ADDCONV2D_FN_INT_H_
#define SILVIA_ADDCONV2D_FN_INT_H_

#include "tensor.h"
#include "utils.h"
#include <ap_fixed.h>
#include <ap_int.h>
#include <hls_stream.h>

template <typename a_t, typename b_t> void brrr(a_t &a, b_t b) {
#pragma HLS inline off
#pragma HLS pipeline
#pragma HLS bind_op op = add impl = dsp variable = a
  a += b;
  return;
}

template <typename din_t, typename weight_t, int CI, int KSZ>
using spkconv2d_int_t = ap_int<weight_t::width + UpLog2<CI + KSZ * KSZ>::val>;

template <typename data_t, typename weight_t, unsigned HO, unsigned WO,
          unsigned CI, unsigned CO, unsigned KSZ, unsigned SIMD = 4>
void spkconv2d_int(
    const weight_t weights[CI][CO][KSZ * KSZ],
    hls::stream<Tensor2d<data_t, KSZ, KSZ>> &dinStream,
    hls::stream<Tensor1d<spkconv2d_int_t<data_t, weight_t, CI, KSZ>, SIMD>>
        &doutStream) {
  typedef spkconv2d_int_t<data_t, weight_t, CI, KSZ> _act_t;
#pragma HLS array_reshape variable = weights type = complete dim = 3
#pragma HLS array_partition variable = weights type = cyclic factor =          \
    SIMD dim = 2

  // Safety checks.
  static_assert(SIMD <= CO, "ERROR: SIMD must lower than or equal to CO.");
  static_assert(CO % SIMD == 0, "ERROR: CO must be divisible by SIMD.");

  _act_t spad[SIMD];
#pragma HLS array_partition variable = spad type = complete dim = 0
#pragma HLS bind_op variable = spad op = add impl = dsp
  Tensor1d<_act_t, SIMD> dout;
#pragma HLS array_partition variable = dout.data type = complete dim = 0

  for (auto window = 0; window < HO * WO; window++) {
  CONV2D_CO_LOOP:
    for (auto co = 0; co < CO / SIMD; co++) {
      // #pragma HLS pipeline II = CI
      // Resetting the scratchpad.
      for (auto i = 0; i < SIMD; i++) {
        spad[i] = 0;
      }

    CONV2D_ACC_CIN_LOOP:
      for (auto ci = 0; ci < CI; ci++) {
// #pragma HLS unroll factor = 1
#pragma HLS pipeline
        auto din = dinStream.read();
#pragma HLS array_reshape variable = din.data type = complete dim = 0
      CONV2D_ACC_FTS_LOOP:
        for (int h = 0; h < KSZ; h++) {
          for (int w = 0; w < KSZ; w++) {
            for (auto i = 0; i < SIMD; i++) {
#ifndef USE_BRRR
              spad[i] += din.data[h][KSZ - 1 - w] *
                         weights[ci][co * SIMD + i][h * KSZ + w];

#else
              brrr<_act_t, weight_t>(
                  spad[i], din.data[h][KSZ - 1 - w] *
                               weights[ci][co * SIMD + i][h * KSZ + w]);
#endif // USE_BRRR
            }
          }
        }
      }
      for (auto i = 0; i < SIMD; i++) {
        dout.data[i] = spad[i];
      }
      doutStream << dout;
    }
  }
}

#endif // SILVIA_ADDCONV2D_FN_INT_H_
