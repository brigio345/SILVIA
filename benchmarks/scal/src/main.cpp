/*
 * Copyright 2019 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef UUT_TOP_H
#define UUT_TOP_H

#include "ap_int.h"
#include "hls_stream.h"
#include "xf_blas.hpp"

using namespace xf::blas;

constexpr auto BLAS_vectorSize = 512;
constexpr auto BLAS_parEntries = 1 << 6;
typedef int8_t BLAS_dataType;
typedef int8_t BLAS_resDataType;


void dut(uint32_t p_n,
             BLAS_dataType p_alpha,
             BLAS_dataType p_x[BLAS_vectorSize],
             BLAS_dataType p_y[BLAS_vectorSize],
             BLAS_dataType p_xRes[BLAS_vectorSize],
             BLAS_dataType p_yRes[BLAS_vectorSize],
             BLAS_resDataType& p_goldRes) {
#pragma HLS interface mode = ap_memory port = p_x
#pragma HLS interface mode = ap_memory port = p_y
#pragma HLS interface mode = ap_memory port = p_xRes
#pragma HLS interface mode = ap_memory port = p_yRes

#pragma HLS array_partition type = block  factor = (BLAS_parEntries) dim = 0 variable = p_x
// #pragma HLS array_partition type = block  factor = (BLAS_parEntries) dim = 0 variable = p_y
#pragma HLS array_partition type = block  factor = (BLAS_parEntries) dim = 0 variable = p_xRes
// #pragma HLS array_partition type = block  factor = (BLAS_parEntries) dim = 0 variable = p_yRes

    hls::stream<typename WideType<BLAS_dataType, BLAS_parEntries>::t_TypeInt> l_strX;
    hls::stream<typename WideType<BLAS_dataType, BLAS_parEntries>::t_TypeInt> l_strR;
#pragma HLS DATAFLOW
    readVec2Stream<BLAS_dataType, BLAS_parEntries>(p_x, p_n, l_strX);
    scal<BLAS_dataType, BLAS_parEntries>(p_n, p_alpha, l_strX, l_strR);
    writeStream2Vec<BLAS_dataType, BLAS_parEntries>(l_strR, p_n, p_xRes);
}

#endif
