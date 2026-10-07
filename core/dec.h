// Emulation of System.Decimal (96-bit magnitude, base-10 scale 0..28), as used
// for every damage/block/power amount in the game. Semantics follow .NET's
// DecCalc: results are exact until they exceed 96 bits or scale 28, then round
// half-to-even; division yields up to 28-29 significant digits and then strips
// trailing zeros. Scale is observable (it affects later rounding), so it is
// tracked exactly, not normalized.
#pragma once

#include <compare>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace sts2 {

using u128 = unsigned __int128;

class Dec {
public:
    // Same layout as decimal.GetBits(): lo, mid, hi magnitude words; flags
    // holds the scale in bits 16-23 and the sign in bit 31.
    uint32_t lo = 0, mid = 0, hi = 0, flags = 0;

    static constexpr uint32_t kSignMask = 0x80000000u;
    static constexpr int kScaleShift = 16;
    static constexpr int kMaxScale = 28;

    constexpr Dec() = default;

    // Implicit, like C#'s int -> decimal conversion.
    constexpr Dec(int32_t v) : Dec(from_int64(v)) {}

    static constexpr Dec from_int64(int64_t v) {
        bool neg = v < 0;
        uint64_t m = neg ? (~static_cast<uint64_t>(v) + 1) : static_cast<uint64_t>(v);
        return make(m, 0, neg);
    }

    // Decimal literal: Dec::lit(15, 1) is 1.5m, Dec::lit(150, 2) is 1.50m.
    static constexpr Dec lit(int64_t mantissa, int scale) {
        bool neg = mantissa < 0;
        uint64_t m = neg ? (~static_cast<uint64_t>(mantissa) + 1) : static_cast<uint64_t>(mantissa);
        return make(m, scale, neg);
    }

    static constexpr Dec make(u128 mag, int scale, bool neg) {
        Dec d;
        d.lo = static_cast<uint32_t>(mag);
        d.mid = static_cast<uint32_t>(mag >> 32);
        d.hi = static_cast<uint32_t>(mag >> 64);
        d.flags = (static_cast<uint32_t>(scale) << kScaleShift) | (neg ? kSignMask : 0u);
        return d;
    }

    static constexpr Dec from_bits(uint32_t lo, uint32_t mid, uint32_t hi, uint32_t flags) {
        Dec d;
        d.lo = lo;
        d.mid = mid;
        d.hi = hi;
        d.flags = flags;
        return d;
    }

    // (decimal)float, DecCalc.VarDecFromR4 (7 significant digits).
    // decimal.MaxValue / MinValue
    static constexpr Dec max_value() { return from_bits(0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0); }
    static constexpr Dec min_value() { return from_bits(0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, kSignMask); }
    static Dec from_float(float f);

    constexpr u128 mag() const { return (static_cast<u128>(hi) << 64) | (static_cast<u128>(mid) << 32) | lo; }
    constexpr int scale() const { return static_cast<int>((flags >> kScaleShift) & 0xFF); }
    constexpr bool negative() const { return (flags & kSignMask) != 0; }
    constexpr bool is_zero() const { return (lo | mid | hi) == 0; }

    // (double)d and (float)d.
    double to_double() const;
    float to_float() const { return static_cast<float>(to_double()); }

    // (int)d and (long)d: truncate toward zero, throw std::overflow_error when out of range.
    int32_t to_int() const;
    int64_t to_int64() const;

    std::string to_string() const;

    // Bitwise identity (value and scale and sign), for golden tests.
    bool identical(const Dec& o) const { return lo == o.lo && mid == o.mid && hi == o.hi && flags == o.flags; }

    friend Dec operator+(const Dec& a, const Dec& b);
    friend Dec operator-(const Dec& a, const Dec& b);
    friend Dec operator*(const Dec& a, const Dec& b);
    friend Dec operator/(const Dec& a, const Dec& b);
    friend constexpr Dec operator-(const Dec& a) {
        Dec r = a;
        r.flags ^= kSignMask;
        return r;
    }

    Dec& operator+=(const Dec& o) { return *this = *this + o; }
    Dec& operator-=(const Dec& o) { return *this = *this - o; }
    Dec& operator*=(const Dec& o) { return *this = *this * o; }
    Dec& operator/=(const Dec& o) { return *this = *this / o; }

    // Value comparison (1.5m == 1.50m, -0 == 0), like decimal.CompareTo.
    friend int compare(const Dec& a, const Dec& b);
    friend bool operator==(const Dec& a, const Dec& b) { return compare(a, b) == 0; }
    friend std::strong_ordering operator<=>(const Dec& a, const Dec& b) { return compare(a, b) <=> 0; }
};

// Math.Truncate / Math.Floor / Math.Ceiling on decimal (result scale 0).
Dec dec_truncate(const Dec& d);
Dec dec_floor(const Dec& d);
Dec dec_ceiling(const Dec& d);

enum class Midpoint : uint8_t { ToEven, AwayFromZero };

// Math.Round(d, decimals, mode). Default is banker's rounding like C#.
Dec dec_round(const Dec& d, int decimals = 0, Midpoint mode = Midpoint::ToEven);

// Math.Max / Math.Min (decimal.Max / decimal.Min pick which operand survives on ties).
inline Dec dec_max(const Dec& a, const Dec& b) { return compare(a, b) >= 0 ? a : b; }
inline Dec dec_min(const Dec& a, const Dec& b) { return compare(a, b) < 0 ? a : b; }

}  // namespace sts2
