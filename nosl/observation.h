// The NoSL actor observation: what a player can see at the current decision,
// and the legal actions bound to it (docs/plans/2026-10-03-silent-a10-nosl.md,
// I01/I02). Schema `nosl.obs` version 1.
//
// The observation is three integer tables:
//   global  [kGlobalWidth]              run and decision scalars
//   tokens  [n_tokens x kTokenWidth]    one row per visible entity or option
//   actions [n_actions x kActionWidth]  one row per legal action, pointing at tokens
//
// Information boundary. Nothing here may depend on hidden state: no RNG
// state, no draw order, no future rooms/rewards, no engine instance ids.
// Piles are emitted as multisets in a canonical order, and the actions of a
// card selection are reordered canonically, so two states that differ only
// in hidden state produce identical tables (nosl_obs_check verifies this).
//
// Map nodes are the ones on the map screen, in grid order; points the map
// generator created and then pruned are not listed (they were until
// 2026-10-04, found by comparing with the game-side encoder, tools/NoslPilot).
//
// Known gaps of version 1 (docs/reports/2026-10-03-nosl-q1-foundation.md):
// publicly known draw-pile tops and enemy move history are not tracked, a
// bundle option shows only its first card, the Crystal Sphere grid is not
// described, and an ally target has no token.
#pragma once

#include <cstdint>
#include <vector>

#include "env/env.h"

namespace sts2::nosl {

inline constexpr int32_t kObsSchemaVersion = 1;
inline constexpr size_t kGlobalWidth = 40;
inline constexpr size_t kTokenWidth = 16;
inline constexpr size_t kActionWidth = 4;

enum class TokenType : int32_t {
    Pad, Hand, Draw, Discard, Exhaust, Deck, Enemy, Power, Relic, Potion,
    MapNext, MapNode, Reward, ChoiceOption, Shop, EventOption, RestOption, Treasure,
    Count,
};

// Token columns. Id columns index the content tables (0 = none); `misc`
// columns index one shared vocabulary (see `misc` below); n0..n7 are numbers.
enum TokenColumn : size_t {
    kColType, kColCard, kColRelic, kColPotion, kColMonster, kColPower, kColMiscA, kColMiscB,
    kColN0, kColN1, kColN2, kColN3, kColN4, kColN5, kColN6, kColN7,
};

// Action columns: the Action kind, the token it acts on (-1: none), the
// target token (-1: none), and a number (a subset-count answer, or the
// combat id of a target that has no token).
enum ActionColumn : size_t { kActKind, kActToken, kActTarget, kActNumber };

namespace misc {
inline constexpr int32_t kMapPoint = 1;      // + MapPointType
inline constexpr int32_t kRewardType = 17;   // + RewardType
inline constexpr int32_t kShopKind = 33;     // + Merchant::EntryRef kind
inline constexpr int32_t kRestOption = 41;   // + RestSiteOptionId
inline constexpr int32_t kChoiceKind = 57;   // + ChoiceOptionKind
inline constexpr int32_t kEnchantment = 64;  // + EnchantmentId
inline constexpr int32_t kAffliction = 96;   // + AfflictionId
inline constexpr int32_t kEvent = 128;       // + EventId
inline constexpr int32_t kHashed = 256;      // + text-key bucket
inline constexpr int32_t kHashBuckets = 1024;
inline constexpr int32_t kSize = kHashed + kHashBuckets;
}  // namespace misc

enum GlobalColumn : size_t {
    kGSchema, kGDecision, kGCharacter, kGAscension, kGAct, kGActFloor, kGTotalFloor,
    kGHp, kGMaxHp, kGBlock, kGGold, kGInCombat, kGEnergy, kGMaxEnergy, kGStars,
    kGTurn, kGRound, kGHandCount, kGDrawCount, kGDiscardCount, kGExhaustCount,
    kGDeckCount, kGRelicCount, kGPotionCount, kGPotionSlots, kGBoss, kGRoomType,
    kGChoiceKind, kGChoiceMin, kGChoiceMax, kGChoiceSelected, kGChoiceSubset,
    kGChoicePrompt, kGChoiceSource, kGChoiceCanSkip, kGTokenCount, kGActionCount,
    kGEvent, kGReserved0, kGReserved1,
};

struct Observation {
    std::vector<int32_t> global;
    std::vector<int32_t> tokens;
    std::vector<int32_t> actions;
    // Row i of `actions` describes legal[i]; the order is canonical, not
    // necessarily Env::LegalActions() order.
    std::vector<Action> legal;

    size_t token_count() const { return tokens.size() / kTokenWidth; }
    size_t action_count() const { return actions.size() / kActionWidth; }
};

// Getters run on a private clone; the source Env is unchanged.
void Observe(const Env& env, Observation& out);

}  // namespace sts2::nosl
