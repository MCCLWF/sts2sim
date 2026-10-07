// `combat`: replays an oracle combat (tools/Sts2Oracle/OracleCombat.cs) and
// prints the same JSON lines, for L3 (tests/l3/l3_combat.py).
//
//   combat --seed S --character C [--ascension N] --coord COL,ROW [--encounter E] --actions A
//
// A is a comma-separated list of the oracle's actions, in order: `p<hand>:<target>`
// (play the card at that hand index; target is a combat id, or -1 for none) or
// `e` (end the turn). Output, one JSON object per line:
//   {"event":"state","state":{...}}   before every action (OracleCombat.Dump)
//     with `--snapshots 1` the line also carries "snapshot", the Env logical
//     state at that decision (tests/live/live_capture_diff.py compares it with
//     the mod's capture of the same live game state)
//   {"event":"end","player_hp":N,"rng":{...}}
//   {"event":"error","message":"..."} when the replay cannot go on
#include "tools/cli/combat_cmd.h"
#include "tools/cli/dump.h"

#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "engine/json.h"
#include "engine/run_setup.h"
#include "engine/sim/errors.h"
#include "env/env.h"
#include "game/All.h"
#include "game/Rewards/Reward.h"
#include "game/Combat/CombatManager.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/PotionCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/GameActions/ActionExecutor.h"
#include "game/MonsterMoves/Intents/AttackIntent.h"
#include "game/Models/Cards/MadScience.h"
#include "game/Runs/RunManager.h"

namespace sts2 {

namespace {

void emit(JsonWriter& w) {
    std::string& s = w.str();
    std::fwrite(s.data(), 1, s.size(), stdout);
    std::fputc('\n', stdout);
}

// OracleCombat.AtPlayPhase (the action executor is idle between calls here).
bool at_play_phase(Sim* sim) {
    CombatState combat(sim);
    PlayerCombatState pcs = Player(sim, 1).PlayerCombatState();
    return combat.CurrentSide() == CombatSide::Player && pcs && pcs.Phase() == PlayerTurnPhase::Play;
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, sep)) {
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

EncounterId parse_encounter(const std::string& entry) {
    for (size_t i = 1; i < kEncounterIdNames.size(); ++i) {
        if (kEncounterIdNames[i] == entry) return static_cast<EncounterId>(i);
    }
    throw GameError("unknown encounter " + entry);
}

CardId parse_card(const std::string& entry) {
    for (size_t i = 1; i < kCardIdNames.size(); ++i)
        if (kCardIdNames[i] == entry) return static_cast<CardId>(i);
    throw GameError("unknown card " + entry);
}

RelicId parse_relic(const std::string& entry) {
    for (size_t i = 1; i < kRelicIdNames.size(); ++i)
        if (kRelicIdNames[i] == entry) return static_cast<RelicId>(i);
    throw GameError("unknown relic " + entry);
}

PotionId parse_potion(const std::string& entry) {
    for (size_t i = 1; i < kPotionIdNames.size(); ++i)
        if (kPotionIdNames[i] == entry) return static_cast<PotionId>(i);
    throw GameError("unknown potion " + entry);
}

std::pair<CardType, MadScienceRider> parse_mad_science_config(const std::string& config);

void customize_loadout(Sim* sim, const std::string& deck, const std::string& relics, const std::string& potion,
                       const std::string& madScienceConfig) {
    if (deck.empty() && relics.empty() && potion.empty() && madScienceConfig.empty()) return;
    Player player(sim, 1);
    if (!deck.empty())
        for (CardModel card : player.Deck().Cards()) CardPileCmd::RemoveFromDeck(card, false);
    for (const std::string& spec : split(deck, ',')) {
        std::vector<std::string> parts = split(spec, '+');
        CardModel card = RunState(sim).CreateCard(parse_card(parts.at(0)), player);
        CardPileCmd::Add(card, PileType::Deck, CardPilePosition::Bottom, AbstractModel{}, true);
        int upgrades = parts.size() == 2 ? std::stoi(parts[1]) : 0;
        if (upgrades < 0) throw GameError("negative card upgrade: " + spec);
        for (int i = 0; i < upgrades; ++i) CardCmd::Upgrade(card);
        if (!madScienceConfig.empty() && card.CardId_() == CardId::MAD_SCIENCE) {
            const auto [type, rider] = parse_mad_science_config(madScienceConfig);
            MadScience madScience(card);
            madScience.SetTinkerTimeType(type);
            madScience.SetTinkerTimeRider(rider);
        }
    }
    for (const std::string& id : split(relics, ',')) RelicCmd::Obtain(parse_relic(id), player);
    if (!potion.empty()) {
        Player::PotionProcureResult result = PotionCmd::TryToProcure(parse_potion(potion), player);
        if (!result.success) throw GameError("could not procure test potion " + potion);
    }
}

std::pair<CardType, MadScienceRider> parse_mad_science_config(const std::string& config) {
    const std::vector<std::string> parts = split(config, ':');
    if (parts.size() != 2) throw GameError("mad-science config must be TYPE:RIDER");
    CardType type;
    if (parts[0] == "ATTACK") type = CardType::Attack;
    else if (parts[0] == "SKILL") type = CardType::Skill;
    else if (parts[0] == "POWER") type = CardType::Power;
    else throw GameError("unknown Mad Science card type " + parts[0]);
    MadScienceRider rider;
    if (parts[1] == "SAPPING") rider = MadScienceRider::Sapping;
    else if (parts[1] == "VIOLENCE") rider = MadScienceRider::Violence;
    else if (parts[1] == "CHOKING") rider = MadScienceRider::Choking;
    else if (parts[1] == "ENERGIZED") rider = MadScienceRider::Energized;
    else if (parts[1] == "WISDOM") rider = MadScienceRider::Wisdom;
    else if (parts[1] == "CHAOS") rider = MadScienceRider::Chaos;
    else if (parts[1] == "EXPERTISE") rider = MadScienceRider::Expertise;
    else if (parts[1] == "CURIOUS") rider = MadScienceRider::Curious;
    else if (parts[1] == "IMPROVEMENT") rider = MadScienceRider::Improvement;
    else throw GameError("unknown Mad Science rider " + parts[1]);
    return {type, rider};
}

void write_choice(JsonWriter& w, Sim* sim, const ChoiceRequest& r) {
    w.begin_object();
    w.field("event", "decision");
    w.field("kind", "card_select");
    w.key("options").begin_object();
    w.key("cards").begin_array();
    for (uint32_t c : r.cards) write_card_name(w, sim, c);
    w.end_array();
    w.field("min", r.min_select);
    w.field("max", r.max_select);
    w.field("can_skip", r.can_skip);
    w.field("source", r.source);
    w.end_object();
    w.key("run"); write_run(w, sim);
    w.end_object();
    emit(w);
}

std::vector<int32_t> parse_selection(const std::string& token) {
    if (token.size() < 2 || token[0] != 's') throw GameError("expected card selection action, got " + token);
    std::vector<int32_t> out;
    std::string body = token.substr(1);
    auto colon = body.find(':');
    int n = std::stoi(colon == std::string::npos ? body : body.substr(0, colon));
    out.push_back(n);
    if (colon != std::string::npos && !body.substr(colon + 1).empty())
        for (const std::string& s : split(body.substr(colon + 1), '.')) out.push_back(std::stoi(s));
    if (static_cast<int>(out.size()) != n + 1) throw GameError("card selection count mismatch: " + token);
    return out;
}

}  // namespace

int run_combat_replay(const std::string& seed, CharacterId character, int ascension, int col, int row,
                      const std::string& encounter, const std::string& actions, const std::string& deck,
                      const std::string& relics, bool takeSpecialRewards, const std::string& potion,
                      int32_t startingEnergy, int32_t startingStars, const std::string& potionTarget,
                      bool potionAutoDeath, const std::string& madScienceConfig, bool snapshots) {
    auto g = std::make_unique<GameState>();
    new_run(*g, NewRunOptions{.seed = seed, .character = character, .ascension = ascension});
    Sim sim(*g);
    std::vector<std::string> acts = split(actions, ',');
    size_t next = 0;
    try {
        customize_loadout(&sim, deck, relics, potion, madScienceConfig);
        RunManager::SetActInternal(&sim, 0);
        MapCoord coord{static_cast<int8_t>(col), static_cast<int8_t>(row)};
        if (encounter.empty()) {
            RunManager::EnterMapCoord(&sim, coord);
        } else {
            RunManager::EnterMapCoordDebug(&sim, coord, parse_encounter(encounter));
        }
        Player player(&sim, 1);
        PlayerCombatState pcs = player.PlayerCombatState();
        if (startingEnergy >= 0) pcs.SetEnergy(startingEnergy);
        if (startingStars >= 0) pcs.SetStars(startingStars);
        if (!potion.empty()) {
            PotionModel used;
            for (PotionModel candidate : player.Potions())
                if (candidate.PotionId_() == parse_potion(potion)) used = candidate;
            if (!used) throw GameError("test potion was not added to player slots: " + potion);
            Creature target;
            if (potionAutoDeath) {
                if (used.Usage() != PotionUsage::Automatic) throw GameError("test potion is not automatic: " + potion);
                player.Creature().SetCurrentHpInternal(1);
            } else if (potionTarget == "player") target = player.Creature();
            else if (potionTarget == "enemy") {
                if (used.TargetType() != TargetType::AnyEnemy) throw GameError("test potion does not target an enemy: " + potion);
                CreatureList enemies = CombatState(&sim).HittableEnemies();
                if (!enemies.empty()) target = enemies.front();
                if (!target) throw GameError("no enemy is a valid test potion target: " + potion);
            } else if (potionTarget == "auto") {
                switch (used.TargetType()) {
                case TargetType::Self:
                case TargetType::AnyPlayer: target = player.Creature(); break;
                case TargetType::AnyEnemy: {
                    CreatureList enemies = CombatState(&sim).HittableEnemies();
                    if (!enemies.empty()) target = enemies.front();
                    break;
                }
                default: break;
                }
            } else if (potionTarget != "none") {
                target = CombatState(&sim).GetCreature(std::stoi(potionTarget));
                if (!target) throw GameError("unknown test potion target: " + potionTarget);
            }
            if (!potionAutoDeath) {
                // Potion effects run before the first combat state and may
                // ask the player to select cards (for example Attack Potion,
                // Liquid Memories, or Gambler's Brew). Their answers are the
                // first `s...` actions in the Oracle trace, so replay those
                // choices here instead of leaving them for the first card
                // play/end-turn action below.
                const GameState potionSnapshot = *g;
                std::vector<int32_t> prefix;
                for (;;) {
                    sim.set_prefix(prefix);
                    try {
                        PlayerChoiceContext ctx;
                        used.OnUseWrapper(ctx, target);
                        if (!sim.prefix_consumed()) throw GameError("unused potion card selection answers");
                        break;
                    } catch (const NeedChoice& need) {
                        JsonWriter w;
                        write_choice(w, &sim, need.request);
                        if (next >= acts.size()) throw;
                        const std::vector<int32_t> answers = parse_selection(acts[next++]);
                        prefix.insert(prefix.end(), answers.begin(), answers.end());
                        *g = potionSnapshot;
                    }
                }
            }
        }
        CombatManager cm(&sim);
        for (;;) {
            if (!g->combat.exists || !cm.IsInProgress()) {
                JsonWriter w;
                w.begin_object();
                w.field("event", "end");
                w.field("player_hp", Player(&sim, 1).Creature().CurrentHp());
                w.key("rng");
                write_rng_counters(w, g->run);
                w.key("run"); write_run(w, &sim);
                w.key("extra_rewards").begin_array();
                for (RewardRef ref : g->run.combat_extra_rewards) write_reward(w, &sim, Reward(&sim, ref));
                w.end_array();
                if (takeSpecialRewards) {
                    for (RewardRef ref : g->run.combat_extra_rewards) {
                        Reward reward(&sim, ref);
                        if (reward.Type() == RewardType::SpecialCard) reward.SelectUnsynchronized();
                    }
                    w.key("run_after_special_rewards"); write_run(w, &sim);
                }
                w.end_object();
                emit(w);
                return 0;
            }
            if (!at_play_phase(&sim)) throw GameError("combat is neither over nor waiting for the player");
            {
                JsonWriter w;
                w.begin_object();
                w.field("event", "state");
                w.key("state");
                write_combat_state(w, &sim);
                if (snapshots) {
                    // The export is a pure function of the copied state. This
                    // replay drives RunManager directly, so the Env decision
                    // phase is set here exactly as Env::ResetCombat sets it.
                    Env logical;
                    logical.state() = *g;
                    logical.state().run.phase = RunPhase::Combat;
                    w.key("snapshot").raw_json(logical.ExportSnapshotJson());
                }
                w.end_object();
                emit(w);
            }
            if (next >= acts.size()) return 0;  // the oracle trace stops here too
            const std::string& a = acts[next++];
            GameState snapshot = *g;
            std::vector<int32_t> prefix;
            for (;;) {
                sim.set_prefix(prefix);
                try {
                    if (a == "e") {
                        ActionExecutor::EndTurn(&sim, Player(&sim, 1).PlayerCombatState().TurnNumber());
                    } else if (!a.empty() && a[0] == 'p') {
                        std::vector<std::string> parts = split(a.substr(1), ':');
                        int hand = std::stoi(parts.at(0));
                        int target = std::stoi(parts.at(1));
                        CardPile handPile = Player(&sim, 1).PlayerCombatState().Hand();
                        if (hand < 0 || hand >= handPile.Count()) throw GameError("hand index out of range: " + a);
                        CardModel card(&sim, handPile.refs()[static_cast<size_t>(hand)]);
                        Creature t = target < 0 ? Creature{} : CombatState(&sim).GetCreature(target);
                        if (!ActionExecutor::PlayCard(&sim, card, t)) throw GameError("the play was cancelled: " + a);
                    } else throw GameError("bad action " + a);
                    if (!sim.prefix_consumed()) throw GameError("unused card selection answers");
                    break;
                } catch (const NeedChoice& need) {
                    if (next >= acts.size()) throw;
                    JsonWriter w; write_choice(w, &sim, need.request);
                    std::vector<int32_t> answers = parse_selection(acts[next++]);
                    prefix.insert(prefix.end(), answers.begin(), answers.end());
                    *g = snapshot;
                }
            }
        }
    } catch (const std::exception& e) {
        JsonWriter w;
        w.begin_object();
        w.field("event", "error");
        w.field("action_index", static_cast<int32_t>(next) - 1);
        w.field("unported", dynamic_cast<const Unported*>(&e) != nullptr);
        w.field("message", std::string(e.what()));
        w.end_object();
        emit(w);
        return 1;
    }
}

}  // namespace sts2
