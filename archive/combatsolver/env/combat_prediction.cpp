#include "env/combat_prediction.h"

#include <algorithm>

#include "engine/json.h"
#include "engine/sim/state_hash.h"
#include "env/snapshot_json.h"
#include "content/generated/ids.h"

namespace sts2 {
namespace {
using snapshot_detail::Json;

void object_size(const Json& json, size_t count) {
    if (json.kind != Json::Object || json.object.size() != count)
        throw GameError("combat prediction snapshot: missing or unexpected object fields");
}

std::string quote(std::string_view value) {
    JsonWriter writer;
    writer.value(value);
    return writer.str();
}

// Preserve the parser's exact number tokens: snapshot uint64/decimal/float
// bit patterns must never pass through floating point while nesting v2.
std::string serialize(const Json& value) {
    switch (value.kind) {
    case Json::Null: return "null";
    case Json::Bool: return value.boolean ? "true" : "false";
    case Json::Number: return value.text;
    case Json::String: return quote(value.text);
    case Json::Array: {
        std::string out = "[";
        bool first = true;
        for (const auto& item : value.array) {
            if (!first) out += ',';
            first = false;
            out += serialize(item);
        }
        return out + ']';
    }
    case Json::Object: {
        std::string out = "{";
        bool first = true;
        for (const auto& [key, item] : value.object) {
            if (!first) out += ',';
            first = false;
            out += quote(key) + ':' + serialize(item);
        }
        return out + '}';
    }
    }
    throw GameError("combat prediction snapshot: unknown JSON value");
}

std::optional<CombatPredictionTerminalStamp> read_terminal(const Json& json) {
    if (json.is_null()) return std::nullopt;
    object_size(json, 2);
    CombatPredictionTerminalStamp stamp;
    stamp.player_turn = json.at("player_turn").integer<int32_t>();
    if (stamp.player_turn < 1) throw GameError("combat prediction snapshot: terminal turn must be positive");
    const Json& outcome = json.at("outcome");
    if (outcome.kind != Json::String) throw GameError("combat prediction snapshot: expected terminal outcome");
    if (outcome.text == "victory") stamp.outcome = CombatPredictionOutcome::Victory;
    else if (outcome.text == "defeat") stamp.outcome = CombatPredictionOutcome::Defeat;
    else throw GameError("combat prediction snapshot: unknown terminal outcome");
    return stamp;
}

std::string write_terminal(const std::optional<CombatPredictionTerminalStamp>& terminal) {
    if (!terminal) return "null";
    return "{\"player_turn\":" + std::to_string(terminal->player_turn) + ",\"outcome\":\""
        + (terminal->outcome == CombatPredictionOutcome::Victory ? "victory" : "defeat") + "\"}";
}

bool read_bool(const Json& value) {
    if (value.kind != Json::Bool) throw GameError("combat prediction snapshot: invalid history boolean");
    return value.boolean;
}

CombatPredictionHistoryTrace read_history(const Json& json) {
    object_size(json, 2);
    const Json& entries = json.at("events");
    const Json& counts = json.at("counters");
    if (entries.kind != Json::Array || counts.kind != Json::Array || counts.array.size() != 6)
        throw GameError("combat prediction snapshot: invalid history shape");
    CombatPredictionHistoryTrace history;
    if (entries.array.size() > 100000) throw GameError("combat prediction snapshot: history limit exceeded");
    std::vector<bool> completed(entries.array.size(), false);
    for (const Json& entry : entries.array) {
        if (entry.kind != Json::Array || entry.array.size() != 20)
            throw GameError("combat prediction snapshot: invalid history event");
        const int kind = entry.array[0].integer<int>();
        if (kind < 0 || kind > static_cast<int>(CombatPredictionEventKind::CardEffectStarted))
            throw GameError("combat prediction snapshot: unknown history event");
        CombatPredictionEvent event;
        event.kind = static_cast<CombatPredictionEventKind>(kind);
        event.card = entry.array[1].integer<uint16_t>();
        event.card_id = entry.array[2].integer<uint16_t>();
        event.owner = entry.array[3].integer<uint16_t>();
        event.creature = entry.array[4].integer<uint16_t>();
        event.dealer = entry.array[5].integer<uint16_t>();
        event.blocked = entry.array[6].integer<int32_t>();
        event.unblocked = entry.array[7].integer<int32_t>();
        event.overkill = entry.array[8].integer<int32_t>();
        event.original_index = entry.array[9].integer<int32_t>();
        event.hit_count = entry.array[10].integer<int32_t>();
        event.flag = read_bool(entry.array[11]);
        event.props = entry.array[12].integer<int32_t>();
        event.block_broken = read_bool(entry.array[13]);
        event.fully_blocked = read_bool(entry.array[14]);
        event.target_killed = read_bool(entry.array[15]);
        const Json& hits = entry.array[16];
        if (hits.kind != Json::Array || hits.array.size() > 512)
            throw GameError("combat prediction snapshot: invalid attack hit list");
        for (const Json& hit : hits.array) {
            if (hit.kind != Json::Array || hit.array.size() != 8)
                throw GameError("combat prediction snapshot: invalid attack hit");
            event.hit_results.push_back(CombatPredictionHitResult{
                hit.array[0].integer<uint16_t>(), hit.array[1].integer<int32_t>(),
                hit.array[2].integer<int32_t>(), hit.array[3].integer<int32_t>(),
                hit.array[4].integer<int32_t>(), read_bool(hit.array[5]),
                read_bool(hit.array[6]), read_bool(hit.array[7])});
        }
        if (event.kind == CombatPredictionEventKind::CreatureAttacked) {
            if (event.hit_count != static_cast<int32_t>(event.hit_results.size()))
                throw GameError("combat prediction snapshot: attack hit count mismatch");
        } else if (event.hit_count != 0 || !event.hit_results.empty()) {
            throw GameError("combat prediction snapshot: unexpected attack hit list");
        }
        const Json& card_facts = entry.array[17];
        if (!card_facts.is_null()) {
            if (card_facts.kind != Json::Array || card_facts.array.size() != 3)
                throw GameError("combat prediction snapshot: invalid card facts");
            event.card_facts = CombatPredictionCardFacts{
                card_facts.array[0].integer<uint16_t>(), card_facts.array[1].integer<int32_t>(),
                card_facts.array[2].integer<uint8_t>()};
        }
        const bool card_kind = event.kind == CombatPredictionEventKind::CardPlayStarted
            || event.kind == CombatPredictionEventKind::CardPlayFinished
            || event.kind == CombatPredictionEventKind::CardDrawn
            || event.kind == CombatPredictionEventKind::CardDrawResolved
            || event.kind == CombatPredictionEventKind::CardGenerated
            || event.kind == CombatPredictionEventKind::CardGenerationResolved
            || event.kind == CombatPredictionEventKind::CardEffectStarted;
        if ((card_kind && (!event.card || !event.card_facts))
            || (!card_kind && event.kind != CombatPredictionEventKind::DamageReceived && event.card_facts)
            || (event.kind == CombatPredictionEventKind::DamageReceived
                && (event.card != 0) != event.card_facts.has_value()))
            throw GameError("combat prediction snapshot: inconsistent card facts");
        const Json& play_facts = entry.array[18];
        if (!play_facts.is_null()) {
            if (play_facts.kind != Json::Array || play_facts.array.size() != 10)
                throw GameError("combat prediction snapshot: invalid play facts");
            event.play_facts = CombatPredictionPlayFacts{
                play_facts.array[0].integer<uint16_t>(), play_facts.array[1].integer<uint8_t>(),
                play_facts.array[2].integer<int32_t>(), play_facts.array[3].integer<int32_t>(),
                play_facts.array[4].integer<int32_t>(), play_facts.array[5].integer<int32_t>(),
                read_bool(play_facts.array[6]), play_facts.array[7].integer<int32_t>(),
                play_facts.array[8].integer<int32_t>(), play_facts.array[9].integer<uint8_t>()};
        }
        const bool play_kind = event.kind == CombatPredictionEventKind::CardPlayStarted
            || event.kind == CombatPredictionEventKind::CardPlayFinished
            || event.kind == CombatPredictionEventKind::CardEffectStarted;
        if (play_kind != event.play_facts.has_value() || (event.play_facts
            && (event.play_facts->play_count < 1 || event.play_facts->play_index < 0
                || event.play_facts->play_index >= event.play_facts->play_count || event.play_facts->target_type > 9)))
            throw GameError("combat prediction snapshot: inconsistent play facts");
        const Json& damage_source = entry.array[19];
        if (event.kind == CombatPredictionEventKind::DamageReceived) {
            if (damage_source.kind != Json::Array || damage_source.array.size() != 2
                || damage_source.array[1].kind != Json::String)
                throw GameError("combat prediction snapshot: invalid damage source");
            int source_kind = damage_source.array[0].integer<int>();
            if (source_kind < 0 || source_kind > static_cast<int>(CombatPredictionDamageSourceKind::MonsterMove))
                throw GameError("combat prediction snapshot: invalid damage source kind");
            event.damage_source_kind = static_cast<CombatPredictionDamageSourceKind>(source_kind);
            event.damage_source_id = damage_source.array[1].text;
            if ((source_kind == 0) != event.damage_source_id.empty())
                throw GameError("combat prediction snapshot: incomplete damage source");
        } else if (!damage_source.is_null()) {
            throw GameError("combat prediction snapshot: unexpected damage source");
        }
        if (event.kind == CombatPredictionEventKind::CardEffectStarted) {
            CombatPredictionEvent canonical;
            canonical.kind = CombatPredictionEventKind::CardEffectStarted;
            canonical.card = event.card; canonical.card_id = event.card_id; canonical.owner = event.owner;
            canonical.card_facts = event.card_facts; canonical.play_facts = event.play_facts;
            if (event != canonical || event.owner != event.card_facts->owner)
                throw GameError("combat prediction snapshot: unexpected card effect payload");
        }
        if (event.kind == CombatPredictionEventKind::CardDrawResolved
            || event.kind == CombatPredictionEventKind::CardGenerationResolved) {
            const auto parent_kind = event.kind == CombatPredictionEventKind::CardDrawResolved
                ? CombatPredictionEventKind::CardDrawn : CombatPredictionEventKind::CardGenerated;
            if (event.original_index < 0 || static_cast<size_t>(event.original_index) >= history.events.size()
                || history.events[event.original_index].kind != parent_kind
                || completed[event.original_index])
                throw GameError("combat prediction snapshot: invalid deferred history reference");
            completed[event.original_index] = true;
        } else if (event.original_index != -1) {
            throw GameError("combat prediction snapshot: unexpected deferred history reference");
        }
        history.Record(event);
    }
    const CombatPredictionHistoryCounters expected{
        counts.array[0].integer<int32_t>(), counts.array[1].integer<int32_t>(),
        counts.array[2].integer<int32_t>(), counts.array[3].integer<int32_t>(),
        counts.array[4].integer<int32_t>(), counts.array[5].integer<int32_t>()};
    if (history.counters != expected) throw GameError("combat prediction snapshot: history counters mismatch");
    return history;
}

std::string write_history(const CombatPredictionHistoryTrace& history) {
    std::string out = "{\"events\":[";
    bool first = true;
    for (const CombatPredictionEvent& event : history.events) {
        if (!first) out += ',';
        first = false;
        out += '[' + std::to_string(static_cast<int>(event.kind)) + ',' + std::to_string(event.card)
            + ',' + std::to_string(event.card_id) + ',' + std::to_string(event.owner)
            + ',' + std::to_string(event.creature) + ',' + std::to_string(event.dealer)
            + ',' + std::to_string(event.blocked) + ',' + std::to_string(event.unblocked)
            + ',' + std::to_string(event.overkill) + ',' + std::to_string(event.original_index)
            + ',' + std::to_string(event.hit_count) + (event.flag ? ",true," : ",false,")
            + std::to_string(event.props) + (event.block_broken ? ",true," : ",false,")
            + (event.fully_blocked ? "true," : "false,")
            + (event.target_killed ? "true,[" : "false,[");
        bool first_hit = true;
        for (const CombatPredictionHitResult& hit : event.hit_results) {
            if (!first_hit) out += ',';
            first_hit = false;
            out += '[' + std::to_string(hit.receiver) + ',' + std::to_string(hit.props)
                + ',' + std::to_string(hit.blocked) + ',' + std::to_string(hit.unblocked)
                + ',' + std::to_string(hit.overkill) + (hit.block_broken ? ",true," : ",false,")
                + (hit.fully_blocked ? "true," : "false,")
                + (hit.target_killed ? "true]" : "false]");
        }
        out += "],";
        if (event.card_facts) {
            const auto& card = *event.card_facts;
            out += '[' + std::to_string(card.owner) + ',' + std::to_string(card.upgrade)
                + ',' + std::to_string(card.type) + ']';
        } else out += "null";
        out += ',';
        if (event.play_facts) {
            const auto& play = *event.play_facts;
            out += '[' + std::to_string(play.target) + ',' + std::to_string(play.result_pile)
                + ',' + std::to_string(play.energy_spent) + ',' + std::to_string(play.energy_value)
                + ',' + std::to_string(play.stars_spent) + ',' + std::to_string(play.star_value)
                + (play.auto_play ? ",true," : ",false,") + std::to_string(play.play_index)
                + ',' + std::to_string(play.play_count) + ',' + std::to_string(play.target_type) + ']';
        } else out += "null";
        out += ',';
        if (event.kind == CombatPredictionEventKind::DamageReceived)
            out += '[' + std::to_string(static_cast<int>(event.damage_source_kind))
                + ',' + quote(event.damage_source_id) + ']';
        else out += "null";
        out += ']';
    }
    const auto& c = history.counters;
    return out + "],\"counters\":[" + std::to_string(c.finished_plays) + ','
        + std::to_string(c.ethereal_plays) + ',' + std::to_string(c.lightning_channels) + ','
        + std::to_string(c.unblocked_hits_received) + ',' + std::to_string(c.cards_drawn) + ','
        + std::to_string(c.cards_generated) + "]}";
}

bool combat_action(Action::Kind kind) {
    return kind == Action::PlayCard || kind == Action::EndTurn
        || kind == Action::UsePotion || kind == Action::Answer;
}

std::vector<CombatPredictionPowerEvent> read_power_events(const Json& json) {
    if (json.kind != Json::Array || json.array.size() > 100000)
        throw GameError("combat prediction snapshot: invalid power events");
    std::vector<CombatPredictionPowerEvent> result;
    for (const auto& row : json.array) {
        if (row.kind != Json::Array || row.array.size() != 8)
            throw GameError("combat prediction snapshot: invalid power event");
        const int kind = row.array[0].integer<int>();
        if (kind < 0 || kind > 2) throw GameError("combat prediction snapshot: unknown power event");
        CombatPredictionPowerEvent event{static_cast<CombatPredictionPowerEventKind>(kind),
            row.array[1].integer<uint16_t>(), row.array[2].integer<uint16_t>(), row.array[3].integer<uint32_t>(),
            row.array[4].integer<uint16_t>(), row.array[5].integer<int32_t>(), row.array[6].integer<int32_t>(),
            read_bool(row.array[7])};
        if (!event.power || event.power > GameState{}.powers.capacity()
            || event.owner > GameState{}.creatures.capacity()
            || !event.power_id || event.power_id >= std::size(kPowerIdNames)
            || (kind == 0 ? event.previous_amount == event.amount
                : event.previous_amount != event.amount || !event.attached))
            throw GameError("combat prediction snapshot: inconsistent power event");
        result.push_back(event);
    }
    return result;
}

std::string write_power_events(const std::vector<CombatPredictionPowerEvent>& events) {
    JsonWriter writer;
    writer.begin_array();
    for (const auto& event : events)
        writer.begin_array().value(static_cast<int32_t>(event.kind)).value(event.power).value(event.owner)
            .value(uint64_t(event.owner_combat_id)).value(event.power_id).value(event.previous_amount)
            .value(event.amount).value(event.attached).end_array();
    writer.end_array();
    return writer.str();
}

void hash_power_events(StateHasher& hash, const std::vector<CombatPredictionPowerEvent>& events) {
    hash(static_cast<uint32_t>(events.size()));
    for (const auto& event : events) {
        hash(event.kind); hash(event.power); hash(event.owner); hash(event.owner_combat_id);
        hash(event.power_id); hash(event.previous_amount); hash(event.amount); hash(event.attached);
    }
}

void hash_terminal(StateHasher& hash, const std::optional<CombatPredictionTerminalStamp>& terminal) {
    hash(terminal.has_value());
    if (terminal) {
        hash(terminal->player_turn);
        hash(terminal->outcome);
    }
}
void hash_history(StateHasher& hash, const CombatPredictionHistoryTrace& history) {
    hash(static_cast<uint32_t>(history.events.size()));
    for (const CombatPredictionEvent& event : history.events) {
        hash(event.kind); hash(event.card); hash(event.card_id); hash(event.owner);
        hash(event.creature); hash(event.dealer); hash(event.blocked);
        hash(event.unblocked); hash(event.overkill); hash(event.original_index);
        hash(event.hit_count); hash(event.flag); hash(event.props);
        hash(event.block_broken); hash(event.fully_blocked); hash(event.target_killed);
        hash(event.damage_source_kind);
        hash(static_cast<uint32_t>(event.damage_source_id.size()));
        for (unsigned char ch : event.damage_source_id) hash(ch);
        hash(static_cast<uint32_t>(event.hit_results.size()));
        for (const CombatPredictionHitResult& hit : event.hit_results) {
            hash(hit.receiver); hash(hit.props); hash(hit.blocked); hash(hit.unblocked);
            hash(hit.overkill); hash(hit.block_broken); hash(hit.fully_blocked); hash(hit.target_killed);
        }
        hash(event.card_facts.has_value());
        if (event.card_facts) {
            hash(event.card_facts->owner); hash(event.card_facts->upgrade); hash(event.card_facts->type);
        }
        hash(event.play_facts.has_value());
        if (event.play_facts) {
            const auto& play = *event.play_facts;
            hash(play.target); hash(play.result_pile);
            hash(play.energy_spent); hash(play.energy_value);
            hash(play.stars_spent); hash(play.star_value);
            hash(play.auto_play); hash(play.play_index); hash(play.play_count); hash(play.target_type);
        }
    }
    const auto& c = history.counters;
    hash(c.finished_plays); hash(c.ethereal_plays); hash(c.lightning_channels);
    hash(c.unblocked_hits_received); hash(c.cards_drawn); hash(c.cards_generated);
}
}  // namespace

void CombatPredictionSession::ValidateBoundary() const {
    const auto& game = *env_->g_;
    if (game.run.phase != RunPhase::Combat || !game.combat.exists || !game.player_combat.exists)
        throw GameError("combat prediction: session left combat");
    if (boundary_.terminal) {
        if (env_->choice_ || env_->pending_ || replay_boundary_base_)
            throw GameError("combat prediction: terminal session has a pending choice");
        if (game.combat.is_in_progress || game.combat.pending_loss
            || boundary_.terminal->player_turn < 1
            || boundary_.terminal->player_turn != game.player_combat.turn_number)
            throw GameError("combat prediction: terminal flags or turn mismatch");
    } else if (!game.combat.is_in_progress) {
        throw GameError("combat prediction: inactive combat without a terminal stamp");
    }
    if (env_->choice_.has_value() != replay_boundary_base_.has_value())
        throw GameError("combat prediction: incomplete choice boundary replay state");
    if (replay_boundary_base_ && replay_boundary_base_->terminal)
        throw GameError("combat prediction: a terminal boundary cannot be a choice base");
    if (replay_boundary_base_) {
        const auto& power_prefix = replay_boundary_base_->power_events;
        const auto& power_events = boundary_.power_events;
        if (power_prefix.size() > power_events.size() || !std::equal(power_prefix.begin(), power_prefix.end(), power_events.begin()))
            throw GameError("combat prediction: choice power facts prefix mismatch");
        const auto& prefix = replay_boundary_base_->history.events;
        const auto& events = boundary_.history.events;
        if (prefix.size() > events.size() || !std::equal(prefix.begin(), prefix.end(), events.begin()))
            throw GameError("combat prediction: choice history does not extend its action base");
    }
}

std::unique_ptr<CombatPredictionSession> CombatPredictionSession::FromGameSnapshot(std::string_view json) {
    CombatPredictionBoundary boundary;
    auto session = std::unique_ptr<CombatPredictionSession>(
        new CombatPredictionSession(Env::ImportSnapshotJsonImpl(json, &boundary)));
    session->boundary_ = boundary;
    if (session->env_->choice_)
        throw GameError("combat prediction: a pending game choice has no policy history action base");
    session->ValidateBoundary();
    return session;
}

std::unique_ptr<CombatPredictionSession> CombatPredictionSession::ImportSnapshotJson(std::string_view json) {
    const Json document = snapshot_detail::Parser(json).parse();
    object_size(document, 9);
    const Json& format = document.at("format");
    if (format.kind != Json::String || format.text != "sts2env.combat_prediction"
        || document.at("version").integer<int>() != 7)
        throw GameError("combat prediction snapshot: incompatible envelope");
    const Json& projection = document.at("history_projection_complete");
    if (projection.kind != Json::Bool || projection.boolean)
        throw GameError("combat prediction snapshot: unsupported complete history claim");
    CombatPredictionBoundary boundary;
    boundary.terminal = read_terminal(document.at("terminal"));
    boundary.history = read_history(document.at("history"));
    boundary.power_events = read_power_events(document.at("power_events"));
    std::optional<CombatPredictionBoundary> base;
    const Json& replay = document.at("replay_terminal_base");
    if (!replay.is_null()) {
        object_size(replay, 3);
        base.emplace();
        base->terminal = read_terminal(replay.at("terminal"));
        base->history = read_history(replay.at("history"));
        base->power_events = read_power_events(replay.at("power_events"));
        if (base->terminal) throw GameError("combat prediction snapshot: terminal choice base");
    }
    auto session = std::unique_ptr<CombatPredictionSession>(new CombatPredictionSession(
        Env::ImportSnapshotJsonImpl(serialize(document.at("inner_snapshot")), &boundary,
            base ? &*base : nullptr)));
    session->boundary_ = boundary;
    session->replay_boundary_base_ = base;
    session->ValidateBoundary();
    if (document.at("prediction_hash").integer<uint64_t>() != session->Hash())
        throw GameError("combat prediction snapshot: hash mismatch");
    return session;
}

std::unique_ptr<CombatPredictionSession> CombatPredictionSession::Clone() const {
    auto clone = std::unique_ptr<CombatPredictionSession>(new CombatPredictionSession(env_->Clone()));
    clone->boundary_ = boundary_;
    clone->replay_boundary_base_ = replay_boundary_base_;
    return clone;
}

std::vector<Action> CombatPredictionSession::LegalActions() const {
    if (terminal()) return {};
    auto actions = env_->LegalActions();
    std::erase_if(actions, [](const Action& action) { return !combat_action(action.kind); });
    return actions;
}

std::unique_ptr<CombatPredictionSession> CombatPredictionSession::Predict(const Action& action) const {
    if (terminal()) throw GameError("combat prediction: cannot advance a terminal session");
    if (!combat_action(action.kind)) throw GameError("combat prediction: action is not a combat transition");
    if (!env_->choice_) env_->ValidateCombatRoot();
    const auto legal = LegalActions();
    if (std::find(legal.begin(), legal.end(), action) == legal.end())
        throw GameError("combat prediction: illegal action");
    auto branch = Clone();
    if (action.kind == Action::Answer) {
        if (!branch->replay_boundary_base_) throw GameError("combat prediction: missing choice boundary base");
        branch->boundary_ = *branch->replay_boundary_base_;
    } else {
        branch->replay_boundary_base_ = branch->boundary_;
    }
    {
        CombatPredictionScope scope(branch->env_->sim(), branch->boundary_);
        branch->env_->Step(action);
    }
    if (!branch->env_->choice_) branch->replay_boundary_base_.reset();
    branch->ValidateBoundary();
    if (!branch->env_->choice_) branch->env_->ValidateCombatRoot();
    return branch;
}

uint64_t CombatPredictionSession::Hash() const {
    StateHasher hash;
    constexpr std::string_view domain = "sts2env.combat_prediction/v7";
    hash(static_cast<uint32_t>(domain.size()));
    for (const unsigned char byte : domain) hash(byte);
    hash(env_->Hash());
    hash_terminal(hash, boundary_.terminal);
    hash_history(hash, boundary_.history);
    hash_power_events(hash, boundary_.power_events);
    hash(replay_boundary_base_.has_value());
    if (replay_boundary_base_) {
        hash_terminal(hash, replay_boundary_base_->terminal);
        hash_history(hash, replay_boundary_base_->history);
        hash_power_events(hash, replay_boundary_base_->power_events);
    }
    return hash.finish();
}

std::string CombatPredictionSession::ExportSnapshotJson() const {
    const std::string base = replay_boundary_base_
        ? "{\"terminal\":" + write_terminal(replay_boundary_base_->terminal)
            + ",\"history\":" + write_history(replay_boundary_base_->history)
            + ",\"power_events\":" + write_power_events(replay_boundary_base_->power_events) + '}' : "null";
    return "{\"format\":\"sts2env.combat_prediction\",\"version\":7,\"inner_snapshot\":"
        + env_->ExportSnapshotJson() + ",\"terminal\":" + write_terminal(boundary_.terminal)
        + ",\"history\":" + write_history(boundary_.history)
        + ",\"power_events\":" + write_power_events(boundary_.power_events)
        + ",\"history_projection_complete\":false,\"replay_terminal_base\":" + base
        + ",\"prediction_hash\":" + std::to_string(Hash()) + '}';
}

}  // namespace sts2
