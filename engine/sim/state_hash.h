// Logical state hashing excludes padding and inactive container storage.
// Pool allocation order and HashSet free-list order are future-observable.
#pragma once
#include <bit>
#include <type_traits>
#include "engine/sim/state_fields.generated.h"

namespace sts2 {

class StateHasher {
public:
    template<class T> void operator()(const T& value) {
        if constexpr (std::is_same_v<T, bool>) {
            byte(value ? 1 : 0);
        } else if constexpr (std::is_enum_v<T>) {
            (*this)(static_cast<std::underlying_type_t<T>>(value));
        } else if constexpr (std::is_integral_v<T>) {
            auto bits = static_cast<std::make_unsigned_t<T>>(value);
            for (size_t i = 0; i < sizeof(T); ++i) {
                byte(static_cast<uint8_t>(bits & 0xff));
                bits >>= 8;
            }
        } else if constexpr (std::is_same_v<T, float>) {
            (*this)(std::bit_cast<uint32_t>(value));
        } else if constexpr (std::is_same_v<T, double>) {
            (*this)(std::bit_cast<uint64_t>(value));
        } else {
            visit_state_fields(value, *this);
        }
    }
    template<class T, size_t N> void operator()(const T (&values)[N]) {
        for (const auto& value : values) (*this)(value);
    }
    template<class T, size_t N> void operator()(const std::array<T, N>& values) {
        for (const auto& value : values) (*this)(value);
    }
    template<class T, size_t N> void operator()(const StaticVec<T, N>& values) {
        (*this)(static_cast<uint32_t>(values.size()));
        for (const auto& value : values) (*this)(value);
    }
    template<class T, size_t N> void operator()(const DotnetSet<T, N>& values) {
        values.visit_storage(*this);
    }
    template<class T, uint16_t N> void operator()(const Pool<T, N>& values) {
        values.visit_storage(*this);
    }
    void operator()(const Dec& value) {
        (*this)(value.lo); (*this)(value.mid); (*this)(value.hi); (*this)(value.flags);
    }
    void operator()(const Rng& value) {
        (*this)(value.random.s0); (*this)(value.random.s1);
        (*this)(value.random.s2); (*this)(value.random.s3); (*this)(value.counter);
    }
    void operator()(const RunRngSet& value) { (*this)(value.seed); (*this)(value.rngs); }
    void operator()(const PlayerRngSet& value) { (*this)(value.seed); (*this)(value.rngs); }
    uint64_t finish() const { return hash_; }

private:
    void byte(uint8_t value) { hash_ = (hash_ ^ value) * 1099511628211ULL; }
    uint64_t hash_ = 14695981039346656037ULL;
};

inline uint64_t hash_state(const GameState& state) {
    StateHasher hash;
    hash(state);
    return hash.finish();
}

}  // namespace sts2
