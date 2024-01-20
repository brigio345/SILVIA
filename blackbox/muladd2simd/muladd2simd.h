#ifndef DOTPROD_X7_H_
#define DOTPROD_X7_H_

#include <ap_int.h>

ap_int<48> muladd2simd(ap_int<8> a0, ap_int<8> d0, ap_int<8> b0,
                      ap_int<48> partialIn);

#endif // DOTPROD_X7_H_
