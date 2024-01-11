#include "dotprod.h"

ap_int<58> dotprod(ap_int<8> a0, ap_int<8> b0, ap_int<8> a1,
		ap_int<8> b1, ap_int<8> a2, ap_int<8> b2) {
#pragma HLS inline off
  return ((a0 * b0) + (a1 * b1) + (a2 * b2));
}
