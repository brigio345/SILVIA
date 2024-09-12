#ifndef SILVIA_UTILS_H_
#define SILVIA_UTILS_H_

#include <type_traits>
#include <ap_fixed.h>

// Returns ceil(log2(N)).
template <unsigned N, unsigned RES = 0> struct UpLog2 {
  enum { val = UpLog2<N / 2 + N % 2, RES + 1>::val };
};

template <unsigned RES> struct UpLog2<1, RES> {
  enum { val = RES };
};

template <typename acc_t, typename din_t, int MIN>
acc_t floor_acc(acc_t acc, din_t din) {
#pragma HLS inline
  acc_t tmp = acc + din;
  if (tmp < MIN) {
    return MIN;
  }
  return tmp;
}

// A MAC unit used in the LIF module.
template <typename acc_t, typename data_t>
void my_mac(acc_t &acc, data_t a, data_t b) {
#pragma HLS inline
  acc += a * b;
}

// Checking if a type is ap_fixed or not.
template <typename T, typename = void> struct is_ap_fx : std::false_type {};

#if __cplusplus < 201402L

template <bool B, class T = void>
using enable_if_t = typename std::enable_if<B, T>::type;

template <typename T>
struct is_ap_fx<
    T, enable_if_t<std::is_same<
           ap_fixed<T::width, T::iwidth, T::qmode, T::omode>, T>::value>>
    : std::true_type {};

#else 

template <typename T>
struct is_ap_fx<
    T, std::enable_if_t<std::is_same<
           ap_fixed<T::width, T::iwidth, T::qmode, T::omode>, T>::value>>
    : std::true_type {};

#endif // __cplusplus
 
#endif // SILVIA_UTILS_H_
