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

using namespace xf::blas;

void _dut(hls::stream<WideType<din_t, PAR>::t_TypeInt> &dinStream,
          hls::stream<WideType<weight_t, PAR * PAR>::t_TypeInt> &weightsStream,
          hls::stream<WideType<dout_t, PAR>::t_TypeInt> &doutStream) {

  WideType<dout_t, PAR> dout;
#pragma HLS array_partition variable = dout type = complete dim = 0
  // Rows of din.
  for (auto i = 0; i < N; i++) {
    // Rows of weights.
    for (auto j = 0; j < N / PAR; j++) {
      for (auto k = 0; k < PAR; k++) {
#pragma HLS unroll
        dout[k] = 0;
      }
      // Columns of weights.
      for (auto k = 0; k < N / PAR; k++) {
#pragma HLS pipeline
        WideType<din_t, PAR> din = dinStream.read();
        WideType<din_t, PAR *PAR> weights = weightsStream.read();
        for (auto l = 0; l < PAR; l++) {
          for (auto m = 0; m < PAR; m++) {
            dout[l] += din[m] * weights[l * PAR + m];
          }
        }
      }
      doutStream.write(dout);
    }
  }
}

void _bring_in(const din_t *din, const weight_t *weights, const dout_t *dout,
               din_t _din[N][N], weight_t _weights[N][N], dout_t _dout[N][N]) {
READ_DIN_IN_I:
  for (auto i = 0; i < N; i++) {
  READ_DIN_IN_J:
    for (auto j = 0; j < N; j++) {
      _din[i][j] = din[i * N + j];
      _weights[i][j] = weights[i * N + j];
      // _dout[i][j] = 0;
    }
  }
  return;
}

void _bring_out(dout_t _dout[N][N], dout_t *dout) {
  for (auto i = 0; i < N; i++) {
    for (auto j = 0; j < N; j++) {
      dout[i * N + j] = _dout[i][j];
    }
  }
}

void _write_data(dout_t _dout[N][N],
                 hls::stream<WideType<dout_t, PAR>::t_TypeInt> &doutStream) {
  for (auto i = 0; i < N; i++) {
    for (auto j = 0; j < N / PAR; j++) {
#pragma HLS pipeline
      WideType<dout_t, PAR> batch = doutStream.read();
#pragma HLS array_partition variable = batch type = complete dim = 0
      for (auto k = 0; k < PAR; k++) {
        _dout[i][j * PAR + k] = batch[k];
      }
    }
  }
  return;
}

void _read_data(din_t din[N][N],
                hls::stream<WideType<din_t, PAR>::t_TypeInt> &stream) {
  WideType<din_t, PAR> batch;
#pragma HLS array_reshape variable = batch type = complete dim = 0
  // Rows
  for (auto i = 0; i < N; i++) {
    // Columns
    for (auto jj = 0; jj < N / PAR; jj++) {
      for (auto j = 0; j < N / PAR; j++) {
#pragma HLS pipeline
        for (auto k = 0; k < PAR; k++) {
          batch[k] = din[i][j * PAR + k];
        }
        stream.write(batch);
      }
    }
  }
}

void _read_weights(
    weight_t weights[N][N],
    hls::stream<WideType<weight_t, PAR * PAR>::t_TypeInt> &stream) {
  WideType<weight_t, PAR * PAR> batch;
#pragma HLS array_partition variable = batch type = complete dim = 0
  // N times read out.
  for (auto i = 0; i < N; i++) {
    // Rows
    for (auto j = 0; j < N / PAR; j++) {
      // Columns
      for (auto k = 0; k < N / PAR; k++) {
#pragma HLS pipeline
        for (auto l = 0; l < PAR; l++) {
          for (auto m = 0; m < PAR; m++) {
            batch[l * PAR + m] = weights[j * PAR + l][k * PAR + m];
          }
        }
        stream.write(batch);
      }
    }
  }
}

void dut(din_t *din, weight_t *weights, dout_t *dout) {
#pragma HLS interface mode = axis port = din depth = N * N
#pragma HLS interface mode = axis port = weights depth = N * N
#pragma HLS interface mode = axis port = dout depth = N * N
#pragma HLS interface mode = s_axilite port = return

  din_t _din[N][N];
#pragma HLS array_reshape variable = _din type = cyclic factor = PAR dim = 2

  weight_t _weights[N][N];
#pragma HLS array_partition variable = _weights type = cyclic factor =         \
    PAR dim = 1
#pragma HLS array_reshape variable = _weights type = cyclic factor =         \
    PAR dim = 2

  dout_t _dout[N][N];
#pragma HLS array_partition variable = _dout type = cyclic factor = PAR dim = 2

  hls::stream<WideType<din_t, PAR>::t_TypeInt, PAR> dinStream;
  hls::stream<WideType<weight_t, PAR * PAR>::t_TypeInt, PAR> weightsStream;
  hls::stream<WideType<dout_t, PAR>::t_TypeInt, 4> doutStream("doutStream");
  _bring_in(din, weights, dout, _din, _weights, _dout);

#pragma HLS dataflow
  _read_data(_din, dinStream);
  _read_weights(_weights, weightsStream);
  _dut(dinStream, weightsStream, doutStream);
  _write_data(_dout, doutStream);
  _bring_out(_dout, dout);

  return;
}
