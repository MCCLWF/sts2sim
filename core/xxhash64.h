// XXH64, bit-identical to System.IO.Hashing.XxHash64.HashToUInt64 (used by
// StringHelper.GetDeterministicHashCode for every RNG seed derivation).
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace sts2 {

namespace xxh_detail {

inline constexpr uint64_t kP1 = 11400714785074694791ULL;
inline constexpr uint64_t kP2 = 14029467366897019727ULL;
inline constexpr uint64_t kP3 = 1609587929392839161ULL;
inline constexpr uint64_t kP4 = 9650029242287828579ULL;
inline constexpr uint64_t kP5 = 2870177450012600261ULL;

inline uint64_t rotl(uint64_t x, int r) { return (x << r) | (x >> (64 - r)); }

inline uint64_t read64(const uint8_t* p) {
    uint64_t v;
    std::memcpy(&v, p, 8);  // little-endian host assumed (x86-64 / arm64)
    return v;
}

inline uint32_t read32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

inline uint64_t round(uint64_t acc, uint64_t input) {
    acc += input * kP2;
    acc = rotl(acc, 31);
    return acc * kP1;
}

inline uint64_t merge_round(uint64_t acc, uint64_t val) {
    acc ^= round(0, val);
    return acc * kP1 + kP4;
}

}  // namespace xxh_detail

inline uint64_t xxhash64(const void* data, size_t len, uint64_t seed = 0) {
    using namespace xxh_detail;
    const auto* p = static_cast<const uint8_t*>(data);
    const uint8_t* const end = p + len;
    uint64_t h;

    if (len >= 32) {
        uint64_t v1 = seed + kP1 + kP2;
        uint64_t v2 = seed + kP2;
        uint64_t v3 = seed;
        uint64_t v4 = seed - kP1;
        const uint8_t* const limit = end - 32;
        do {
            v1 = round(v1, read64(p));
            v2 = round(v2, read64(p + 8));
            v3 = round(v3, read64(p + 16));
            v4 = round(v4, read64(p + 24));
            p += 32;
        } while (p <= limit);
        h = rotl(v1, 1) + rotl(v2, 7) + rotl(v3, 12) + rotl(v4, 18);
        h = merge_round(h, v1);
        h = merge_round(h, v2);
        h = merge_round(h, v3);
        h = merge_round(h, v4);
    } else {
        h = seed + kP5;
    }

    h += static_cast<uint64_t>(len);

    while (p + 8 <= end) {
        h ^= round(0, read64(p));
        h = rotl(h, 27) * kP1 + kP4;
        p += 8;
    }
    if (p + 4 <= end) {
        h ^= static_cast<uint64_t>(read32(p)) * kP1;
        h = rotl(h, 23) * kP2 + kP3;
        p += 4;
    }
    while (p < end) {
        h ^= static_cast<uint64_t>(*p) * kP5;
        h = rotl(h, 11) * kP1;
        ++p;
    }

    h ^= h >> 33;
    h *= kP2;
    h ^= h >> 29;
    h *= kP3;
    h ^= h >> 32;
    return h;
}

// StringHelper.GetDeterministicHashCode: XXH64 (seed 0) over the UTF-8 bytes.
// std::string_view is expected to already hold UTF-8.
inline uint64_t deterministic_hash(std::string_view utf8) {
    return xxhash64(utf8.data(), utf8.size(), 0);
}

// StringHelper.GetDeterministicHashCodeOld, for "old"-prefixed legacy seeds.
// Operates on UTF-16 code units in C#; ASCII-only input is assumed here.
inline int32_t deterministic_hash_old(std::string_view ascii) {
    uint32_t num = 352654597u;
    uint32_t num2 = num;
    for (size_t i = 0; i < ascii.size(); i += 2) {
        num = ((num << 5) + num) ^ static_cast<uint8_t>(ascii[i]);
        if (i == ascii.size() - 1) break;
        num2 = ((num2 << 5) + num2) ^ static_cast<uint8_t>(ascii[i + 1]);
    }
    return static_cast<int32_t>(num + num2 * 1566083941u);
}

}  // namespace sts2
