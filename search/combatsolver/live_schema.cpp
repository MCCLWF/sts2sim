// Private adapter for the CombatSolver Mod, outside the ordinary Env ABI.
// It returns POD field shapes and immutable monster definitions only. No run
// initialization, opening hand, RNG draws or combat commands occur here.
#include "search/combatsolver/live_schema.h"
#include "env/env.h"
#include "env/portable_registry.h"
#include "game/All.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/NativeCallback.h"
#include <bit>
#include <set>

namespace sts2::combatsolver {
using namespace snapshot_detail;
namespace {
// Creature.StunInternal keeps the stun logic as a callback in state. The game
// names it by class and method; resolve that name to this runtime's registry
// index. Every native registration with the same owner and description wraps
// the same source method, so the first in symbol order is taken.
uint16_t ResolveStunMove(const Json& stun) {
    if (stun.at("owner").is_null() || stun.at("name").is_null()) return 0;  // CreatureCmd.Stun without logic
    const std::string& owner = stun.at("owner").text;
    const std::string marker = ":Move:" + stun.at("name").text;
    for (const auto& record : native_callbacks()) {
        if (record.kind != NativeCallbackKind::Move || record.owner != owner) continue;
        const auto column = record.symbol.rfind(':');
        if (column == std::string::npos || column < marker.size()) continue;
        if (record.symbol.compare(column - marker.size(), marker.size(), marker) == 0)
            return register_move_fn(record.move);
    }
    throw InvalidRequest("unknown stun callback " + owner + "." + stun.at("name").text);
}
}
std::string LiveCaptureSchema(std::string_view request) {
    const auto input = Parser(request).parse();
    if (input.kind != Json::Object || input.object.size() != 2)
        throw InvalidRequest("live schema requires ascension and monsters");
    const int ascension = input.at("ascension").integer<int>();
    const auto& monsters = input.at("monsters");
    if (ascension < 0 || ascension > 10 || monsters.kind != Json::Array)
        throw InvalidRequest("invalid live schema context");
    Env empty;
    empty.state().run.phase = RunPhase::Ended; // serialize shape without running a decision
    auto blank = Parser(empty.ExportSnapshotJson()).parse();
    blank.object.at("decision") = Json{};
    blank.object.at("state_hash") = Json{};
    // Materialize one default value per pool solely to serialize its shape.
    auto& g = empty.state();
    g.cards.alloc(); g.creatures.alloc(); g.powers.alloc(); g.relics.alloc();
    g.potions.alloc(); g.orbs.alloc(); g.afflictions.alloc(); g.rewards.alloc();
    const auto defaults = Parser(empty.ExportSnapshotJson()).parse().at("state");
    JsonWriter writer;
    writer.begin_object().field("format", "sts2solver.live_schema").field("version", 1).key("snapshot");
    write_snapshot_json(writer, blank);
    for (const auto* pool : {"cards", "creatures", "powers", "relics", "potions", "orbs", "afflictions", "rewards"}) {
        writer.key(std::string(pool) + "_default");
        write_snapshot_json(writer, defaults.at(pool).at("slots").array[0].at("value"));
    }
    writer.key("monster_default");
    write_snapshot_json(writer, defaults.at("creatures").at("slots").array[0].at("value").at("monster"));
    writer.key("monster_layout").begin_object().field("format", "sts2env.monster_machines").field("version", 1)
        .key("machines").begin_array();
    std::set<int> combat_ids;
    for (const auto& source : monsters.array) {
        const auto id = static_cast<MonsterId>(source.at("id").integer<uint16_t>());
        const int combat_id = source.at("combat_id").integer<int>();
        if (id <= MonsterId::NONE || id >= MonsterId::Count || combat_id < 0 || !combat_ids.insert(combat_id).second)
            throw InvalidRequest("invalid or duplicate monster schema identity");
        const auto& extra = source.at("extra");
        if (extra.kind != Json::Array || extra.array.size() != kExtraSlots)
            throw InvalidRequest("invalid monster schema fields");
        // Scalar context only supplies constructors of immutable definitions.
        // The managed adapter must compare every state, callback, intent and
        // branch against the frozen live definition before using its registry ID.
        Env context;
        context.state().run.ascension = ascension;
        const auto ref = context.state().creatures.alloc();
        auto& creature = context.state().creatures[ref];
        creature.monster.id = id;
        for (int i = 0; i < kExtraSlots; ++i) creature.monster.extra[i] = extra.array[i].integer<int32_t>();
        auto def = MonsterModel(&context.sim(), ref).GenerateMoveStateMachine();
        const auto index = intern_move_machine(std::move(def), id);
        const auto& definition = move_machine_def(index);
        uint16_t stun_move = 0;
        if (auto stun = source.object.find("stun"); stun != source.object.end() && stun->second.kind == Json::Object)
            stun_move = ResolveStunMove(stun->second);
        writer.begin_object().field("combat_id", combat_id).field("monster_id", static_cast<int32_t>(id))
            .field("exists", true).field("definition", index).field("stun_move", static_cast<int32_t>(stun_move))
            .field("initial", definition.initial).key("states").begin_array();
                for (const auto& move : definition.states) {
                    writer.begin_object().field("id", move.id).field("kind", static_cast<int32_t>(move.kind))
                        .field("on_perform", move.onPerformName).field("follow_up", move.followUp)
                        .field("follow_up_id", move.followUpId).field("must_perform_once", move.mustPerformOnceBeforeTransitioning)
                        .key("intents").begin_array();
                    for (const auto& intent : move.intents) {
                        writer.begin_object().field("type", static_cast<int32_t>(intent.type)).field("damage", intent.damage)
                            .field("dynamic_damage", intent.damageCalc != nullptr).field("repeat", intent.multi ? intent.repeat : 1)
                            .field("dynamic_repeat", intent.repeatCalc != nullptr).field("multi", intent.multi)
                            .field("strong", intent.strong).field("count", intent.count).end_object();
                    }
                    writer.end_array().key("weights").begin_array();
                    for (const auto& weight : move.weights) {
                        writer.begin_object().field("state", weight.state).field("repeat_type", static_cast<int32_t>(weight.repeatType))
                            .field("max_times", weight.maxTimes).field("cooldown", weight.cooldown)
                            .field("dynamic", weight.weightLambda != nullptr).field("weight_bits", static_cast<uint64_t>(std::bit_cast<uint32_t>(weight.weight)))
                            .end_object();
                    }
                    writer.end_array().key("branches").begin_array();
                    for (const auto& branch : move.branches) {
                        writer.begin_object().field("state", branch.state).field("conditional", branch.condition != nullptr).end_object();
                    }
                    writer.end_array().end_object();
                }
                writer.end_array();
        writer.end_object();
    }
    writer.end_array().end_object().end_object();
    return writer.str();
}
}
