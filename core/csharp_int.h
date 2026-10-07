// Default unchecked C# int arithmetic, without C++ signed-overflow UB.
#pragma once
#include <bit>
#include <cstdint>
#include "engine/sim/errors.h"
namespace sts2 {
constexpr int32_t int_add(int32_t a, int32_t b) {
    return std::bit_cast<int32_t>(uint32_t(a) + uint32_t(b));
}
constexpr int32_t int_sub(int32_t a, int32_t b) {
    return std::bit_cast<int32_t>(uint32_t(a) - uint32_t(b));
}
constexpr int32_t int_mul(int32_t a, int32_t b) {
    return std::bit_cast<int32_t>(uint32_t(a) * uint32_t(b));
}
inline int32_t checked_int32(int64_t value) {
    if (value < INT32_MIN || value > INT32_MAX) throw GameError("original checked int32 arithmetic overflow");
    return static_cast<int32_t>(value);
}
inline int32_t checked_int_add(int32_t a, int32_t b) { return checked_int32(int64_t{a} + b); }
// A summarized List.Count/Enumerable.Count cannot wrap to a negative number.
inline int32_t list_count_add(int32_t count, int32_t amount = 1) {
    if (count < 0 || amount < 0 || amount > INT32_MAX - count)
        throw CapacityError("history count exceeds original int32 range");
    return count + amount;
}
} // namespace sts2
