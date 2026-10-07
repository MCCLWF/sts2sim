#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/map.h"

namespace sts2 {
class Env;
class JsonWriter;
struct Sim;

struct Action {
    enum Kind : uint8_t {
        StartRun, MapMove, PlayCard, EndTurn, TakeReward, Proceed,
        EventOption, RestOption, ShopBuy, TreasurePick, UsePotion, DiscardPotion, Answer, Resume,
    } kind = StartRun;
    MapCoord coord{};
    int32_t index = -1;
    int32_t target = -1;
    friend constexpr bool operator==(const Action&, const Action&) = default;
};

// Native command requests live outside the trivially-copyable GameState.
// References and the source hash belong to one owned Env state. A request
// never supplies RNG state; the selected native stream is consumed in-place.
struct Operation {
    enum class Kind : uint8_t {
        Action, ObtainRelic, TransformCards, GenerateRewards, RerollCardReward, MerchantPurchase,
    } kind = Kind::Action;
    enum class TransformRng : uint8_t {
        PlayerTransformations, RunNiche, RunCombatCardSelection, Event,
    } rng_source = TransformRng::PlayerTransformations;
    uint64_t source_hash = 0;
    uint8_t stop_mask = 0;  // 0=operation done; bit 0=enemy phase; bit 1=outcome
    Action action;
    uint16_t relic_id = 0;
    uint32_t relic_ref = 0;
    int32_t insert_index = -1;
    std::vector<uint32_t> cards;
    bool sequential = false;
    uint32_t reward_ref = 0;
    int32_t entry_index = -1;
    bool ignore_cost = false;
    uint8_t room = 0;
    friend bool operator==(const Operation&, const Operation&) = default;
};

const char* OperationName(Operation::Kind kind);

// Completed native return values are execution facts, not persistent game
// state. A suspended operation has no result until its prefix settles.
struct MerchantPurchaseResult {
    int32_t entry_index;
    bool success;
    bool ignore_cost;
};
struct OperationResult {
    std::optional<MerchantPurchaseResult> merchant_purchase;
};
Operation ParseOperationJson(std::string_view json);
void WriteOperation(JsonWriter& writer, const Operation& operation);
void ValidateOperation(const Env& source, const Operation& operation);
OperationResult ExecuteOperation(Sim& sim, const Operation& operation);
}  // namespace sts2
