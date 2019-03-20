#ifndef RX_FOUNDATION_TYPES_H
#define RX_FOUNDATION_TYPES_H

#include <rx/core/traits/conditional.h>

using rx_size = decltype(sizeof 0);
using rx_byte = unsigned char;
using rx_s8 = signed char;
using rx_u8 = unsigned char;
using rx_s16 = signed short;
using rx_u16 = unsigned short;
using rx_s32 = signed int;
using rx_u32 = unsigned int;
using rx_s64 = rx::traits::conditional<sizeof(signed long) == 8, signed long, signed long long>;
using rx_u64 = rx::traits::conditional<sizeof(unsigned long) == 8, unsigned long, unsigned long long>;
using rx_f32 = float;
using rx_f64 = double;
using rx_ptrdiff = long;
using rx_uintptr = rx_size;

constexpr rx_size operator"" _z(unsigned long long value) {
  return static_cast<rx_size>(value);
}

#endif // RX_FOUNDATION_TYPES_H
