#ifndef SILVIA_LINE_BUFFER_H_
#define SILVIA_LINE_BUFFER_H_

#include "tensor.h"
#include "utils.h"
#include <ap_shift_reg.h>
#include <hls_stream.h>

template <typename T, unsigned HI, unsigned WI, unsigned CI, unsigned CO,
          unsigned KSZ, unsigned STRD, unsigned PAD, unsigned SIMD = 1>
class LineBuffer {
  static_assert(STRD < KSZ, "ERROR: STRD must be lower than kernel size.");
  static_assert(PAD == KSZ / 2 || PAD == 0,
                "ERROR: padding must be equal to either zero or half of the "
                "kernel size.");

  static constexpr unsigned _FTS = KSZ * KSZ;
  static constexpr unsigned _HO = (HI + 2 * PAD - KSZ) / STRD + 1;
  static constexpr unsigned _WO = (WI + 2 * PAD - KSZ) / STRD + 1;
  static constexpr unsigned _CO_SIMD = CO / SIMD;

public:
  LineBuffer(void) {
    // #pragma HLS array_partition variable = _buff type = complete dim = 2
    // #pragma HLS array_partition variable = _window type = complete dim = 2
  }

  void init(hls::stream<T> &dinStream);
  void slide(hls::stream<T> &dinStream,
             hls::stream<Tensor2d<T, KSZ, KSZ>> &doutStream);

private:
  ap_shift_reg<T, WI> _buff[CI][KSZ - 1];
  ap_shift_reg<T, KSZ> _window[CI][KSZ];

};

template <typename T, unsigned HI, unsigned WI, unsigned CI, unsigned CO,
          unsigned KSZ, unsigned STRD, unsigned PAD, unsigned SIMD>
void LineBuffer<T, HI, WI, CI, CO, KSZ, STRD, PAD, SIMD>::init(
    hls::stream<T> &dinStream) {
#pragma HLS inline

  // Load row padding in the line buffer.
  for (auto h = 0; h < PAD; h++) {
    for (int w = 0; w < WI; w++) {
      for (int ci = 0; ci < CI; ci++) {
#pragma HLS pipeline II = 1
        _buff[ci][h].shift(0);
      }
    }
  }

  // Load data.
  for (auto h = PAD; h < KSZ - 1; h++) {
    for (int w = 0; w < WI; w++) {
      for (int ci = 0; ci < CI; ci++) {
#pragma HLS pipeline II = 1
        _buff[ci][h].shift(dinStream.read());
      }
    }
  }
}

template <typename T, unsigned HI, unsigned WI, unsigned CI, unsigned CO,
          unsigned KSZ, unsigned STRD, unsigned PAD, unsigned SIMD>
void LineBuffer<T, HI, WI, CI, CO, KSZ, STRD, PAD, SIMD>::slide(
    hls::stream<T> &dinStream,
    hls::stream<Tensor2d<T, KSZ, KSZ>> &doutStream) {
  init(dinStream);
  for (auto ho = 0; ho < _HO; ho++) {
    for (auto wo = 0; wo < _WO; wo++) {
      if (wo == 0) {
        // First window.
        for (auto w = 0; w < KSZ; w++) {
          for (auto ci = 0; ci < CI; ci++) {
#pragma HLS pipeline II = 1
            if (w < PAD) {
              // Inserting the vertical padding.
              for (auto h = 0; h < KSZ; h++) {
                _window[ci][h].shift(0);
              }
            } else {
              // Loading the buffer heads to the window.
              // Special case: last convolution line and we do not have
              // padding as last line.
              if (ho < _HO - 1 || ((_HO - 1) * STRD + KSZ <= HI + PAD)) {
                auto din = dinStream.read();
                for (auto h = 0; h < KSZ - 1; h++) {
                  _window[ci][h].shift(_buff[ci][h].read(WI - 1));
                }
                _window[ci][KSZ - 1].shift(din);

                // Sliding the buffer.
                for (auto h = 0; h < KSZ - 1; h++) {
                  _buff[ci][h].shift(
                      (h + 1 < KSZ - 1) ? _buff[ci][h + 1].read(WI - 1) : din);
                }
              } else {
                // Sliding in padding as last line.
                for (auto h = 0; h < KSZ - 1; h++) {
                  _window[ci][h].shift(_buff[ci][h].read(WI - 1));
                }
                _window[ci][KSZ - 1].shift(0);

                // Sliding the buffer.
                for (auto h = 0; h < KSZ - 1; h++) {
                  _buff[ci][h].shift(
                      h + 1 < KSZ - 1 ? _buff[ci][h + 1].read(WI - 1) : T(0));
                }
              }
            }
          }
        }
        // }
      } else if (wo < _WO - 1) {
        for (auto s = 0; s < STRD; s++) {
          for (auto ci = 0; ci < CI; ci++) {
#pragma HLS pipeline II = 1
            // Loading the buffer heads to the window.
            // Special case: last convolution line and we do not have padding
            // as last line.
            if (ho < _HO - 1 || ((_HO - 1) * STRD + KSZ <= HI + PAD)) {
              auto din = dinStream.read();
              for (auto h = 0; h < KSZ - 1; h++) {
                _window[ci][h].shift(_buff[ci][h].read(WI - 1));
              }
              _window[ci][KSZ - 1].shift(din);

              // Sliding the buffer.
              for (auto h = 0; h < KSZ - 1; h++) {
                _buff[ci][h].shift(
                    h + 1 < KSZ - 1 ? _buff[ci][h + 1].read(WI - 1) : din);
              }
            } else {
              // Sliding in padding as last line.
              for (auto h = 0; h < KSZ - 1; h++) {
                _window[ci][h].shift(_buff[ci][h].read(WI - 1));
              }
              _window[ci][KSZ - 1].shift(0);

              // Sliding the buffer.
              for (auto h = 0; h < KSZ - 1; h++) {
                _buff[ci][h].shift(
                    h + 1 < KSZ - 1 ? _buff[ci][h + 1].read(WI - 1) : T(0));
              }
            }
          }
        }
      } else {
        for (auto s = 0; s < STRD; s++) {
          for (auto ci = 0; ci < CI; ci++) {
#pragma HLS pipeline II = 1
            // Loading the buffer heads to the window.
            // Special case: last convolution line and we do not have padding
            // as last line.
            if (s < STRD - 1 ||
                (wo == _WO - 1 && ((_WO - 1) * STRD + KSZ <= WI + PAD))) {
              if (ho < _HO - 1 || ((_HO - 1) * STRD + KSZ <= HI + PAD)) {
                auto din = dinStream.read();
                for (auto h = 0; h < KSZ - 1; h++) {
                  _window[ci][h].shift(_buff[ci][h].read(WI - 1));
                }
                _window[ci][KSZ - 1].shift(din);

                // Sliding the buffer.
                for (auto h = 0; h < KSZ - 1; h++) {
                  _buff[ci][h].shift(
                      h + 1 < KSZ - 1 ? _buff[ci][h + 1].read(WI - 1) : din);
                }
              } else {
                // Sliding in padding as last line.
                for (auto h = 0; h < KSZ - 1; h++) {
                  _window[ci][h].shift(_buff[ci][h].read(WI - 1));
                }
                _window[ci][KSZ - 1].shift(0);

                // Sliding the buffer.
                for (auto h = 0; h < KSZ - 1; h++) {
                  _buff[ci][h].shift(
                      h + 1 < KSZ - 1 ? _buff[ci][h + 1].read(WI - 1) : T(0));
                }
              }
            } else {
              for (auto h = 0; h < KSZ; h++) {
                _window[ci][h].shift(0);
              }
            }
          }
        }
      }
      // Sending out the windows.
      for (auto co = 0; co < _CO_SIMD; co++) {
        for (auto ci = 0; ci < CI; ci++) {
#pragma HLS pipeline II = 1
          Tensor2d<T, KSZ, KSZ> dout;
          for (auto i = 0; i < KSZ; i++) {
            for (auto j = 0; j < KSZ; j++) {
              dout.data[i][j] = _window[ci][i].read(j);
            }
          }
          doutStream << dout;
        }
      }
    }
    if (ho < _HO - 1 || ((_HO - 1) * STRD + KSZ <= HI + PAD &&
                         ((_WO - 1) * STRD + KSZ > WI + PAD))) {
      for (auto s = 0; s < STRD - 1; s++) {
        for (auto i = 0; i < WI; i++) {
          for (auto ci = 0; ci < CI; ci++) {
#pragma HLS pipeline II = 1
            auto din = dinStream.read();
            // Sliding the buffer.
            for (auto h = 0; h < KSZ - 1; h++) {
              _buff[ci][h].shift(h + 1 < KSZ - 1 ? _buff[ci][h + 1].read(WI - 1)
                                                 : din);
            }
          }
        }
      }
    }
  }
}

#endif // SILVIA_LINE_BUFFER_H_
