#ifndef DOUBLE_MUL_ADD_H_
#define DOUBLE_MUL_ADD_H_

#include <ap_int.h>

ap_int<96> doubleMulAdd(ap_uint<1> ap_clk2x, ap_int<27> a0, ap_int<18> b0,
                        ap_int<48> c0, ap_int<27> a1, ap_int<18> b1,
                        ap_int<48> c1);

#endif // DOUBLE_MUL_ADD_H_
