// Run-level pieces of the simulation state (relic bags, room sets, odds).
// The state root is GameState in engine/sim/state.h.
#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <type_traits>

#include "content/generated/tables.h"
#include "core/rng.h"
#include "engine/containers.h"
#include "engine/map.h"

namespace sts2 {

inline constexpr int kMaxDeck = 128;
inline constexpr int kMaxRelics = 64;
// 3 starting slots plus Alchemical Coffer (+4), Potion Belt (+2), and
// Phial Holster (+1). This is storage for the known non-stackable slot
// increases, not a claim about arbitrary imported or modified states.
inline constexpr int kMaxPotionSlots = 10;
inline constexpr int kMaxEvents = 40;
inline constexpr int kMaxActs = 4;

// Runs/RelicGrabBag.cs. `deques` keeps Dictionary<RelicRarity, List<RelicModel>>
// insertion order, which decides the order the deques are shuffled.
struct RelicGrabBag {
    struct Deque {
        RelicRarity rarity = RelicRarity::None;
        StaticVec<RelicId, 160> relics;
    };
    StaticVec<Deque, 8> deques;
    StaticVec<RelicId, 32> mp_fallback;
    StaticVec<RelicId, 320> original;  // _originalRelics, for RefreshRarity
    bool refresh_allowed = false;

    bool populated() const { return !deques.empty(); }
    Deque* find(RelicRarity r) {
        for (Deque& d : deques) {
            if (d.rarity == r) return &d;
        }
        return nullptr;
    }
};

// Rooms/RoomSet.cs
struct RoomSet {
    StaticVec<EventId, kMaxEvents> events;
    int32_t events_visited = 0;
    StaticVec<EncounterId, 16> normal_encounters;
    int32_t normal_encounters_visited = 0;
    StaticVec<EncounterId, 16> elite_encounters;
    int32_t elite_encounters_visited = 0;
    int32_t boss_encounters_visited = 0;
    EncounterId boss = EncounterId::NONE;
    EncounterId second_boss = EncounterId::NONE;
    EventId ancient = EventId::NONE;
};

struct ActState {
    ActId id = ActId::NONE;
    RoomSet rooms;
    StaticVec<EventId, 4> shared_ancient_subset;
    bool has_shared_ancient_subset = false;  // _sharedAncientSubset != null
};

// Odds/UnknownMapPointOdds (values are floats in the game).
struct UnknownMapPointOdds {
    float monster = 0.1f;
    float elite = -1.0f;
    float treasure = 0.02f;
    float shop = 0.03f;
};

struct PlayerOdds {
    float card_rarity = -0.05f;   // CardRarityOdds.CurrentValue
    float potion_reward = 0.4f;   // PotionRewardOdds.CurrentValue
};

// Seeded new-run configuration. The game consults local save unlocks for
// content pools, first-run tutorials and act discovery. Pool filtering can
// use an exact epoch snapshot while run discovery remains veteran-only.
struct ProfileConfig {
    bool all_unlocked = true;
    uint64_t unlocked_epoch_bits = 0x1ffffffffffffffULL;
};

// Live UnlockState facts. Epoch bits follow EpochModel.AllEpochIds in the
// pinned game version (57 entries); encounter bits use generated EncounterId
// values (NONE is bit 0). The two sources are distinct in a live run.
struct UnlockProfileState {
    int32_t number_of_runs = 999999999;
    uint64_t unlocked_epoch_bits = 0x1ffffffffffffffULL;
    // ModelDb.AllEncounters contains 85 IDs; five generated encounter IDs
    // (event/deprecated/etc.) are absent from that list.
    uint64_t encounters_seen_low = 0xff7ffffffff3fffeULL;
    uint64_t encounters_seen_high = 0x7efdfffULL;
    bool full_veteran_compatible = true;
};

// Saves/AncientStats.GetVisitsAs: visits are profile progress keyed by both
// ancient event and character. Kept in the run snapshot so Env clients can
// supply the profile history used by ancient event dialogue and variables.
struct AncientEventProgressState {
    static constexpr size_t kEventIdCount = static_cast<size_t>(EventId::Count);
    static constexpr size_t kCharacterIdCount = static_cast<size_t>(CharacterId::Count);
    int32_t visits[kEventIdCount][kCharacterIdCount] = {};

    int32_t GetVisitsAs(EventId ancient, CharacterId character) const {
        const size_t event_index = static_cast<size_t>(ancient);
        const size_t character_index = static_cast<size_t>(character);
        if (event_index == 0 || event_index >= static_cast<size_t>(EventId::Count) ||
            character_index == 0 || character_index >= static_cast<size_t>(CharacterId::Count)) return 0;
        return visits[event_index][character_index];
    }

    void SetVisitsAs(EventId ancient, CharacterId character, int32_t count) {
        const size_t event_index = static_cast<size_t>(ancient);
        const size_t character_index = static_cast<size_t>(character);
        if (event_index == 0 || event_index >= static_cast<size_t>(EventId::Count) ||
            character_index == 0 || character_index >= static_cast<size_t>(CharacterId::Count))
            throw std::invalid_argument("invalid ancient progress key");
        if (count < 0) throw std::invalid_argument("negative ancient visit count");
        visits[event_index][character_index] = count;
    }
};
static_assert(std::is_trivially_copyable_v<AncientEventProgressState>);

}  // namespace sts2
