#include "dotprod_x7.h"

ap_int<48> dotprod_x7(ap_int<8> a0, ap_int<8> d0, ap_int<8> b0,
                      ap_int<48> partialIn) {
  // ap_int<27> _d0{d0};
  //_d0.range(7,0) = d0.range(7,0);
  // for (auto i=26; i >= 8; i--)
  //_d0.range(i,i) = d0.range(7,7);

  // ap_int<27> _a0{a0 << 18};
  // _a0.range(26,26) = a0.range(7,7);
  // _a0.range(25,18) = a0.range(7,0);

  // ap_int<18> _b0{b0};
  ap_int<27> _a0{a0};
  _a0 <<= 18;
  return (ap_int<48>((ap_int<27>(a0) << 18) + ap_int<27>(d0)) *
                    ap_int<18>(b0)) +
         partialIn;
}
