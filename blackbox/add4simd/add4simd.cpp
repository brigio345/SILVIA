#include "add4simd.h"

ap_int<48> add4simd(ap_int<48> a, ap_int<48> b) {
#pragma HLS inline off
  ap_int<48> dout;
  auto dout0 = ap_int<12>(a(47, 36)) + b(47, 36);
  auto dout1 = ap_int<12>(a(35, 24)) + b(35, 24);
  auto dout2 = ap_int<12>(a(23, 12)) + b(23, 12);
  auto dout3 = ap_int<12>(a(11, 0)) + b(11, 0);
  dout(47, 36) = dout0;
  dout(35, 24) = dout1;
  dout(23, 12) = dout2;
  dout(11, 0) = dout3;
  return dout;
}
