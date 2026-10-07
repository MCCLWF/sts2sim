#include "core/dec.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace sts2 {

namespace {

constexpr u128 kTwo96 = static_cast<u128>(1) << 96;

constexpr uint64_t kPow10U64[20] = {
    1ULL,
    10ULL,
    100ULL,
    1000ULL,
    10000ULL,
    100000ULL,
    1000000ULL,
    10000000ULL,
    100000000ULL,
    1000000000ULL,
    10000000000ULL,
    100000000000ULL,
    1000000000000ULL,
    10000000000000ULL,
    100000000000000ULL,
    1000000000000000ULL,
    10000000000000000ULL,
    100000000000000000ULL,
    1000000000000000000ULL,
    10000000000000000000ULL,
};

// DecCalc.s_doublePowers10 (as far as a decimal scale can reach, plus the
// range VarDecFromR4 uses).
constexpr double kDoublePow10[] = {
    1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17,
    1e18, 1e19, 1e20, 1e21, 1e22, 1e23, 1e24, 1e25, 1e26, 1e27, 1e28, 1e29, 1e30, 1e31, 1e32, 1e33, 1e34, 1e35,
    1e36, 1e37, 1e38, 1e39, 1e40,
};

[[noreturn]] void throw_overflow() { throw std::overflow_error("Value was either too large or too small for a Decimal."); }

// Minimal fixed-width unsigned 256-bit integer, enough for aligned sums and
// full 96x96-bit products before rounding.
struct U256 {
    uint64_t w[4] = {0, 0, 0, 0};

    static U256 from(u128 v) {
        U256 r;
        r.w[0] = static_cast<uint64_t>(v);
        r.w[1] = static_cast<uint64_t>(v >> 64);
        return r;
    }

    bool fits96() const { return w[3] == 0 && w[2] == 0 && (w[1] >> 32) == 0; }
    bool fits128() const { return w[3] == 0 && w[2] == 0; }
    u128 low128() const { return (static_cast<u128>(w[1]) << 64) | w[0]; }
};

int cmp(const U256& a, const U256& b) {
    for (int i = 3; i >= 0; --i) {
        if (a.w[i] != b.w[i]) return a.w[i] < b.w[i] ? -1 : 1;
    }
    return 0;
}

U256 add(const U256& a, const U256& b) {
    U256 r;
    u128 carry = 0;
    for (int i = 0; i < 4; ++i) {
        u128 s = static_cast<u128>(a.w[i]) + b.w[i] + carry;
        r.w[i] = static_cast<uint64_t>(s);
        carry = s >> 64;
    }
    return r;
}

// a - b, requires a >= b.
U256 sub(const U256& a, const U256& b) {
    U256 r;
    uint64_t borrow = 0;
    for (int i = 0; i < 4; ++i) {
        uint64_t x = a.w[i], y = b.w[i];
        uint64_t d = x - y - borrow;
        borrow = (x < y || (x == y && borrow)) ? 1 : 0;
        r.w[i] = d;
    }
    return r;
}

U256 mul_u64(const U256& a, uint64_t m) {
    U256 r;
    u128 carry = 0;
    for (int i = 0; i < 4; ++i) {
        u128 p = static_cast<u128>(a.w[i]) * m + carry;
        r.w[i] = static_cast<uint64_t>(p);
        carry = p >> 64;
    }
    return r;
}

U256 mul_pow10(U256 a, int k) {
    while (k > 0) {
        int c = std::min(k, 19);
        a = mul_u64(a, kPow10U64[c]);
        k -= c;
    }
    return a;
}

U256 mul_128(u128 a, u128 b) {
    uint64_t a0 = static_cast<uint64_t>(a), a1 = static_cast<uint64_t>(a >> 64);
    uint64_t b0 = static_cast<uint64_t>(b), b1 = static_cast<uint64_t>(b >> 64);
    U256 r = U256::from(static_cast<u128>(a0) * b0);
    U256 t = U256::from(static_cast<u128>(a0) * b1);
    t.w[3] = t.w[2];
    t.w[2] = t.w[1];
    t.w[1] = t.w[0];
    t.w[0] = 0;
    r = add(r, t);
    t = U256::from(static_cast<u128>(a1) * b0);
    t.w[3] = t.w[2];
    t.w[2] = t.w[1];
    t.w[1] = t.w[0];
    t.w[0] = 0;
    r = add(r, t);
    t = U256::from(static_cast<u128>(a1) * b1);
    t.w[3] = t.w[1];
    t.w[2] = t.w[0];
    t.w[1] = 0;
    t.w[0] = 0;
    return add(r, t);
}

// In-place a /= d, returns remainder.
uint64_t divmod_u64(U256& a, uint64_t d) {
    u128 rem = 0;
    for (int i = 3; i >= 0; --i) {
        u128 cur = (rem << 64) | a.w[i];
        a.w[i] = static_cast<uint64_t>(cur / d);
        rem = cur % d;
    }
    return static_cast<uint64_t>(rem);
}

struct Mag {
    u128 mag;
    int scale;
};

// DecCalc.ScaleResult: drop the fewest decimal digits so that the magnitude
// fits in 96 bits and the scale is <= 28, rounding half to even with a sticky
// bit for all dropped digits.
Mag scale_result(U256 v, int scale) {
    int k = 0;
    if (!v.fits96()) {
        U256 limit = U256::from(kTwo96);
        while (cmp(v, limit) >= 0) {
            limit = mul_u64(limit, 10);
            ++k;
        }
    }
    if (k < scale - Dec::kMaxScale) k = scale - Dec::kMaxScale;
    if (k > scale) throw_overflow();
    if (k == 0) return {v.low128(), scale};

    scale -= k;
    bool sticky = false;
    uint64_t last_rem = 0, last_pow = 1;
    for (int left = k; left > 0;) {
        int c = std::min(left, 19);
        sticky |= last_rem != 0;
        last_pow = kPow10U64[c];
        last_rem = divmod_u64(v, last_pow);
        left -= c;
    }
    u128 q = v.low128();
    uint64_t half = last_pow >> 1;
    if (last_rem > half || (last_rem == half && (sticky || (q & 1) != 0))) {
        ++q;
        if (q == kTwo96) {
            // Rounding carried past 96 bits: drop one more digit.
            if (scale == 0) throw_overflow();
            --scale;
            u128 nq = q / 10;
            unsigned r = static_cast<unsigned>(q % 10);
            if (r > 5 || (r == 5 && (nq & 1) != 0)) ++nq;
            q = nq;
        }
    }
    return {q, scale};
}

Dec add_signed(const Dec& a, const Dec& b, bool negate_b) {
    bool an = a.negative();
    bool bn = b.negative() != negate_b;
    int as = a.scale(), bs = b.scale();
    int s = std::max(as, bs);
    U256 am = U256::from(a.mag()), bm = U256::from(b.mag());
    if (as < s) am = mul_pow10(am, s - as);
    if (bs < s) bm = mul_pow10(bm, s - bs);

    U256 r;
    bool rn;
    if (an == bn) {
        r = add(am, bm);
        rn = an;
    } else {
        int c = cmp(am, bm);
        if (c > 0) {
            r = sub(am, bm);
            rn = an;
        } else if (c < 0) {
            r = sub(bm, am);
            rn = bn;
        } else {
            // Exact zero; only its sign bit is at stake. VarDecAdd subtracts from
            // the operand it had to rescale (the smaller scale; a on ties) and
            // flips on borrow, but returns the other operand unchanged when the
            // rescaled one is zero.
            r = U256{};
            if (as == bs) {
                rn = an;
            } else {
                bool a_rescaled = as < bs;
                bool rescaled_zero = a_rescaled ? a.is_zero() : b.is_zero();
                rn = (a_rescaled != rescaled_zero) ? an : bn;
            }
        }
    }
    Mag m = scale_result(r, s);
    return Dec::make(m.mag, m.scale, rn);
}

// DecCalc.SearchScale: the largest power of ten (<= 9, and not pushing the
// scale past 28) the quotient can be multiplied by without leaving 96 bits.
int search_scale(u128 q, int scale) {
    int max_cur = std::min(9, Dec::kMaxScale - scale);
    int cur = 0;
    for (int c = max_cur; c > 0; --c) {
        if (q <= (kTwo96 - 1) / kPow10U64[c]) {
            cur = c;
            break;
        }
    }
    if (cur < 9 && cur + scale < 0) throw_overflow();
    return cur;
}

// DecCalc.OverflowUnscale: the quotient carried into bit 96; divide by 10.
int overflow_unscale(u128& q, int scale, bool sticky) {
    if (--scale < 0) throw_overflow();
    u128 nq = q / 10;
    unsigned r = static_cast<unsigned>(q % 10);
    if (r > 5 || (r == 5 && (sticky || (nq & 1) != 0))) ++nq;
    q = nq;
    return scale;
}

u128 pow10_u128(int k) {
    u128 r = 1;
    for (int i = 0; i < k; ++i) r *= 10;
    return r;
}

}  // namespace

Dec operator+(const Dec& a, const Dec& b) { return add_signed(a, b, false); }

Dec operator-(const Dec& a, const Dec& b) { return add_signed(a, b, true); }

Dec operator*(const Dec& a, const Dec& b) {
    bool rn = a.negative() != b.negative();
    int s = a.scale() + b.scale();
    u128 am = a.mag(), bm = b.mag();
    // VarDecMul returns a plain zero (scale 0, positive) when its 32x32-bit
    // fast path has a scale too large for any digit to survive, and when the
    // wide path's raw product is zero. The fast path otherwise keeps the scale
    // and sign of a zero product.
    bool fast_path = (am >> 32) == 0 && (bm >> 32) == 0;
    if (fast_path ? s > Dec::kMaxScale + 19 : (am == 0 || bm == 0)) return Dec{};
    Mag m = scale_result(mul_128(am, bm), s);
    return Dec::make(m.mag, m.scale, rn);
}

Dec operator/(const Dec& a, const Dec& b) {
    u128 d = b.mag();
    if (d == 0) throw std::domain_error("Attempted to divide by zero.");
    int scale = a.scale() - b.scale();
    u128 q = a.mag() / d;
    u128 r = a.mag() % d;
    bool unscale = false;

    for (;;) {
        int cur;
        if (r == 0) {
            if (scale >= 0) break;
            cur = std::min(9, -scale);
        } else {
            unscale = true;
            if (scale == Dec::kMaxScale || (cur = search_scale(q, scale)) == 0) {
                u128 r2 = r << 1;
                if (r2 > d || (r2 == d && (q & 1) != 0)) {
                    ++q;
                    if (q == kTwo96) scale = overflow_unscale(q, scale, true);
                }
                break;
            }
        }
        uint64_t power = kPow10U64[cur];
        scale += cur;
        q *= power;
        if (q >= kTwo96) throw_overflow();
        u128 num = r * power;
        q += num / d;
        r = num % d;
        if (q >= kTwo96) {
            scale = overflow_unscale(q, scale, r != 0);
            break;
        }
    }

    if (unscale) {
        while (scale > 0 && q % 10 == 0) {
            q /= 10;
            --scale;
        }
    }
    return Dec::make(q, scale, a.negative() != b.negative());
}

int compare(const Dec& a, const Dec& b) {
    int sa = a.is_zero() ? 0 : (a.negative() ? -1 : 1);
    int sb = b.is_zero() ? 0 : (b.negative() ? -1 : 1);
    if (sa != sb) return sa < sb ? -1 : 1;
    if (sa == 0) return 0;
    int s = std::max(a.scale(), b.scale());
    U256 am = mul_pow10(U256::from(a.mag()), s - a.scale());
    U256 bm = mul_pow10(U256::from(b.mag()), s - b.scale());
    int c = cmp(am, bm);
    return sa < 0 ? -c : c;
}

Dec Dec::from_float(float input) {
    uint32_t bits;
    std::memcpy(&bits, &input, 4);
    int exp = static_cast<int>((bits >> 23) & 0xFF) - 126;
    if (exp < -94) return Dec{};
    if (exp > 96) throw_overflow();

    bool neg = false;
    if (input < 0) {
        input = -input;
        neg = true;
    }

    double dbl = input;
    int power = 6 - ((exp * 19728) >> 16);
    if (power >= 0) {
        if (power > kMaxScale) power = kMaxScale;
        dbl *= kDoublePow10[power];
    } else {
        if (power != -1 || dbl >= 1E7)
            dbl /= kDoublePow10[-power];
        else
            power = 0;
    }

    if (dbl < 1E6 && power < kMaxScale) {
        dbl *= 10;
        ++power;
    }

    uint32_t mant = static_cast<uint32_t>(static_cast<int32_t>(std::nearbyint(dbl)));
    if (mant == 0) return Dec{};

    if (power < 0) {
        u128 m = static_cast<u128>(mant) * pow10_u128(-power);
        if (m >= kTwo96) throw_overflow();
        return make(m, 0, neg);
    }

    int lmax = std::min(power, 6);
    if ((mant & 0xF) == 0 && lmax >= 4 && mant % 10000 == 0) {
        mant /= 10000;
        power -= 4;
        lmax -= 4;
    }
    if ((mant & 3) == 0 && lmax >= 2 && mant % 100 == 0) {
        mant /= 100;
        power -= 2;
        lmax -= 2;
    }
    if ((mant & 1) == 0 && lmax >= 1 && mant % 10 == 0) {
        mant /= 10;
        power -= 1;
    }
    return make(mant, power, neg);
}

double Dec::to_double() const {
    constexpr double ds2to64 = 1.8446744073709552e+019;
    uint64_t low64 = (static_cast<uint64_t>(mid) << 32) | lo;
    double dbl = (static_cast<double>(low64) + static_cast<double>(hi) * ds2to64) / kDoublePow10[scale()];
    return negative() ? -dbl : dbl;
}

int64_t Dec::to_int64() const {
    u128 q = mag() / pow10_u128(scale());
    if (negative()) {
        if (q > static_cast<u128>(INT64_MAX) + 1) throw_overflow();
        return static_cast<int64_t>(~static_cast<uint64_t>(q) + 1);
    }
    if (q > static_cast<u128>(INT64_MAX)) throw_overflow();
    return static_cast<int64_t>(q);
}

int32_t Dec::to_int() const {
    int64_t v = to_int64();
    if (v < INT32_MIN || v > INT32_MAX) throw_overflow();
    return static_cast<int32_t>(v);
}

std::string Dec::to_string() const {
    u128 m = mag();
    std::string digits;
    do {
        digits.push_back(static_cast<char>('0' + static_cast<int>(m % 10)));
        m /= 10;
    } while (m != 0);
    int s = scale();
    while (static_cast<int>(digits.size()) <= s) digits.push_back('0');
    std::string out = negative() && !is_zero() ? "-" : "";
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
        out.push_back(digits[static_cast<size_t>(i)]);
        if (i == s && s > 0) out.push_back('.');
    }
    return out;
}

Dec dec_truncate(const Dec& d) { return Dec::make(d.mag() / pow10_u128(d.scale()), 0, d.negative()); }

Dec dec_floor(const Dec& d) {
    u128 p = pow10_u128(d.scale());
    u128 q = d.mag() / p;
    if (d.negative() && d.mag() % p != 0) ++q;
    return Dec::make(q, 0, d.negative());
}

Dec dec_ceiling(const Dec& d) {
    u128 p = pow10_u128(d.scale());
    u128 q = d.mag() / p;
    if (!d.negative() && d.mag() % p != 0) ++q;
    return Dec::make(q, 0, d.negative());
}

Dec dec_round(const Dec& d, int decimals, Midpoint mode) {
    if (d.scale() <= decimals) return d;
    int drop = d.scale() - decimals;
    u128 p = pow10_u128(drop);
    u128 q = d.mag() / p;
    u128 r = d.mag() % p;
    u128 half = p / 2;
    bool up = mode == Midpoint::AwayFromZero ? r >= half : (r > half || (r == half && (q & 1) != 0));
    if (up) ++q;
    return Dec::make(q, decimals, d.negative());
}

}  // namespace sts2
