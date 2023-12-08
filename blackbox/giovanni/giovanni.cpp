#include "giovanni.h"

ap_int<48> giovanni(ap_int<24> a0, ap_int<24> a1, ap_int<24> b0,
                    ap_int<24> b1) {
  ap_int<48> dout;
  dout(47, 24) = ap_int<24>(a0 + b0);
  dout(23, 0) = ap_int<24>(a1 + b1);
  return dout;
}
