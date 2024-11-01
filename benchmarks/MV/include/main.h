#ifndef VITIS_LIBRARIES_DOT_MAIN_H_
#define VITIS_LIBRARIES_DOT_MAIN_H_

#include <cstdint>
#include <ap_int.h>

constexpr int N = 192;
constexpr int PAR = 8;
constexpr int SEED = 123;

constexpr unsigned log2ceil(uint64_t value) {
    if (value >= 2) {
        uint64_t mask = 0x8000000000000000;
        unsigned result = 64;
        value = value - 1;

        while (mask != 0) {
            if (value & mask)
                return result;
            mask >>= 1;
            --result;
        }
    }
    return 0;
}

constexpr unsigned NBIT = 8;
typedef ap_int<NBIT> weight_t;

#ifdef BOOL_INPUTS
typedef bool din_t;
typedef ap_int<weight_t::width + log2ceil(uint64_t(N))> dout_t;
#else
typedef ap_int<NBIT> din_t;
typedef ap_int<din_t::width + weight_t::width + log2ceil(uint64_t(N))> dout_t;
#endif // BOOL_INPUTS

#endif // VITIS_LIBRARIES_DOT_MAIN_H_
