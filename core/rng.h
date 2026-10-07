// Bit-exact port of MegaCrit.Sts2.Core.Random.MegaRandom (Xoshiro256**) and
// the Rng wrapper (src/Core/Random/Rng.cs), plus the run/player stream seeding
// from Runs/RunRngSet.cs and Random/PlayerRngSet.cs.
//
// Floating-point notes: every expression mirrors the C# operand types exactly
// (float stays float, double stays double). Build with -ffp-contract=off.
#pragma once

#include <array>
#include <cassert>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdint>
#include <span>
#include <type_traits>
#include <string>
#include <string_view>

#include "core/xxhash64.h"
#include "core/rng_observer.h"

namespace sts2 {

class MegaRandom {
public:
    uint64_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;

    MegaRandom() = default;
    explicit MegaRandom(uint64_t seed) { reinitialise(seed); }

    static uint64_t splitmix64(uint64_t& x) {
        uint64_t n = (x += 11400714819323198485ULL);
        n = (n ^ (n >> 30)) * 13787848793156543929ULL;
        n = (n ^ (n >> 27)) * 10723151780598845931ULL;
        return n ^ (n >> 31);
    }

    void reinitialise(uint64_t seed) {
        s0 = splitmix64(seed);
        s1 = splitmix64(seed);
        s2 = splitmix64(seed);
        s3 = splitmix64(seed);
    }

    uint64_t next_ulong() {
        uint64_t s = s0, t1 = s1, t2 = s2, t3 = s3;
        uint64_t result = rotl(t1 * 5, 7) * 9;
        uint64_t num = t1 << 17;
        t2 ^= s;
        t3 ^= t1;
        t1 ^= t2;
        s ^= t3;
        t2 ^= num;
        t3 = rotl(t3, 45);
        s0 = s;
        s1 = t1;
        s2 = t2;
        s3 = t3;
        return result;
    }

    double next_double() { return static_cast<double>(next_ulong() >> 11) * 1.1102230246251565E-16; }

    float next_float() { return static_cast<float>(next_ulong() >> 40) * 5.9604645E-08f; }

    int32_t next_int_raw() { return static_cast<int32_t>(next_ulong() >> 33); }

    uint32_t next_uint() { return static_cast<uint32_t>(next_ulong()); }

    bool next_bool_raw() { return (next_ulong() & 0x8000000000000000ULL) != 0; }

    // MegaRandom.Next(int maxValue)
    int32_t next(int32_t max_value) {
        assert(max_value >= 1);
        return static_cast<int32_t>(next_double() * static_cast<double>(max_value));
    }

    // MegaRandom.Next(int minValue, int maxValue)
    int32_t next(int32_t min_value, int32_t max_value) {
        assert(min_value < max_value);
        int64_t range = static_cast<int64_t>(max_value) - static_cast<int64_t>(min_value);
        if (range <= INT32_MAX) {
            return static_cast<int32_t>(next_double() * static_cast<double>(static_cast<int32_t>(range))) + min_value;
        }
        return static_cast<int32_t>(static_cast<int64_t>(next_double() * static_cast<double>(range)) + min_value);
    }

    friend bool operator==(const MegaRandom&, const MegaRandom&) = default;

private:
    static uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
};

// Mirrors MegaCrit.Sts2.Core.Random.Rng. The counter is serialized by the game
// (SerializableRng.counter), so it is tracked for state comparison.
class Rng {
public:
    MegaRandom random;
    int32_t counter = 0;

    Rng() : random(0) {}
    explicit Rng(uint64_t seed) : random(seed) {}
    // Rng(ulong seed, string name)
    Rng(uint64_t seed, std::string_view name) : random(seed + deterministic_hash(name)) {}

    static Rng from_state(uint64_t s0, uint64_t s1, uint64_t s2, uint64_t s3, int32_t counter) {
        Rng r;
        r.random.s0 = s0;
        r.random.s1 = s1;
        r.random.s2 = s2;
        r.random.s3 = s3;
        r.counter = counter;
        return r;
    }

    // Rng(Player player, ModelId id, ulong mixin): seed + slot + hash(id.Entry) + mixin, all wrapping.
    static Rng for_content(uint64_t run_seed, int32_t player_slot, std::string_view entry, uint64_t mixin = 0) {
        uint64_t s = run_seed + static_cast<uint64_t>(static_cast<int64_t>(player_slot)) + deterministic_hash(entry);
        return Rng(s + mixin);
    }

    bool next_bool() {
        RngTraceCall trace(*this,"NextBool");
        ++counter;
        return trace.Finish(random.next(2) == 0,*this);
    }

    int32_t next_int(int32_t max_exclusive = INT32_MAX) {
        RngTraceCall trace(*this,"NextInt(max)",{RngTraceValue(max_exclusive)});
        ++counter;
        return trace.Finish(random.next(max_exclusive),*this);
    }

    int32_t next_int(int32_t min_inclusive, int32_t max_exclusive) {
        assert(min_inclusive < max_exclusive);
        RngTraceCall trace(*this,"NextInt(min,max)",{RngTraceValue(min_inclusive),RngTraceValue(max_exclusive)});
        ++counter;
        return trace.Finish(random.next(min_inclusive, max_exclusive),*this);
    }

    uint32_t next_unsigned_int(uint32_t max_exclusive = UINT32_MAX) {
        RngTraceCall trace(*this,"NextUnsignedInt(max)",{RngTraceValue(max_exclusive)});
        return trace.Finish(next_unsigned_int(0u, max_exclusive),*this);
    }

    uint32_t next_unsigned_int(uint32_t min_inclusive, uint32_t max_exclusive) {
        assert(min_inclusive < max_exclusive);
        RngTraceCall trace(*this,"NextUnsignedInt(min,max)",{RngTraceValue(min_inclusive),RngTraceValue(max_exclusive)});
        ++counter;
        double num = random.next_double();
        double num2 = static_cast<double>(max_exclusive - min_inclusive);
        return trace.Finish(min_inclusive + static_cast<uint32_t>(num * num2),*this);
    }

    uint64_t next_unsigned_long() {
        RngTraceCall trace(*this,"NextUnsignedLong");
        ++counter;
        return trace.Finish(random.next_ulong(),*this);
    }

    uint64_t next_unsigned_long(uint64_t max_exclusive) {
        RngTraceCall trace(*this,"NextUnsignedLong(max)",{RngTraceValue(max_exclusive)});
        if (max_exclusive == UINT64_MAX) return trace.Finish(next_unsigned_long(),*this);
        return trace.Finish(next_unsigned_long(0, max_exclusive),*this);
    }

    uint64_t next_unsigned_long(uint64_t min_inclusive, uint64_t max_exclusive) {
        assert(min_inclusive < max_exclusive);
        RngTraceCall trace(*this,"NextUnsignedLong(min,max)",{RngTraceValue(min_inclusive),RngTraceValue(max_exclusive)});
        ++counter;
        double num = random.next_double();
        double num2 = static_cast<double>(max_exclusive - min_inclusive);
        return trace.Finish(min_inclusive + static_cast<uint64_t>(num * num2),*this);
    }

    float next_float(float max = 1.0f) {
        RngTraceCall trace(*this,"NextFloat(max)",{RngTraceValue(max)});
        return trace.Finish(next_float(0.0f,max),*this);
    }

    float next_float(float min, float max) {
        assert(!(min > max));
        RngTraceCall trace(*this,"NextFloat(min,max)",{RngTraceValue(min),RngTraceValue(max)});
        ++counter;
        float range = max - min;  // float subtraction, as in C#
        return trace.Finish(static_cast<float>(random.next_double() * static_cast<double>(range) + static_cast<double>(min)),*this);
    }

    double next_double() {
        RngTraceCall trace(*this,"NextDouble");
        ++counter;
        return trace.Finish(random.next_double(),*this);
    }

    double next_double(double min, double max) {
        assert(!(min > max));
        RngTraceCall trace(*this,"NextDouble(min,max)",{RngTraceValue(min),RngTraceValue(max)});
        ++counter;
        return trace.Finish(random.next_double() * (max - min) + min,*this);
    }

    float next_gaussian_float(float mean = 0.0f, float std_dev = 1.0f, float min = 0.0f, float max = 1.0f) {
        RngTraceCall trace(*this,"NextGaussianFloat",{RngTraceValue(mean),RngTraceValue(std_dev),RngTraceValue(min),RngTraceValue(max)});
        return trace.Finish(static_cast<float>(next_gaussian_double(mean, std_dev, min, max)),*this);
    }

    double next_gaussian_double(double mean = 0.0, double std_dev = 1.0, double min = 0.0, double max = 1.0) {
        assert(!(min > max));
        assert(mean >= 0.0 && mean <= 1.0);
        RngTraceCall trace(*this,"NextGaussianDouble",{RngTraceValue(mean),RngTraceValue(std_dev),RngTraceValue(min),RngTraceValue(max)});
        double v;
        do {
            double d = 1.0 - next_double();
            double num = 1.0 - next_double();
            double num2 = std::sqrt(-2.0 * std::log(d));
            double d2 = kTwoPi * num;
            double num3 = num2 * std::cos(d2);
            v = mean + num3 * std_dev;
        } while (v < 0.0 || v > 1.0);
        return trace.Finish(v * (max - min) + min,*this);
    }

    int32_t next_gaussian_int(int32_t mean, int32_t std_dev, int32_t min, int32_t max) {
        assert(min <= max && mean >= min && mean <= max);
        RngTraceCall trace(*this,"NextGaussianInt",{RngTraceValue(mean),RngTraceValue(std_dev),RngTraceValue(min),RngTraceValue(max)});
        int32_t r;
        do {
            double d = 1.0 - next_double();
            double num = 1.0 - next_double();
            double num2 = std::sqrt(-2.0 * std::log(d)) * std::sin(kTwoPi * num);
            double a = static_cast<double>(mean) + static_cast<double>(std_dev) * num2;
            r = static_cast<int32_t>(std::nearbyint(a));  // Math.Round: half to even
        } while (r < min || r > max);
        return trace.Finish(r,*this);
    }

    // Rng.NextItem: index into a materialized sequence, or -1 (no RNG draw) when empty.
    int32_t next_item_index(int32_t count) {
        RngTraceCall trace(*this,"NextItem",{RngTraceValue(count)});
        if (count == 0) return trace.Finish(int32_t{-1},*this);
        return trace.Finish(next_int(0, count),*this);
    }

    template <class T>
    T* next_item(std::span<T> items) {
        int32_t i = next_item_index(static_cast<int32_t>(items.size()));
        return i < 0 ? nullptr : &items[static_cast<size_t>(i)];
    }

    // Rng.WeightedNextItem with a pre-evaluated weight list. Returns the chosen
    // index, or -1 for the C# fallback (default(T)).
    int32_t weighted_next_index(std::span<const float> weights) {
        RngTraceCall trace(*this,"WeightedNextItem",{},weights);
        return trace.Finish(weighted_pick(next_float(), weights),*this);
    }

    // Rng.WeightedNextItem(float randInput, ...). Enumerable.Sum over floats
    // accumulates in double and narrows once at the end.
    static int32_t weighted_pick(float rand_input, std::span<const float> weights) {
        double sum = 0.0;
        for (float w : weights) sum += static_cast<double>(w);
        float remaining = rand_input * static_cast<float>(sum);
        for (size_t i = 0; i < weights.size(); ++i) {
            remaining -= weights[i];
            if (remaining <= 0.0f) return static_cast<int32_t>(i);
        }
        return -1;
    }

    // Rng.Shuffle and ListExtensions.UnstableShuffle (identical draw sequence).
    template <class T>
    void shuffle(std::span<T> list) {
        RngTraceCall trace(*this,"Shuffle",{RngTraceValue(static_cast<int32_t>(list.size()))});
        for (int32_t i = static_cast<int32_t>(list.size()) - 1; i > 0; --i) {
            int32_t j = next_int(i + 1);
            T a = list[static_cast<size_t>(j)];
            T b = list[static_cast<size_t>(i)];
            list[static_cast<size_t>(i)] = a;
            list[static_cast<size_t>(j)] = b;
        }
        trace.FinishVoid(*this);
    }

    // --- C# names, for the line-by-line port (game/). Same draws as above.
    bool NextBool() { return next_bool(); }
    int32_t NextInt(int32_t maxExclusive = INT32_MAX) { return next_int(maxExclusive); }
    int32_t NextInt(int32_t minInclusive, int32_t maxExclusive) { return next_int(minInclusive, maxExclusive); }
    uint32_t NextUnsignedInt(uint32_t maxExclusive = UINT32_MAX) { return next_unsigned_int(maxExclusive); }
    float NextFloat(float max = 1.0f) { return next_float(max); }
    float NextFloat(float min, float max) { return next_float(min, max); }
    double NextDouble() { return next_double(); }
    double NextDouble(double min, double max) { return next_double(min, max); }
    int32_t NextGaussianInt(int32_t mean, int32_t stdDev, int32_t min, int32_t max) {
        return next_gaussian_int(mean, stdDev, min, max);
    }
    // Rng.NextItem over a materialized list: default(T) (no draw) when empty.
    template <class List>
    auto NextItem(const List& items) -> std::remove_cvref_t<decltype(items[0])> {
        int32_t i = next_item_index(static_cast<int32_t>(items.size()));
        return i < 0 ? std::remove_cvref_t<decltype(items[0])>{} : items[static_cast<size_t>(i)];
    }
    // Rng.Shuffle<T>(IList<T>)
    template <class List>
    void Shuffle(List& list) {
        shuffle(std::span(list.data(), list.size()));
    }

    friend bool operator==(const Rng&, const Rng&) = default;

private:
    static constexpr double kTwoPi = 3.141592653589793 * 2.0;  // Math.PI * 2.0
};

// RunRngType (Entities/Rngs/RunRngType.cs), in enum declaration order.
enum class RunRngType : uint8_t {
    UpFront,
    Shuffle,
    UnknownMapPoint,
    CombatCardGeneration,
    CombatPotionGeneration,
    CombatCardSelection,
    CombatEnergyCosts,
    CombatTargets,
    MonsterAi,
    Niche,
    CombatOrbs,
    TreasureRoomRelics,
    Count
};

// StringHelper.SnakeCase(enum name), the per-stream hash salt.
inline constexpr std::array<std::string_view, static_cast<size_t>(RunRngType::Count)> kRunRngNames = {
    "up_front",
    "shuffle",
    "unknown_map_point",
    "combat_card_generation",
    "combat_potion_generation",
    "combat_card_selection",
    "combat_energy_costs",
    "combat_targets",
    "monster_ai",
    "niche",
    "combat_orbs",
    "treasure_room_relics",
};

enum class PlayerRngType : uint8_t { Rewards, Shops, Transformations, Count };

inline constexpr std::array<std::string_view, static_cast<size_t>(PlayerRngType::Count)> kPlayerRngNames = {
    "rewards",
    "shops",
    "transformations",
};

// SeedHelper.CanonicalizeSeed for ASCII seeds: upper-case, O->0, I->1, trim.
inline std::string canonicalize_seed(std::string_view seed) {
    std::string s;
    size_t b = 0, e = seed.size();
    while (b < e && std::isspace(static_cast<unsigned char>(seed[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(seed[e - 1]))) --e;
    for (size_t i = b; i < e; ++i) {
        char c = static_cast<char>(std::toupper(static_cast<unsigned char>(seed[i])));
        if (c == 'O') c = '0';
        if (c == 'I') c = '1';
        s.push_back(c);
    }
    return s;
}

// RunRngSet.Seed: hash of the (already canonical) seed string.
inline uint64_t run_seed_from_string(std::string_view seed) {
    if (seed.starts_with("old")) {
        return static_cast<uint32_t>(deterministic_hash_old(seed.substr(3)));
    }
    return deterministic_hash(seed);
}

struct RunRngSet {
    uint64_t seed = 0;
    std::array<Rng, static_cast<size_t>(RunRngType::Count)> rngs{};

    RunRngSet() = default;
    explicit RunRngSet(uint64_t run_seed) : seed(run_seed) {
        for (size_t i = 0; i < rngs.size(); ++i) rngs[i] = Rng(seed, kRunRngNames[i]);
    }

    Rng& operator[](RunRngType t) { return rngs[static_cast<size_t>(t)]; }
    const Rng& operator[](RunRngType t) const { return rngs[static_cast<size_t>(t)]; }

    // RunRngSet properties (C# names).
    Rng& UpFront() { return (*this)[RunRngType::UpFront]; }
    Rng& Shuffle() { return (*this)[RunRngType::Shuffle]; }
    Rng& UnknownMapPoint() { return (*this)[RunRngType::UnknownMapPoint]; }
    Rng& CombatCardGeneration() { return (*this)[RunRngType::CombatCardGeneration]; }
    Rng& CombatPotionGeneration() { return (*this)[RunRngType::CombatPotionGeneration]; }
    Rng& CombatCardSelection() { return (*this)[RunRngType::CombatCardSelection]; }
    Rng& CombatEnergyCosts() { return (*this)[RunRngType::CombatEnergyCosts]; }
    Rng& CombatTargets() { return (*this)[RunRngType::CombatTargets]; }
    Rng& MonsterAi() { return (*this)[RunRngType::MonsterAi]; }
    Rng& Niche() { return (*this)[RunRngType::Niche]; }
    Rng& CombatOrbs() { return (*this)[RunRngType::CombatOrbs]; }
    Rng& TreasureRoomRelics() { return (*this)[RunRngType::TreasureRoomRelics]; }
    uint64_t Seed() const { return seed; }

    friend bool operator==(const RunRngSet&, const RunRngSet&) = default;
};

struct PlayerRngSet {
    uint64_t seed = 0;
    std::array<Rng, static_cast<size_t>(PlayerRngType::Count)> rngs{};

    PlayerRngSet() = default;
    explicit PlayerRngSet(uint64_t player_seed) : seed(player_seed) {
        for (size_t i = 0; i < rngs.size(); ++i) rngs[i] = Rng(seed, kPlayerRngNames[i]);
    }

    // Player.InitializeSeed: hash(seed string) + slot index.
    static PlayerRngSet for_player(std::string_view seed_string, int32_t slot) {
        return PlayerRngSet(deterministic_hash(seed_string) + static_cast<uint64_t>(slot));
    }

    Rng& operator[](PlayerRngType t) { return rngs[static_cast<size_t>(t)]; }
    const Rng& operator[](PlayerRngType t) const { return rngs[static_cast<size_t>(t)]; }

    Rng& Rewards() { return (*this)[PlayerRngType::Rewards]; }
    Rng& Shops() { return (*this)[PlayerRngType::Shops]; }
    Rng& Transformations() { return (*this)[PlayerRngType::Transformations]; }

    friend bool operator==(const PlayerRngSet&, const PlayerRngSet&) = default;
};

}  // namespace sts2
