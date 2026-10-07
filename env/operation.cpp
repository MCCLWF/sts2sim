#include "env/operation.h"

#include <algorithm>
#include <limits>
#include <unordered_set>

#include "env/env.h"
#include "env/c_action_validation.h"
#include "env/snapshot_json.h"
#include "env/operation_json.h"
#include "engine/json.h"
#include "game/All.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Entities/Merchant/MerchantInventory.h"
#include "game/Rewards/Reward.h"

namespace sts2 {
namespace {
using snapshot_detail::Json;

void fields(const Json& node, std::initializer_list<std::string_view> required,
            std::initializer_list<std::string_view> optional = {}) {
    if (node.kind != Json::Object) throw InvalidRequest("operation: expected object");
    for (auto key : required) node.at(key);
    for (const auto& [key, _] : node.object)
        if (std::find(required.begin(), required.end(), key) == required.end()
            && std::find(optional.begin(), optional.end(), key) == optional.end())
            throw InvalidRequest("operation: unknown field " + key);
}
std::string_view text(const Json& node) {
    if (node.kind != Json::String) throw InvalidRequest("operation: expected string");
    return node.text;
}
bool boolean(const Json& node) {
    if (node.kind != Json::Bool) throw InvalidRequest("operation: expected boolean");
    return node.boolean;
}
template<class Names> uint16_t named(const Json& node, const Names& names) {
    auto value = text(node);
    for (size_t i = 1; i < names.size(); ++i)
        if (names[i] == value) return static_cast<uint16_t>(i);
    throw InvalidRequest("operation: unknown model ID " + std::string(value));
}
const char* rng_name(Operation::TransformRng rng) {
    constexpr const char* names[] = {"player_transformations", "run_niche", "run_combat_card_selection", "event"};
    if (static_cast<size_t>(rng) >= std::size(names)) throw InvalidRequest("operation: unknown RNG source");
    return names[static_cast<size_t>(rng)];
}
Operation::TransformRng rng_from_text(std::string_view value) {
    for (uint8_t i = 0; i < 4; ++i) {
        auto kind = static_cast<Operation::TransformRng>(i);
        if (rng_name(kind) == value) return kind;
    }
    throw InvalidRequest("operation: unknown transform RNG source");
}
const char* room_name(uint8_t value) {
    switch (static_cast<RoomType>(value)) {
    case RoomType::Monster: return "MONSTER";
    case RoomType::Elite: return "ELITE";
    case RoomType::Boss: return "BOSS";
    case RoomType::Treasure: return "TREASURE";
    default: throw InvalidRequest("operation: invalid reward room");
    }
}
uint8_t room_from_text(std::string_view value) {
    for (RoomType room : {RoomType::Monster, RoomType::Elite, RoomType::Boss, RoomType::Treasure})
        if (room_name(static_cast<uint8_t>(room)) == value) return static_cast<uint8_t>(room);
    throw InvalidRequest("operation: invalid reward room");
}
const char* stop_name(uint8_t mask) {
    constexpr const char* names[] = {"operation_done", "enemy_phase_done", "safe_outcome", "enemy_phase_or_outcome"};
    if (mask > 3) throw InvalidRequest("operation: invalid stop mask");
    return names[mask];
}
}  // namespace

const char* OperationName(Operation::Kind kind) {
    constexpr const char* names[] = {"action", "obtain_relic", "transform_cards", "generate_rewards",
                                    "reroll_card_reward", "merchant_purchase"};
    if (static_cast<size_t>(kind) >= std::size(names)) throw InvalidRequest("operation: unknown kind");
    return names[static_cast<size_t>(kind)];
}

Operation ParseOperationJson(std::string_view input) {
    try { return ParseOperationDocument(snapshot_detail::Parser(input).parse()); }
    catch (const InvalidRequest&) { throw; }
    catch (const UnsupportedOperation&) { throw; }
    catch (const GameError& e) { throw InvalidRequest(std::string("operation: ") + e.what()); }
}

Operation ParseOperationDocument(const Json& root) {
    try {
        fields(root, {"format", "version", "kind", "source_hash", "params"}, {"stop"});
        if (text(root.at("format")) != "sts2env.operation" || root.at("version").integer<int>() != 1)
            throw InvalidRequest("operation: incompatible format or version");
        Operation op;
        const auto kind = text(root.at("kind"));
        bool found = false;
        for (uint8_t i = 0; i < 6; ++i)
            if (OperationName(static_cast<Operation::Kind>(i)) == kind) {
                op.kind = static_cast<Operation::Kind>(i); found = true; break;
            }
        if (!found) throw UnsupportedOperation("operation: unsupported kind " + std::string(kind));
        op.source_hash = root.at("source_hash").integer<uint64_t>();
        if (root.object.contains("stop")) {
            const auto stop = text(root.at("stop"));
            bool valid = false;
            for (uint8_t i = 0; i < 4; ++i) if (stop_name(i) == stop) { op.stop_mask = i; valid = true; break; }
            if (!valid) throw UnsupportedOperation("operation: unsupported stop condition " + std::string(stop));
        }
        const Json& p = root.at("params");
        switch (op.kind) {
        case Operation::Kind::Action:
            fields(p, {"kind", "index", "target", "col", "row"});
            op.action = action_from_c_checked({p.at("kind").integer<int32_t>(), p.at("index").integer<int32_t>(),
                p.at("target").integer<int32_t>(), p.at("col").integer<int32_t>(), p.at("row").integer<int32_t>()});
            break;
        case Operation::Kind::ObtainRelic:
            fields(p, {"insert_index"}, {"relic_id", "relic_ref"});
            if (p.object.contains("relic_id") == p.object.contains("relic_ref"))
                throw InvalidRequest("operation: supply exactly one relic_id or relic_ref");
            if (p.object.contains("relic_id")) op.relic_id = named(p.at("relic_id"), kRelicIdNames);
            else op.relic_ref = p.at("relic_ref").integer<uint32_t>();
            op.insert_index = p.at("insert_index").integer<int32_t>();
            break;
        case Operation::Kind::TransformCards:
            fields(p, {"cards", "rng_source", "mode"});
            if (p.at("cards").kind != Json::Array || p.at("cards").array.empty() || p.at("cards").array.size() > 1023)
                throw InvalidRequest("operation: cards must contain 1..1023 references");
            for (const Json& card : p.at("cards").array) op.cards.push_back(card.integer<uint32_t>());
            op.rng_source = rng_from_text(text(p.at("rng_source")));
            if (text(p.at("mode")) != "batch" && text(p.at("mode")) != "sequential")
                throw InvalidRequest("operation: transform mode must be batch or sequential");
            op.sequential = text(p.at("mode")) == "sequential";
            break;
        case Operation::Kind::GenerateRewards:
            fields(p, {"room"});
            op.room = room_from_text(text(p.at("room")));
            break;
        case Operation::Kind::RerollCardReward:
            fields(p, {"reward_ref"}); op.reward_ref = p.at("reward_ref").integer<uint32_t>(); break;
        case Operation::Kind::MerchantPurchase:
            fields(p, {"entry_index", "ignore_cost"});
            op.entry_index = p.at("entry_index").integer<int32_t>();
            op.ignore_cost = boolean(p.at("ignore_cost")); break;
        }
        return op;
    } catch (const InvalidRequest&) { throw; }
      catch (const UnsupportedOperation&) { throw; }
      catch (const GameError& e) { throw InvalidRequest(std::string("operation: ") + e.what()); }
}

void WriteOperation(JsonWriter& w, const Operation& op) {
    w.begin_object().field("format", "sts2env.operation").field("version", 1)
        .field("kind", OperationName(op.kind)).field("source_hash", op.source_hash)
        .field("stop", stop_name(op.stop_mask)).key("params").begin_object();
    switch (op.kind) {
    case Operation::Kind::Action:
        w.field("kind", static_cast<int32_t>(op.action.kind)).field("index", op.action.index)
            .field("target", op.action.target).field("col", op.action.coord.col).field("row", op.action.coord.row); break;
    case Operation::Kind::ObtainRelic:
        if (op.relic_id) w.field("relic_id", kRelicIdNames.at(op.relic_id));
        else w.field("relic_ref", op.relic_ref);
        w.field("insert_index", op.insert_index); break;
    case Operation::Kind::TransformCards:
        w.key("cards").begin_array(); for (uint32_t card : op.cards) w.value(card); w.end_array();
        w.field("rng_source", rng_name(op.rng_source)).field("mode", op.sequential ? "sequential" : "batch"); break;
    case Operation::Kind::GenerateRewards: w.field("room", room_name(op.room)); break;
    case Operation::Kind::RerollCardReward: w.field("reward_ref", op.reward_ref); break;
    case Operation::Kind::MerchantPurchase: w.field("entry_index", op.entry_index).field("ignore_cost", op.ignore_cost); break;
    }
    w.end_object().end_object();
}

void ValidateOperation(const Env& source, const Operation& op) {
    (void)OperationName(op.kind);
    // source_hash binds state-local handles to the precise source, including
    // choice-prefix identity. Validate before allocating or consuming RNG.
    if (op.source_hash != source.Hash()) throw InvalidRequest("operation: stale source_hash");
    if (op.stop_mask > 3) throw InvalidRequest("operation: invalid stop mask");
    if (op.kind == Operation::Kind::Action) {
        if (op.stop_mask && (op.action.kind == Action::Answer || op.action.kind == Action::Resume))
            throw InvalidRequest("operation: a continuation retains the pending stop policy");
        const auto legal = source.LegalActions();
        if (std::find(legal.begin(), legal.end(), op.action) == legal.end())
            throw InvalidRequest("operation: action is not legal at this decision");
        return;
    }
    if (source.decision().kind == DecisionKind::Choice)
        throw InvalidRequest("operation: continue the pending native operation with Answer first");
    if (source.decision().kind == DecisionKind::Ended)
        throw InvalidRequest("operation: run has ended");
    if (source.decision().kind == DecisionKind::Boundary && op.kind != Operation::Kind::GenerateRewards)
        throw InvalidRequest("operation: suspended operation must be resumed first");
    // Vanilla getters may materialize caches; all validation runs on a clone.
    auto facts = source.Clone();
    const auto& g = facts->state();
    Sim* sim = &facts->sim();
    Player player(sim, 1);
    auto supported = [](ModelKind kind, uint16_t id, std::string_view model) {
        if (!IsUsable(kind, id)) throw Unported("operation: unported model " + std::string(model));
    };
    switch (op.kind) {
    case Operation::Kind::ObtainRelic: {
        if ((op.relic_id != 0) == (op.relic_ref != 0)) throw InvalidRequest("operation: ambiguous relic");
        uint16_t id = op.relic_id;
        if (id >= kRelicIdNames.size()) throw InvalidRequest("operation: invalid relic ID");
        if (op.relic_ref) {
            if (!g.relics.live(op.relic_ref) || g.relics[op.relic_ref].owner != 0)
                throw InvalidRequest("operation: relic_ref must be a live unowned instance");
            id = static_cast<uint16_t>(g.relics[op.relic_ref].id);
        }
        if (op.insert_index < -1 || op.insert_index > static_cast<int32_t>(player.Relics().size()))
            throw InvalidRequest("operation: relic insert_index out of range");
        supported(ModelKind::Relic, id, kRelicIdNames.at(id));
        break;
    }
    case Operation::Kind::TransformCards: {
        if (op.cards.empty() || op.cards.size() > INT32_MAX) throw InvalidRequest("operation: invalid transform count");
        if (static_cast<uint8_t>(op.rng_source) > 3) throw InvalidRequest("operation: invalid RNG source");
        if (op.rng_source == Operation::TransformRng::Event
            && (g.run.phase != RunPhase::Event || g.run.events[0].id == EventId::NONE
                || !g.run.events[0].began || g.run.events[0].is_finished || g.run.events[0].cleanup_called))
            throw InvalidRequest("operation: no active unfinished event RNG");
        if (op.rng_source == Operation::TransformRng::RunCombatCardSelection && !g.combat.is_in_progress)
            throw InvalidRequest("operation: combat selection RNG requires combat");
        std::unordered_set<uint32_t> unique;
        for (uint32_t ref : op.cards) {
            if (!ref || !g.cards.live(ref) || !unique.insert(ref).second)
                throw InvalidRequest("operation: card reference is missing or duplicated");
            CardModel card(sim, ref);
            if (card.Owner() != player || !card.Pile() || !card.IsTransformable())
                throw InvalidRequest("operation: card is not transformable in its current pile");
            supported(ModelKind::Card, card.DenseId(), name(card.CardId_()));
        }
        break;
    }
    case Operation::Kind::RerollCardReward: {
        if (!op.reward_ref || !g.rewards.live(op.reward_ref)
            || !g.run.rewards.rewards.contains(op.reward_ref)) throw InvalidRequest("operation: reward is not in the current set");
        Reward reward(sim, op.reward_ref);
        if (source.decision().kind != DecisionKind::Rewards || reward.Type() != RewardType::Card
            || reward.SuccessfullySelected() || !reward.CanReroll())
            throw InvalidRequest("operation: card reward cannot reroll");
        break;
    }
    case Operation::Kind::MerchantPurchase: {
        auto entries = Merchant::AllEntries(sim);
        if (source.decision().kind != DecisionKind::Shop || op.entry_index < 0
            || static_cast<size_t>(op.entry_index) >= entries.size()) throw InvalidRequest("operation: invalid shop entry");
        if (!Merchant::IsStocked(sim, entries[op.entry_index])) throw InvalidRequest("operation: shop entry is out of stock");
        if (!op.ignore_cost && !Merchant::EnoughGold(sim, entries[op.entry_index]))
            throw InvalidRequest("operation: insufficient gold");
        break;
    }
    case Operation::Kind::GenerateRewards:
        // Resume the exact winning operation through its vanilla room flow,
        // rather than generating a second reward set or bypassing end hooks.
        if (!source.decision().boundary || source.decision().boundary->kind != ExecutionBoundaryKind::SafeOutcome
            || source.decision().kind != DecisionKind::Boundary)
            throw UnsupportedOperation("operation: reward generation requires a pre-settlement combat boundary");
        if (g.combat.pending_loss || player.Creature().IsDead())
            throw InvalidRequest("operation: losing combat has no reward settlement");
        if (g.run.rooms.empty() || static_cast<uint8_t>(g.run.rooms.back()) != op.room)
            throw InvalidRequest("operation: reward room differs from current combat room");
        if (op.stop_mask) throw InvalidRequest("operation: reward settlement requires operation_done stop");
        break;
    case Operation::Kind::Action: break;
    }
}

OperationResult ExecuteOperation(Sim& sim, const Operation& op) {
    OperationResult result;
    Player player(&sim, 1);
    switch (op.kind) {
    case Operation::Kind::ObtainRelic:
        if (op.relic_ref) RelicCmd::Obtain(RelicModel(&sim, op.relic_ref), player, op.insert_index);
        else RelicCmd::Obtain(static_cast<RelicId>(op.relic_id), player, op.insert_index);
        break;
    case Operation::Kind::TransformCards: {
        Rng* rng = nullptr;
        switch (op.rng_source) {
        case Operation::TransformRng::PlayerTransformations: rng = &player.PlayerRng().Transformations(); break;
        case Operation::TransformRng::RunNiche: rng = &player.RunState().Rng().Niche(); break;
        case Operation::TransformRng::RunCombatCardSelection: rng = &player.RunState().Rng().CombatCardSelection(); break;
        case Operation::TransformRng::Event: rng = &sim.g->run.events[0].rng; break;
        }
        if (op.sequential) {
            for (uint32_t card : op.cards) CardCmd::TransformToRandom(CardModel(&sim, card), *rng);
        } else {
            std::vector<CardTransformation> transformations;
            for (uint32_t card : op.cards) transformations.emplace_back(CardModel(&sim, card));
            CardCmd::Transform(transformations, rng);
        }
        break;
    }
    case Operation::Kind::RerollCardReward: Reward(&sim, op.reward_ref).Reroll(); break;
    case Operation::Kind::MerchantPurchase:
        result.merchant_purchase = MerchantPurchaseResult{op.entry_index,
            Merchant::TryPurchase(&sim, Merchant::AllEntries(&sim)[op.entry_index], op.ignore_cost), op.ignore_cost};
        break;
    case Operation::Kind::GenerateRewards:
        throw UnsupportedOperation("operation: reward generation requires a pre-settlement combat boundary");
    case Operation::Kind::Action: throw GameError("operation: action must use the Env decision loop");
    }
    return result;
}
}  // namespace sts2
