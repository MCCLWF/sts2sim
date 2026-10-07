// `replay`: replays a run recorded by the oracle (tools/Sts2Oracle/OracleRun.cs)
// and prints the simulator's decision points in the same format, for L4
// (tests/l4/l4_run.py).
//
//   replay --seed S --character C [--ascension N] [--no-neow 1] [--hp N]
//          [--deck CARD[+N],...] [--relics ID,...]   (actions on stdin, one per line)
//
// Actions (tests/l4/l4_run.py writes them from the oracle's action events):
//   map COL ROW | play HAND TARGET | end | reward I | proceed | select N I1..IN
//   | event I | rest I | relic I | relicskip | shop I | leave
// A rewards screen inside an effect (a nested choice, RewardsCmd) takes
// `reward I` / `proceed` too.
// Output, one JSON object per line: every decision point as
//   {"event":"decision","kind":...,"run":...,"options":...}   (combat: "combat")
// then {"event":"end","reason":"actions"|"ended","run":...}, or
//   {"event":"error","action_index":K,"message":"..."}.
#include "tools/cli/replay_cmd.h"
#include "tools/cli/oracle_actions.h"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "engine/json.h"
#include "engine/save_json.h"
#include "engine/sim/alloc.h"
#include "engine/sim/errors.h"
#include "env/env.h"
#include "env/run_queries.h"
#include "game/All.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Entities/RestSite/RestSiteOption.h"
#include "game/Models/Relics/DustyTome.h"
#include "game/Runs/RunFlow.h"
#include "tools/cli/dump.h"

namespace sts2 {

namespace {

void emit(JsonWriter& w) {
    std::fwrite(w.str().data(), 1, w.str().size(), stdout);
    std::fputc('\n', stdout);
}

// Privileged endpoint diagnostics: observe the generated map and complete
// RNG state without generating a replacement map or advancing any stream.
void write_boundary_state(JsonWriter& w, const GameState& g, bool includeMap = true) {
    const auto write_rng = [&](const Rng& rng) {
        w.begin_object().field("counter", rng.counter)
            .field("state0", rng.random.s0).field("state1", rng.random.s1)
            .field("state2", rng.random.s2).field("state3", rng.random.s3).end_object();
    };
    static constexpr const char* run_names[] = {"UpFront", "Shuffle", "UnknownMapPoint", "CombatCardGeneration",
        "CombatPotionGeneration", "CombatCardSelection", "CombatEnergyCosts", "CombatTargets", "MonsterAi",
        "Niche", "CombatOrbs", "TreasureRoomRelics"};
    static constexpr const char* player_names[] = {"rewards", "shops", "transformations"};
    w.begin_object();
    if (includeMap) {
        w.key("map");
        write_map(w, g.run.map);
    }
    w.key("run_rng").begin_object();
    for (size_t i = 0; i < g.run.rng.rngs.size(); ++i) {
        w.key(run_names[i]); write_rng(g.run.rng.rngs[i]);
    }
    w.end_object().key("player_rng").begin_object();
    for (size_t i = 0; i < g.run.player.rng.rngs.size(); ++i) {
        w.key(player_names[i]); write_rng(g.run.player.rng.rngs[i]);
    }
    w.end_object().end_object();
}

std::vector<std::string> split_loadout(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, sep)) if (!item.empty()) out.push_back(item);
    return out;
}

CardId parse_loadout_card(const std::string& entry) {
    for (size_t i = 1; i < kCardIdNames.size(); ++i)
        if (kCardIdNames[i] == entry) return static_cast<CardId>(i);
    throw GameError("unknown card " + entry);
}

RelicId parse_loadout_relic(const std::string& entry) {
    for (size_t i = 1; i < kRelicIdNames.size(); ++i)
        if (kRelicIdNames[i] == entry) return static_cast<RelicId>(i);
    throw GameError("unknown relic " + entry);
}

void customize_loadout(Sim* sim, const std::string& deck, const std::string& relics) {
    if (deck.empty() && relics.empty()) return;
    Player player(sim, 1);
    if (!deck.empty()) for (CardModel card : player.Deck().Cards()) CardPileCmd::RemoveFromDeck(card, false);
    for (const std::string& spec : split_loadout(deck, ',')) {
        std::vector<std::string> parts = split_loadout(spec, '+');
        CardModel card = RunState(sim).CreateCard(parse_loadout_card(parts.at(0)), player);
        CardPileCmd::Add(card, PileType::Deck, CardPilePosition::Bottom, AbstractModel{}, true);
        int upgrades = parts.size() == 2 ? std::stoi(parts[1]) : 0;
        if (upgrades < 0) throw GameError("negative card upgrade: " + spec);
        for (int i = 0; i < upgrades; ++i) CardCmd::Upgrade(card);
    }
    for (const std::string& id : split_loadout(relics, ',')) {
        RelicId relicId = parse_loadout_relic(id);
        RelicModel relic(sim, alloc_relic(*sim->g, relicId, 0));
        if (relicId == RelicId::DUSTY_TOME)
            (void)DustyTome(sim, relic.Index()).SetupForPlayer(player);
        RelicCmd::Obtain(relic, player);
    }
}

void write_rewards(JsonWriter& w, Sim* sim) {
    w.field("kind", "rewards");
    w.key("options").begin_array();
    for (Reward r : RunFlow::RewardOptions(sim)) write_reward(w, sim, r);
    w.end_array();
    w.key("run");
    write_run(w, sim);
}

void write_choice_decision(JsonWriter& w, Sim* sim, const ChoiceRequest& r) {
    w.begin_object();
    w.field("event", "decision");
    if (r.source == "rewards") {
        write_rewards(w, sim);
    } else if (r.kind == ChoiceKind::Bundle) {
        w.field("kind", "bundle_select");
        w.key("options").begin_object();
        w.key("bundles").begin_array();
        for (const ChoiceOption& option : r.options) {
            w.begin_array();
            for (uint32_t card : option.cards) write_card_name(w, sim, card);
            w.end_array();
        }
        w.end_array();
        w.end_object();
        w.key("run");
        write_run(w, sim);
    } else if (r.kind == ChoiceKind::Option) {
        w.field("kind", "choice");
        w.key("options").begin_object();
        w.field("source", r.source).field("prompt", r.prompt).field("option_count", r.option_count)
         .field("min_select", r.min_select).field("max_select", r.max_select)
         .field("subset", r.subset).field("can_skip", r.can_skip);
        w.key("options").begin_array();
        for (size_t i = 0; i < r.options.size(); ++i) {
            const ChoiceOption& option = r.options[i];
            const char* kind = option.kind == ChoiceOptionKind::Card ? "card"
                : option.kind == ChoiceOptionKind::Bundle ? "bundle"
                : option.kind == ChoiceOptionKind::Reward ? "reward"
                : option.kind == ChoiceOptionKind::Proceed ? "proceed" : "alternative";
            w.begin_object().field("index", static_cast<int32_t>(i)).field("key", option.key).field("kind", kind).end_object();
        }
        w.end_array().end_object();
        w.key("run");
        write_run(w, sim);
    } else {
        w.field("kind", "card_select");
        w.key("options").begin_object();
        w.key("cards").begin_array();
        for (uint32_t card : r.cards) write_card_name(w, sim, card);
        w.end_array();
        w.field("min", r.min_select);
        w.field("max", r.max_select);
        w.field("can_skip", r.can_skip);
        w.field("source", r.source);
        if (r.prompt == "CardReward") {
            w.key("alternatives").begin_array();
            bool can_reroll = false;
            for (const ChoiceOption& option : r.options) {
                if (option.kind != ChoiceOptionKind::Alternative) continue;
                w.value(option.key);
                can_reroll |= option.key == "REROLL";
            }
            w.end_array().field("can_reroll", can_reroll);
        }
        w.end_object();
        w.key("run");
        write_run(w, sim);
    }
    w.end_object();
}

std::vector<int32_t> setup_choice_answers(const ChoiceRequest& r, const std::vector<std::string>& action) {
    auto number = [&](size_t i) { return std::stoi(action.at(i)); };
    if (r.source == "rewards") {
        if (action.at(0) == "proceed") return {r.option_count - 1};
        if (action.at(0) == "reward") return {number(1)};
        throw GameError("replay: expected a reward action for a loadout reward choice");
    }
    if (r.kind == ChoiceKind::Bundle) {
        if (action.at(0) != "bundle") throw GameError("replay: expected a bundle action for a loadout choice");
        return {number(1)};
    }
    if (action.at(0) != "select") throw GameError("replay: expected a card selection for a loadout choice");
    int count = number(1);
    if (r.subset) {
        if (count < 0 || static_cast<size_t>(count + 2) != action.size())
            throw GameError("replay: loadout card selection count mismatch");
        std::vector<int32_t> answers{count};
        for (int i = 0; i < count; ++i) answers.push_back(number(static_cast<size_t>(i + 2)));
        return answers;
    }
    if (count == 0) return {r.option_count};
    return {number(2)};
}

void customize_loadout_with_choices(Env& env, const std::string& deck, const std::string& relics,
                                    const std::vector<std::vector<std::string>>& actions, size_t& action_index) {
    const GameState base = env.state();
    std::vector<int32_t> answers;
    for (;;) {
        env.state() = base;
        // Oracle's test route records starting relic selections against a
        // temporary MapPointHistoryEntry (RoomType.Map), then removes it once
        // the loadout is complete. Mirror only the fields observable during
        // those callbacks; restore them below before StartRun.
        env.state().run.total_floor = static_cast<int32_t>(base.run.total_floor + 1);
        env.state().run.current_entry_rooms = static_cast<uint16_t>(1u << static_cast<unsigned>(RoomType::Map));
        env.sim().set_prefix(answers);
        try {
            customize_loadout(&env.sim(), deck, relics);
            if (!env.sim().prefix_consumed()) throw GameError("replay: loadout did not consume every setup choice");
            env.state().run.total_floor = base.run.total_floor;
            env.state().run.current_entry_rooms = base.run.current_entry_rooms;
            return;
        } catch (const NeedChoice& need) {
            JsonWriter w;
            write_choice_decision(w, &env.sim(), need.request);
            emit(w);
            if (action_index >= actions.size()) throw GameError("replay: oracle trace ended during starting relic selection");
            std::vector<int32_t> next = setup_choice_answers(need.request, actions[action_index++]);
            answers.insert(answers.end(), next.begin(), next.end());
        }
    }
}

// OracleRun.ChooseShop: the stocked entries.
void write_shop_entry(JsonWriter& w, Sim* sim, Merchant::EntryRef e) {
    const MerchantState& s = sim->g->run.merchant;
    w.begin_object();
    switch (e.kind) {
    case Merchant::EntryRef::CharacterCard:
    case Merchant::EntryRef::ColorlessCard: {
        const MerchantCardEntryState& c = e.kind == Merchant::EntryRef::CharacterCard ? s.character_cards[e.index] : s.colorless_cards[e.index];
        w.field("kind", "card");
        w.field("id", CardModel(sim, c.card).data().entry);
        break;
    }
    case Merchant::EntryRef::Relic:
        w.field("kind", "relic");
        w.field("id", RelicModel(sim, s.relics[e.index].relic).data().entry);
        break;
    case Merchant::EntryRef::Potion:
        w.field("kind", "potion");
        w.field("id", PotionModel(sim, s.potions[e.index].potion).data().entry);
        break;
    case Merchant::EntryRef::CardRemoval:
        w.field("kind", "remove");
        w.field("id", "remove");
        break;
    }
    w.field("cost", Merchant::Cost(sim, e));
    w.field("affordable", Merchant::EnoughGold(sim, e));
    w.end_object();
}

// The decision the run waits for, in the oracle's format. False when the run ended.
bool write_decision(JsonWriter& w, Env& env, bool snapshots) {
    Sim* sim = &env.sim();
    Decision d = env.decision();
    w.begin_object();
    w.field("event", "decision");
    switch (d.kind) {
    case DecisionKind::Map: {
        w.field("kind", "map");
        w.key("options").begin_array();
        for (MapCoord c : RunFlow::MapOptions(sim)) {
            w.begin_array();
            w.value(static_cast<int32_t>(c.col));
            w.value(static_cast<int32_t>(c.row));
            w.end_array();
        }
        w.end_array();
        w.key("run");
        write_run(w, sim);
        break;
    }
    case DecisionKind::Combat:
        w.field("kind", "combat");
        w.key("combat");
        write_combat_state(w, sim);
        // tests/live/live_capture_diff.py --runs: the logical state the mod's
        // capture of the same live decision is compared with.
        if (snapshots) w.key("snapshot").raw_json(env.ExportSnapshotJson());
        break;
    case DecisionKind::Rewards: write_rewards(w, sim); break;
    case DecisionKind::RestSite:
        w.field("kind", "rest");
        w.key("options").begin_array();
        for (const RunFlow::RestSiteOptionView& o : RunFlow::RestSiteOptions(sim)) {
            w.begin_object();
            w.field("id", RestSite::OptionId(o.id));
            w.field("enabled", o.enabled);
            w.end_object();
        }
        w.end_array();
        w.key("run");
        write_run(w, sim);
        break;
    case DecisionKind::Shop:
        w.field("kind", "shop");
        w.key("options").begin_array();
        for (Merchant::EntryRef e : RunFlow::ShopOptions(sim)) write_shop_entry(w, sim, e);
        w.end_array();
        w.key("run");
        write_run(w, sim);
        break;
    case DecisionKind::Treasure:
        w.field("kind", "relic_select");
        w.key("options").begin_object();
        w.key("relics").begin_array();
        for (RelicId r : RunFlow::TreasureRelics(sim)) w.value(data(r).entry);
        w.end_array();
        w.field("can_skip", true);
        w.end_object();
        w.key("run");
        write_run(w, sim);
        break;
    case DecisionKind::Event: {
        w.field("kind", "event");
        w.key("options").begin_object();
        w.field("event", data(EventModel(sim).EventId_()).entry);
        w.key("options").begin_array();
        for (const EventOptionState& o : RunFlow::EventOptions(sim)) {
            w.begin_object();
            w.field("key", LocKey{o.text_key}.str());
            w.field("locked", o.is_locked);
            w.end_object();
        }
        w.end_array();
        w.end_object();
        w.key("run");
        write_run(w, sim);
        break;
    }
    case DecisionKind::Choice: {
        const ChoiceRequest& r = d.request;
        if (r.source == "rewards") {
            write_rewards(w, sim);
            break;
        }
        if (r.kind == ChoiceKind::Bundle) {
            w.field("kind", "bundle_select");
            w.key("options").begin_object();
            w.key("bundles").begin_array();
            for (const auto& option : r.options) {
                w.begin_array();
                for (auto card : option.cards) write_card_name(w, sim, card);
                w.end_array();
            }
            w.end_array();
            w.end_object();
            w.key("run");
            write_run(w, sim);
            break;
        }
        if (r.kind == ChoiceKind::Option) {
            w.field("kind", "choice");
            w.key("options").begin_object();
            w.field("source", r.source).field("prompt", r.prompt).field("option_count", r.option_count)
             .field("min_select", r.min_select).field("max_select", r.max_select)
             .field("subset", r.subset).field("can_skip", r.can_skip);
            w.key("options").begin_array();
            for (size_t i = 0; i < r.options.size(); ++i) {
                const ChoiceOption& option = r.options[i];
                const char* kind = option.kind == ChoiceOptionKind::Card ? "card"
                    : option.kind == ChoiceOptionKind::Bundle ? "bundle"
                    : option.kind == ChoiceOptionKind::Reward ? "reward"
                    : option.kind == ChoiceOptionKind::Proceed ? "proceed" : "alternative";
                w.begin_object().field("index", static_cast<int32_t>(i)).field("key", option.key).field("kind", kind).end_object();
            }
            w.end_array().end_object();
            w.key("run");
            write_run(w, sim);
            break;
        }
        w.field("kind", "card_select");
        w.key("options").begin_object();
        w.key("cards").begin_array();
        for (uint32_t c : r.cards) write_card_name(w, sim, c);
        w.end_array();
        w.field("min", r.min_select);
        w.field("max", r.max_select);
        w.field("can_skip", r.can_skip);
        w.field("source", r.source);
        if (r.prompt == "CardReward") {
            w.key("alternatives").begin_array();
            bool can_reroll = false;
            for (const auto& option : r.options) {
                if (option.kind != ChoiceOptionKind::Alternative) continue;
                w.value(option.key);
                can_reroll |= option.key == "REROLL";
            }
            w.end_array().field("can_reroll", can_reroll);
        }
        w.end_object();
        w.key("run");
        write_run(w, sim);
        break;
    }
    case DecisionKind::Ended: w.end_object(); return false;
    }
    w.end_object();
    return true;
}

std::vector<std::string> words(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream in(line);
    std::string w;
    while (in >> w) out.push_back(w);
    return out;
}

void ApplyAction(Env& env, const std::vector<std::string>& a) {
    offline::ApplyOracleAction(env, a, [&](const Action& action) { env.Step(action); });
}

}  // namespace

int run_replay(const std::string& seed, CharacterId character, int ascension, bool noNeow, int startHp,
               const std::string& deck, const std::string& relics, bool run_queries, const std::string& stop,
               bool verify_snapshots, bool snapshots) {
    std::vector<std::vector<std::string>> actions;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::vector<std::string> a = words(line);
        if (!a.empty()) actions.push_back(std::move(a));
    }
    Env env;
    size_t k = 0;
    try {
        new_run(env.state(), NewRunOptions{.seed = seed, .character = character, .ascension = ascension});
        if (noNeow) env.state().run.started_with_neow = false;  // ExtraRunFields.StartedWithNeow
        if (startHp > 0) {
            // The oracle's --hp: Creature.SetMaxHpInternal, SetCurrentHpInternal.
            CreatureState& c = env.state().creatures[env.state().run.player.creature];
            c.max_hp = std::min(startHp, 999999999);
            c.current_hp = std::min(c.current_hp, c.max_hp);
            c.current_hp = std::min(startHp, c.max_hp);
        }
        bool multiplayerLoadout = false;
        if (stop == "after-loadout") {
            for (const std::string& id : split_loadout(relics, ','))
                multiplayerLoadout = multiplayerLoadout || id == "MASSIVE_SCROLL";
        }
        if (multiplayerLoadout) env.state().run.player_count = 2;
        customize_loadout_with_choices(env, deck, relics, actions, k);
        env.state().run.player_count = 1;
        if (stop == "after-loadout") {
            JsonWriter e;
            e.begin_object();
            e.field("event", "end");
            e.field("reason", "boundary");
            e.field("stop", "after-loadout");
            e.key("run");
            write_run(e, &env.sim());
            e.key("boundary_state");
            write_boundary_state(e, env.state(), false);
            if (run_queries) { e.key("queries"); WriteRunQueries(e, env); }
            e.end_object();
            emit(e);
            return 0;
        }
        env.Step(Action{.kind = Action::StartRun});
        for (;;) {
            std::unique_ptr<Env> restored;
            if (verify_snapshots) {
                restored = Env::ImportSnapshotJson(env.ExportSnapshotJson());
                if (restored->Hash() != env.Hash() || restored->LegalActions() != env.LegalActions())
                    throw GameError("replay: snapshot restore changed the decision");
            }
            JsonWriter w;
            if (!write_decision(w, env, snapshots)) {
                JsonWriter e;
                e.begin_object();
                e.field("event", "end");
                const RunData& run = env.state().run;
                const bool player_dead = Player(&env.sim(), 1).Creature().IsDead();
                e.field("reason", run.is_victory ? "victory" : (player_dead ? "death" : "ended"));
                if (run.is_victory && stop == "victory") e.field("stop", "victory");
                e.key("run");
                write_run(e, &env.sim());
                if (run_queries) { e.key("queries"); WriteRunQueries(e, env); }
                e.end_object();
                emit(e);
                return 0;
            }
            if (k == actions.size()) {
                JsonWriter e;
                e.begin_object();
                e.field("event", "end");
                const auto& run = env.state().run;
                bool boundary = false;
                if (run.phase == RunPhase::Map) {
                    if (stop == "after-act1" || stop == "after-act2")
                        boundary = run.current_act_index == (stop == "after-act1" ? 1 : 2)
                            && run.visited_map_coords.empty();
                    else if (stop == "before-act1-boss" && run.current_act_index == 0)
                        for (const auto coord : RunFlow::MapOptions(&env.sim()))
                            boundary = boundary || RunFlow::IsBossPoint(&env.sim(), coord);
                }
                e.field("reason", boundary ? "boundary" : "actions");
                if (boundary) e.field("stop", stop);
                e.key("run");
                write_run(e, &env.sim());
                if (boundary) { e.key("boundary_state"); write_boundary_state(e, env.state()); }
                if (run_queries) { e.key("queries"); WriteRunQueries(e, env); }
                e.end_object();
                emit(e);
                return 0;
            }
            if (run_queries) {
                JsonWriter q;
                q.begin_object().field("event", "run_query").key("queries");
                WriteRunQueries(q, env);
                q.end_object();
                emit(q);
            }
            emit(w);
            // Preserve the actual current decision before an incompatible
            // oracle action can fail during successor verification. Otherwise
            // the mismatch is hidden by a later "not on the map" error.
            if (restored) {
                auto reference = env.Clone();
                ApplyAction(*restored, actions[k]);
                ApplyAction(*reference, actions[k]);
                if (restored->Hash() != reference->Hash())
                    throw GameError("replay: snapshot restore changed the successor");
            }
            ApplyAction(env, actions[k]);
            ++k;
        }
    } catch (const std::exception& ex) {
        JsonWriter e;
        e.begin_object();
        e.field("event", "error");
        e.field("action_index", static_cast<int32_t>(k));
        e.field("unported", dynamic_cast<const Unported*>(&ex) != nullptr);
        e.field("message", std::string(ex.what()));
        e.end_object();
        emit(e);
        return 1;
    }
}

}  // namespace sts2
