#include "doubleMAC.h"

ap_int<96> doubleMAC(ap_uint<1> ap_clk2x, ap_int<27> a0, ap_int<18> b0,
                         ap_int<48> c0, ap_int<27> a1, ap_int<18> b1,
                         ap_int<48> c1) {
#pragma HLS inline off 

  ap_int<96> dout;
  dout(95, 48) = ap_int<48>(ap_int<48>(a0 * b0) + c0);
  dout(47, 0) = ap_int<48>(ap_int<48>(a1 * b1) + c1);
  return dout;
}
