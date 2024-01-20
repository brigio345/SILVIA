#include "dotprod_x7.h"

ap_int<48> dotprod_x7(ap_int<8> a0, ap_int<8> d0, ap_int<8> b0,
                      ap_int<48> partialIn) {
  return ap_int<48>((a0 + d0) * b0) + partialIn;
}
