#ifndef HLS4NM_TB_CONV2D_TB_H_
#define HLS4NM_TB_CONV2D_TB_H_

#include "ap_int.h"
#include "utils.h"

constexpr unsigned SEED = 123;
constexpr unsigned KSZ = 3;
constexpr unsigned HI = 24;
constexpr unsigned WI = 24;
constexpr unsigned STRD = 1;
constexpr unsigned PAD = 1;
constexpr unsigned SIMD = 16;
constexpr unsigned HO = (HI + 2 * PAD - KSZ) / STRD + 1;
constexpr unsigned WO = (WI + 2 * PAD - KSZ) / STRD + 1;
constexpr unsigned CO = 128;
constexpr unsigned CI = 64;
constexpr unsigned WEIGHT_W = 8;
constexpr unsigned DATA_W = 8;
constexpr char WEIGHTS_NAME[] = "weights";
constexpr char INPUTS_NAME[] = "inputs";

typedef bool data_t;
typedef ap_int<WEIGHT_W> weight_t;
typedef ap_int<weight_t::width + UpLog2<CI + KSZ * KSZ>::val> act_t;

#endif // HLS4NM_TB_CONV2D_TB_H_
