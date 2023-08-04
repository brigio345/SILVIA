// (C) Copyright 2016-2022 Xilinx, Inc.
// All Rights Reserved.
//
// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.
//
#include <iostream>

#include <ap_int.h>

constexpr size_t N = 128;

using namespace std;

void example(uint8_t a[N], uint8_t b[N]) {
#pragma HLS INTERFACE m_axi port = a depth = N
#pragma HLS INTERFACE m_axi port = b depth = N
  for (size_t i = 0; i < N; i += 4) {
#pragma HLS PIPELINE II = 1
//#pragma HLS UNROLL factor = 4
    const auto in0 = a[i + 0];
    const auto in1 = a[i + 1];
    const auto in2 = a[i + 2];
    const auto in3 = a[i + 3];
    const auto out0 = in0 + 1;
    const auto out1 = in1 + in0;
    const auto out2 = in2 + in0;
    const auto out3 = in3 + in0;
    b[i + 0] = out0;
    b[i + 1] = out1;
    b[i + 2] = out2;
    b[i + 3] = out3;
  }
}

int main() {
  uint8_t in[N], res[N];
  for (size_t i = 0; i < N; ++i) {
    in[i] = i;
  }

  example(in, res);

  int mismatch = 0;
  for (uint8_t i = 0; i < N; i += 4) {
  	if (res[i] != i + 1) {
        cout << "Expected=" << int(i + 1) << " Got=" << int(res[i]) << endl;
        mismatch++;
	}
    for (int j = 1; j < 4; j++) {
      if (res[i + j] != i + j + i) {
        //cout << "Expected=" << i + j + (j > 0 ? i : 1) << " Got=" << res[i] << endl;
        cout << "Expected=" << int(i + j + i) << " Got=" << int(res[i + j]) << endl;
        mismatch++;
      }
    }
  }

  cout << "Test passed.\n";
  return mismatch;
}
