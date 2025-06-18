#ifndef HSL4NM_TB_CONV2D_DUT_H_
#define HSL4NM_TB_CONV2D_DUT_H_

#include "tensor.h"
#include <hls_stream.h>
#include "tb.h"

void dut(hls::stream<data_t>& dinStream,
         hls::stream<Tensor1d<act_t, SIMD>>& doutStream);

#endif  // HSL4NM_TB_CONV2D_DUT_H_
