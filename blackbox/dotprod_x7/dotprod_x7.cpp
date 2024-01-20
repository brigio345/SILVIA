#include "dotprod_x7.h"

ap_int<48> dotprod_x7(ap_int<8> a0, ap_int<8> a1, ap_int<8> a2, ap_int<8> a3,
                      ap_int<8> a4, ap_int<8> a5, ap_int<8> a6, ap_int<8> d0,
                      ap_int<8> d1, ap_int<8> d2, ap_int<8> d3, ap_int<8> d4,
                      ap_int<8> d5, ap_int<8> d6, ap_int<8> b0, ap_int<8> b1,
                      ap_int<8> b2, ap_int<8> b3, ap_int<8> b4, ap_int<8> b5,
                      ap_int<8> b6) {

  ap_int<27> a[7]{a0, a1, a2, a3, a4, a5, a6};
  ap_int<27> d[7]{d0, d1, d2, d3, d4, d5, d6};
  ap_int<18> b[7]{b0, b1, b2, b3, b4, b5, b6};

  ap_int<48> partials[8];
  partials[0] = 0;
  for (auto i = 0; i < 7; i++)
    partials[i + 1] = (ap_int<48>((ap_int<27>(a[i]) << 18) + ap_int<27>(d[i])) *
                       ap_int<18>(b[i])) +
                      partials[i];
  return partials[7];
}
