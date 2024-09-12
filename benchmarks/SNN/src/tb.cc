#include "tb.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "dut.h"
#include "inputs.h"
#include "tensor.h"
#include "weights.h"
#include <hls_stream.h>

using namespace std;
// using namespace hls4nm;

int main(void) {
  constexpr unsigned FTS = KSZ * KSZ;

  hls::stream<data_t> dinStream("dinStream");
  hls::stream<Tensor1d<act_t, SIMD>> doutStream("doutStream");

  act_t actDUT[HO][WO][CO];

  int actGolden[HO][WO][CO];
  data_t padded_inputs[HI + 2 * PAD][WI + 2 * PAD][CI];
  for (auto h = 0; h < HO; h++) {
    for (auto w = 0; w < WO; w++) {
      for (auto c = 0; c < CO; c++) {
        actGolden[h][w][c] = 0;
      }
    }
  }

  // Generating a padded version of the inputs.
  for (auto h = 0; h < HI + 2 * PAD; h++) {
    for (auto w = 0; w < WI + 2 * PAD; w++) {
      for (auto c = 0; c < CI; c++) {
        padded_inputs[h][w][c] =
            (h < PAD || h >= HI + PAD || w < PAD || w >= WI + PAD)
                ? data_t(0)
                : INPUTS[h - PAD][w - PAD][c];
      }
    }
  }

  for (auto h = 0; h < HO; h++) {
    for (auto w = 0; w < WO; w++) {
      for (auto hk = 0; hk < KSZ; hk++) {
        for (auto wk = 0; wk < KSZ; wk++) {
          auto ft = hk * KSZ + wk;
          auto hi = h * STRD + hk;
          auto wi = w * STRD + wk;
          for (auto ci = 0; ci < CI; ci++) {
            for (auto co = 0; co < CO; co++) {
              actGolden[h][w][co] +=
                  int(padded_inputs[hi][wi][ci]) * int(WEIGHTS[ci][co][ft]);
            }
          }
        }
      }
    }
  }
  for (auto hi = 0; hi < HI; hi++) {
    for (auto wi = 0; wi < WI; wi++) {
      for (auto ci = 0; ci < CI; ci++) {
        dinStream << INPUTS[hi][wi][ci];
      }
    }
  }

  dut(dinStream, doutStream);

  for (auto h = 0; h < HO; h++) {
    for (auto w = 0; w < WO; w++) {
      for (auto co = 0; co < CO / SIMD; co++) {
        auto act = doutStream.read();
        for (auto i = 0; i < SIMD; i++) {
          actDUT[h][w][co * SIMD + i] = act.data[i];
        }
      }
    }
  }

  bool pass = true;
  for (auto h = 0; h < HO; h++) {
    for (auto w = 0; w < WO; w++) {
      for (auto co = 0; co < CO && pass; co++) {
        if (actGolden[h][w][co] != actDUT[h][w][co]) {
          cerr << "ERROR!!!\n";
          cerr << "actDUT[" << h << "][" << w << "][" << co
               << "] = " << int(actDUT[h][w][co]) << "." << endl;
          cerr << "actGolden[" << h << "][" << w << "][" << co
               << "] = " << int(actGolden[h][w][co]) << "." << endl;
          pass = false;
        }
      }
    }
  }

  if (pass) {
    cout << '\n'
         << string(50, '*') << '\n'
         << "PASSED!" << '\n'
         << string(50, '*') << '\n'
         << endl;
  } else {
    cerr << '\n'
         << string(50, '*') << '\n'
         << "FAILED!" << '\n'
         << string(50, '*') << '\n'
         << endl;
  }

  return pass ? 0 : EXIT_FAILURE;
}
