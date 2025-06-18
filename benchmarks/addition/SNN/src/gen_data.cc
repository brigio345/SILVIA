#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

using std::endl;
using std::ofstream;
using std::string;
using std::transform;

#include "tb.h"

void gen_weights(const string &fname) {
  ofstream ofs(fname + ".h");
  string fnameUpper = fname;
  transform(fnameUpper.begin(), fnameUpper.end(), fnameUpper.begin(), toupper);

  // Header guards.
  ofs << "#ifndef HLS4NM_TB_DATA_" + fnameUpper + "_H_\n"
      << "#define "
      << "HLS4NM_TB_DATA_" + fnameUpper + "_H_\n"
      << endl;

  // Putting in dependencies.
  ofs << "#include \"tb.h\"" << endl;

  ofs << "const weight_t " << fnameUpper << "[" << CI
      << "][" << CO << "][" << KSZ * KSZ << "] = {" << endl;
  weight_t weights[CI][CO][KSZ * KSZ];
  for (int ci = 0; ci < CI; ci++) {
    ofs << "{";
    for (int co = 0; co < CO; co++) {
      ofs << "{";
      for (int k = 0; k < KSZ * KSZ; k++) {
        weights[ci][co][k] = rand() % (1 << WEIGHT_W) - (1 << (WEIGHT_W-1));
        ofs << int(weights[ci][co][k])
            << (k < KSZ * KSZ - 1
                    ? ", "
                    : (co < CO - 1 ? "},"
                                   : (ci < CI - 1 ? "}}," : "}}};\n")));
      }
    }
  }
  ofs << std::endl;

  // Closing the header guard.
  ofs << "#endif // HLS4NM_TB_DATA_" + fnameUpper + "_H_ " << endl;
  ofs.close();
}

void gen_inputs(const string &fname) {
  ofstream ofs(fname + ".h");
  string fnameUpper = fname;
  transform(fnameUpper.begin(), fnameUpper.end(), fnameUpper.begin(), toupper);

  // Header guards.
  ofs << "#ifndef HLS4NM_TB_DATA_" + fnameUpper + "_H_\n"
      << "#define "
      << "HLS4NM_TB_DATA_" + fnameUpper + "_H_\n"
      << endl;

  // Putting in dependencies.
  ofs << "#include \"tb.h\"" << endl;

  // Defining the array.
  ofs << "const data_t " << fnameUpper << "[" << HI << "][" << WI << "][" << CI
      << "] = {" << endl;
  bool inputs[HI][WI][CI];
  for (int h = 0; h < HI; h++) {
    ofs << "{";
    for (int w = 0; w < WI; w++) {
      ofs << "{";
      for (int ci = 0; ci < CI; ci++) {
        // inputs[h][w][ci] = rand() % (1 << DATA_W) - (1 << (DATA_W-1));
        inputs[h][w][ci] = rand() % 2 == 0;
        ofs << unsigned(inputs[h][w][ci])
            << (ci < CI - 1
                    ? ", "
                    : (w < WI - 1 ? "}," : (h < HI - 1 ? "}}," : "}}};\n")));
      }
    }
  }
  ofs << std::endl;

  // Closing the header guard.
  ofs << "#endif // HLS4NM_TB_DATA_" + fnameUpper + "_H_ " << endl;
  ofs.close();
}

int main(void) {
  srand(SEED);
  gen_weights(WEIGHTS_NAME);
  gen_inputs(INPUTS_NAME);
  return 0;
}
