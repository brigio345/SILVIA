#include "dut.h"
#include "main.h"
#include "xf_blas.hpp"
#include <hls_stream.h>

#ifndef __SYNTHESIS__
#ifdef DEBUG
#include <iostream>
using namespace std;
#endif // DEBUG
#endif // __SYNTHESIS__

void __dut(hls::stream<xf::blas::WideType<din_t, PAR>::t_TypeInt> &din0Stream,
           hls::stream<xf::blas::WideType<din_t, PAR>::t_TypeInt> &din1Stream,
           hls::stream<xf::blas::WideType<din_t, PAR>::t_TypeInt> &din2Stream,
           hls::stream<dout_t> &dout0Stream,
           hls::stream<dout_t> &dout1Stream) {
  dout_t dout0 = 0, dout1 = 0;
  for (auto i = 0; i < N / PAR; i++) {
#pragma HLS pipeline

    xf::blas::WideType<din_t, PAR> din0 = din0Stream.read();
    xf::blas::WideType<din_t, PAR> din1 = din1Stream.read();
    xf::blas::WideType<din_t, PAR> din2 = din2Stream.read();
#pragma HLS array_partition variable = din0 type = complete dim = 0
#pragma HLS array_partition variable = din1 type = complete dim = 0
#pragma HLS array_partition variable = din2 type = complete dim = 0

    for (auto j = 0; j < PAR; j++) {
      
      dout0 += din0[j] * din2[j];
      dout1 += din1[j] * din2[j];
    }

  }
  dout0Stream.write(dout0);
  dout1Stream.write(dout1);
  return;
}

void dut(din_t* din0, din_t* din1, din_t* din2, dout_t& dout0, dout_t& dout1) {
#pragma HLS interface mode=axis port=din0 depth=N
#pragma HLS interface mode=axis port=din1 depth=N
#pragma HLS interface mode=axis port=din2 depth=N
#pragma HLS interface mode=s_axilite port=return


#pragma HLS array_partition variable = din0 type = cyclic factor = PAR dim = 0
#pragma HLS array_partition variable = din1 type = cyclic factor = PAR dim = 0
#pragma HLS array_partition variable = din2 type = cyclic factor = PAR dim = 0

  hls::stream<xf::blas::WideType<din_t, PAR>::t_TypeInt> din0Stream(
      "din0Stream");
  hls::stream<xf::blas::WideType<din_t, PAR>::t_TypeInt> din1Stream(
      "din1Stream");
  hls::stream<xf::blas::WideType<din_t, PAR>::t_TypeInt> din2Stream(
      "din2Stream");
  hls::stream<dout_t> dout0Stream("dout0Stream");
  hls::stream<dout_t> dout1Stream("dout1Stream");
#pragma HLS dataflow
  xf::blas::readVec2Stream<din_t, PAR>(din0, N, din0Stream);
  xf::blas::readVec2Stream<din_t, PAR>(din1, N, din1Stream);
  xf::blas::readVec2Stream<din_t, PAR>(din2, N, din2Stream);
  __dut(din0Stream, din1Stream, din2Stream, dout0Stream, dout1Stream);
  dout0 = dout0Stream.read();
  dout1 = dout1Stream.read();
  return;
}
