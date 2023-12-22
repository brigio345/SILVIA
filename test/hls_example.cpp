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
//#include <iostream>

#include <ap_int.h>
#include <cstdint>
#include <iostream>
#include "../blackbox/dsp_add_simd.h"

const int N = 128;

//using namespace std;

extern "C" {
	void example(uint8_t a[N], uint8_t b[N], uint8_t c[N]) {
#pragma HLS INTERFACE m_axi port = a depth = N
#pragma HLS INTERFACE m_axi port = b depth = N
		for (int i = 0; i < N; i += 1) {
#pragma HLS PIPELINE II = 1
#pragma HLS UNROLL factor = 8
			c[i] = a[i] + b[i];
		}
		ap_int<48> tmp0 = 12;
		ap_int<48> tmp1 = 27;
		dsp_add_4simd_pipe_l0(tmp0, tmp1);
	}
}

int main() {
	uint8_t a[N], b[N], res[N], res_gold[N];
	for (int i = 0; i < N; ++i) {
		a[i] = i;
		b[i] = i % 17;
	}

	example(a, b, res);
	for (int i = 0; i < N; i++)
		res_gold[i] = a[i] + b[i];

	int mismatch = 0;
	for (int i = 0; i < N; i++) {
		mismatch += (res[i] != res_gold[i]);
		std::cout << i << ": " << unsigned(res_gold[i]) << " " << unsigned(res[i]);
		if (res[i] != res_gold[i])
			std::cout << " MISMATCH";
		std::cout << std::endl;
	}

	//cout << "Test passed.\n";
	return (mismatch > 0);
}
