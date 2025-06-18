#include <iostream>

#include "dut.h"
#include "line_buffer.h"
#include "spkconv2d.h"
#include "tb.h"
#include "tensor.h"
#include "utils.h"
#include "weights.h"
#include <hls_stream.h>

void core(const weight_t weights[CI][CO][KSZ * KSZ],
          hls::stream<Tensor2d<data_t, KSZ, KSZ>> &s0,
          hls::stream<Tensor1d<act_t, SIMD>> &s1) {
  spkconv2d_int<data_t, weight_t, HO, WO, CI, CO, KSZ, SIMD>(weights, s0, s1);
}

void dut(hls::stream<data_t> &dinStream,
         hls::stream<Tensor1d<act_t, SIMD>> &doutStream) {
#pragma HLS dataflow

  static LineBuffer<data_t, HI, WI, CI, CO, KSZ, STRD, PAD, SIMD> lb;

  hls::stream<Tensor2d<data_t, KSZ, KSZ>> tmpStream("tmpStream");
  lb.slide(dinStream, tmpStream);
  core(WEIGHTS, tmpStream, doutStream);
  // cnn.convolve(tmpStream, doutStream);
}
