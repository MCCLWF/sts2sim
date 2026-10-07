// `smoke`: random agents play runs through the environment (M1-j).
//
//   smoke --seeds S1,S2,... [--character C] [--ascension N] [--policy-seed P]
//         [--max-steps N] [--no-neow] [--ported-only] [--greedy] [--hp N]
//
// --hp N: a development aid that starts the player with N max HP, so that a
// weak policy still travels the whole act (tests the run flow, not balance).
// --greedy: a less hopeless policy (the default is the oracle's, which dies
// around floor 3-5 in the game too): combat by one step of lookahead on a copy
// of the state; it rests when below 60% HP.
//
// --ported-only: where the policy has a choice, it keeps to ported content
// (IsUsable): it takes only usable cards, relics and potions, prefers map
// points that are not unknown (an unknown room may hold an unported event),
// and at Neow takes an option whose relic is usable. Content it cannot avoid
// (an event in an unknown room, an elite's relic) can still stop the run.
//
// Each run goes until the next map point would be the act-1 boss, the player
// dies, or something fails. One JSON line per run:
//   {"seed","end":"boss"|"death"|"unported"|"error"|"max_steps","message","steps","floor","hp","gold","deck"}
// then a summary line. Exit code 1 if any run ended in "error".
#include "tools/cli/smoke_cmd.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "core/rng.h"
#include "engine/json.h"
#include "engine/sim/errors.h"
#include "env/env.h"
#include "game/All.h"
#include "game/Runs/RunFlow.h"

namespace sts2 {

namespace {

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

// The combat actions the oracle's policy chooses from (OracleCombat.RunOne).
std::vector<Action> CombatActions(Sim& sim) {
    std::vector<Action> plays;
    CombatState combat(&sim);
    CardList hand = Player(&sim, 1).PlayerCombatState().Hand().Cards();
    for (size_t i = 0; i < hand.size(); ++i) {
        CardModel card = hand[i];
        if (!card.CanPlay()) continue;
        TargetType t = card.TargetType();
        if (t == TargetType::AnyEnemy || t == TargetType::AnyAlly) {
            for (Creature c : combat.Creatures()) {
                if (card.IsValidTarget(c)) {
                    plays.push_back(Action{.kind = Action::PlayCard, .index = static_cast<int32_t>(i), .target = c.CombatId()});
                }
            }
        } else if (card.IsValidTarget(Creature{})) {
            plays.push_back(Action{.kind = Action::PlayCard, .index = static_cast<int32_t>(i), .target = -1});
        }
    }
    return plays;
}

struct Outcome {
    std::string end;
    std::string message;
    int steps = 0;
};

bool UsableCard(Sim& sim, uint32_t card) { return IsUsable(ModelKind::Card, static_cast<uint16_t>(CardModel(&sim, card).CardId_())); }
bool UsableRelic(RelicModel r) { return r && IsUsable(ModelKind::Relic, static_cast<uint16_t>(r.RelicId_())); }

// Whether the ported-only policy may take this reward.
bool UsableReward(Reward r) {
    switch (r.Type()) {
    case RewardType::Potion:
        // This smoke policy has no potion-use action. Keep potion rewards out
        // of its ported-only choices so adding a usable potion cannot change
        // the later combat route without exercising the potion API.
        return false;
    case RewardType::Relic: return UsableRelic(r.Relic());
    default: return true;  // gold, a card reward (its choice skips unported cards), a card removal
    }
}

bool UsableShopEntry(Sim& sim, Merchant::EntryRef e) {
    const MerchantState& s = sim.g->run.merchant;
    switch (e.kind) {
    case Merchant::EntryRef::CharacterCard: return UsableCard(sim, s.character_cards[e.index].card);
    case Merchant::EntryRef::ColorlessCard: return UsableCard(sim, s.colorless_cards[e.index].card);
    case Merchant::EntryRef::Relic: return UsableRelic(RelicModel(&sim, s.relics[e.index].relic));
    case Merchant::EntryRef::Potion:
        // The policy does not spend a turn using potions, so it also skips
        // shop potions in ported-only mode.
        return false;
    case Merchant::EntryRef::CardRemoval: return true;
    }
    return true;
}

// An answer to a choice inside an effect (the policy's).
int32_t ChoiceAnswer(Sim& sim, const ChoiceRequest& r, Rng& policy, bool portedOnly) {
    int32_t pick = -1;
    if (r.source == "rewards") {
        // A nested rewards screen: a reward not taken yet, or proceed (= the set size).
        std::vector<int32_t> available;
        int32_t n = r.option_count - 1;
        for (int32_t i = 0; i < n; ++i) {
            Reward reward = RunFlow::RewardOptions(&sim)[static_cast<size_t>(i)];
            if (reward.SuccessfullySelected()) continue;
            if (portedOnly && !UsableReward(reward)) continue;
            available.push_back(i);
        }
        int32_t k = policy.NextInt(static_cast<int32_t>(available.size()) + 1);
        pick = k == static_cast<int32_t>(available.size()) ? n : available[static_cast<size_t>(k)];
    } else if (r.subset && r.prompt == "count") {
        int32_t most = std::min(r.max_select, r.option_count);
        int32_t least = std::min(r.min_select, most);
        pick = least + policy.NextInt(most - least + 1);
    } else if (r.subset) {
        std::vector<int32_t> free;
        for (int32_t i = 0; i < r.option_count; ++i) {
            if (std::find(r.selected.begin(), r.selected.end(), i) == r.selected.end()) free.push_back(i);
        }
        pick = free[static_cast<size_t>(policy.NextInt(static_cast<int32_t>(free.size())))];
    } else if (portedOnly && r.kind == ChoiceKind::CardSelect && !r.cards.empty()) {
        // A card reward: a usable card, else the first alternative (Skip).
        std::vector<int32_t> usable;
        for (size_t i = 0; i < r.cards.size(); ++i) {
            if (UsableCard(sim, r.cards[i])) usable.push_back(static_cast<int32_t>(i));
        }
        int32_t k = policy.NextInt(static_cast<int32_t>(usable.size()) + 1);
        pick = k < static_cast<int32_t>(usable.size()) ? usable[static_cast<size_t>(k)] : static_cast<int32_t>(r.cards.size());
    } else {
        pick = r.option_count > 0 ? policy.NextInt(r.option_count) : -1;
    }
    return pick;
}

// --greedy's combat: one step of lookahead. Each candidate (a play, or ending
// the turn) runs on a copy of the state, followed by the end of the turn;
// the score favours winning the combat, then the player's HP against the
// enemies' HP. Choices on the way are answered by the policy.
double Score(Env& tmp) {
    Sim& sim = tmp.sim();
    Creature player = Player(&sim, 1).Creature();
    if (player.IsDead()) return -1e9;
    if (sim.g->run.phase != RunPhase::Combat) return 1e6 + player.CurrentHp();
    double enemies = 0;
    for (Creature c : CombatState(&sim).Enemies()) {
        if (c.IsAlive()) enemies += c.CurrentHp() + c.Block() + 10.0;  // an enemy left alive keeps hitting
    }
    return player.CurrentHp() - enemies;
}

void Settle(Env& tmp, Rng& policy, bool portedOnly) {
    for (int guard = 0; guard < 64 && tmp.decision().kind == DecisionKind::Choice; ++guard) {
        Decision d = tmp.decision();
        tmp.Step(Action{.kind = Action::Answer, .index = ChoiceAnswer(tmp.sim(), d.request, policy, portedOnly)});
    }
}

Action LookaheadCombatAction(Env& env, Rng& policy, bool portedOnly) {
    std::vector<Action> candidates = CombatActions(env.sim());
    candidates.push_back(Action{.kind = Action::EndTurn});
    double best = -1e18;
    std::vector<Action> bests;
    for (const Action& a : candidates) {
        double score;
        try {
            Env tmp;
            tmp.state() = env.state();
            tmp.Step(a);
            Settle(tmp, policy, portedOnly);
            if (a.kind != Action::EndTurn && tmp.decision().kind == DecisionKind::Combat) {
                tmp.Step(Action{.kind = Action::EndTurn});
                Settle(tmp, policy, portedOnly);
            }
            score = Score(tmp);
        } catch (const Unported&) {
            score = -1e12;  // keep away from unported content
        }
        if (score > best + 1e-9) {
            best = score;
            bests.clear();
        }
        if (score >= best - 1e-9) bests.push_back(a);
    }
    return bests[static_cast<size_t>(policy.NextInt(static_cast<int32_t>(bests.size())))];
}

void PlayOne(Env& env, Rng& policy, int maxSteps, bool portedOnly, bool greedy, Outcome& o) {
    for (o.steps = 0; o.steps < maxSteps; ++o.steps) {
        Decision d = env.decision();
        Sim& sim = env.sim();
        switch (d.kind) {
        case DecisionKind::Ended:
            o.end = Player(&sim, 1).Creature().IsDead() ? "death" : "ended";
            return;
        case DecisionKind::Map: {
            if (std::getenv("SMOKE_TRACE") != nullptr) {
                Creature c = Player(&sim, 1).Creature();
                std::fprintf(stderr, "floor %d room %d hp %d/%d gold %d deck %zu\n", sim.g->run.total_floor,
                             sim.g->run.rooms.empty() ? -1 : static_cast<int>(sim.g->run.rooms.back()), c.CurrentHp(), c.MaxHp(),
                             sim.g->run.player.gold, sim.g->run.player.deck.size());
            }
            StaticVec<MapCoord, 8> options = RunFlow::MapOptions(&sim);
            if (portedOnly) {
                StaticVec<MapCoord, 8> known;
                const ActMap& map = sim.g->run.map;
                for (MapCoord c : options) {
                    int p = map.get_point(c.col, c.row);
                    if (p < 0 || map.points[static_cast<size_t>(p)].type != MapPointType::Unknown) known.push_back(c);
                }
                if (!known.empty()) options = known;
            }
            MapCoord next = options[static_cast<size_t>(policy.NextInt(static_cast<int32_t>(options.size())))];
            if (RunFlow::IsBossPoint(&sim, next)) {
                o.end = "boss";
                return;
            }
            env.Step(Action{.kind = Action::MapMove, .coord = next});
            break;
        }
        case DecisionKind::Combat: {
            if (std::getenv("SMOKE_TRACE") != nullptr && std::string(std::getenv("SMOKE_TRACE")) == "2") {
                Creature c = Player(&sim, 1).Creature();
                std::fprintf(stderr, "  %s turn %d hp %d block %d energy %d |", std::string(data(sim.g->combat.encounter).entry).c_str(),
                             Player(&sim, 1).PlayerCombatState().TurnNumber(), c.CurrentHp(), c.Block(),
                             Player(&sim, 1).PlayerCombatState().Energy());
                for (Creature e : CombatState(&sim).Enemies()) std::fprintf(stderr, " %d/%d", e.CurrentHp(), e.Block());
                std::fprintf(stderr, "\n");
            }
            if (greedy) {
                env.Step(LookaheadCombatAction(env, policy, portedOnly));
                break;
            }
            std::vector<Action> plays = CombatActions(sim);
            if (!plays.empty() && policy.NextDouble() < 0.85) {
                env.Step(plays[static_cast<size_t>(policy.NextInt(static_cast<int32_t>(plays.size())))]);
            } else {
                env.Step(Action{.kind = Action::EndTurn});
            }
            break;
        }
        case DecisionKind::Rewards: {
            // The oracle's policy: a reward not taken yet, or proceed.
            std::vector<int32_t> available;
            int32_t n = static_cast<int32_t>(RunFlow::RewardOptions(&sim).size());
            for (int32_t i = 0; i < n; ++i) {
                if (!RunFlow::RewardTakable(&sim, i)) continue;
                if (portedOnly && !UsableReward(RunFlow::RewardOptions(&sim)[static_cast<size_t>(i)])) continue;
                available.push_back(i);
            }
            int32_t pick = policy.NextInt(static_cast<int32_t>(available.size()) + 1);
            env.Step(pick == static_cast<int32_t>(available.size())
                         ? Action{.kind = Action::Proceed}
                         : Action{.kind = Action::TakeReward, .index = available[static_cast<size_t>(pick)]});
            break;
        }
        case DecisionKind::Event: {
            // The oracle's policy: unlocked options that do not kill the player.
            const EventOptionList& options = RunFlow::EventOptions(&sim);
            std::vector<int32_t> legal, safe;
            for (size_t i = 0; i < options.size(); ++i) {
                if (options[i].is_locked) continue;
                legal.push_back(static_cast<int32_t>(i));
                if (!WillKillPlayer(options[i], Player(&sim, 1))) safe.push_back(static_cast<int32_t>(i));
            }
            if (!safe.empty()) legal = safe;
            if (portedOnly) {
                // An option that gives a relic (Neow): only a usable one.
                std::vector<int32_t> usable;
                for (int32_t i : legal) {
                    RelicRef r = options[static_cast<size_t>(i)].relic;
                    if (r == 0 || UsableRelic(RelicModel(&sim, r))) usable.push_back(i);
                }
                if (!usable.empty()) legal = usable;
            }
            if (legal.empty()) throw GameError("an event with no option to choose");
            env.Step(Action{.kind = Action::EventOption, .index = legal[static_cast<size_t>(policy.NextInt(static_cast<int32_t>(legal.size())))]});
            break;
        }
        case DecisionKind::RestSite: {
            std::vector<Action> legal;
            StaticVec<RunFlow::RestSiteOptionView, 8> options = RunFlow::RestSiteOptions(&sim);
            for (size_t i = 0; i < options.size(); ++i) {
                if (options[i].enabled) legal.push_back(Action{.kind = Action::RestOption, .index = static_cast<int32_t>(i)});
            }
            if (RunFlow::CanProceedFromRestSite(&sim)) legal.push_back(Action{.kind = Action::Proceed});
            if (legal.empty()) throw GameError("a rest site with nothing to do");
            if (greedy) {
                Creature c = Player(&sim, 1).Creature();
                bool low = c.CurrentHp() * 10 < c.MaxHp() * 6;
                RestSiteOptionId want = low ? RestSiteOptionId::Heal : RestSiteOptionId::Smith;
                for (size_t i = 0; i < options.size(); ++i) {
                    if (options[i].id == want && options[i].enabled) {
                        legal = {Action{.kind = Action::RestOption, .index = static_cast<int32_t>(i)}};
                        break;
                    }
                }
            }
            env.Step(legal[static_cast<size_t>(policy.NextInt(static_cast<int32_t>(legal.size())))]);
            break;
        }
        case DecisionKind::Shop: {
            // The oracle's policy: an affordable stocked entry, or leave.
            std::vector<int32_t> legal;
            int32_t n = static_cast<int32_t>(RunFlow::ShopOptions(&sim).size());
            StaticVec<Merchant::EntryRef, 16> entries = RunFlow::ShopOptions(&sim);
            for (int32_t i = 0; i < n; ++i) {
                if (!RunFlow::ShopOptionBuyable(&sim, i)) continue;
                if (portedOnly && !UsableShopEntry(sim, entries[static_cast<size_t>(i)])) continue;
                legal.push_back(i);
            }
            int32_t pick = policy.NextInt(static_cast<int32_t>(legal.size()) + 1);
            env.Step(pick == static_cast<int32_t>(legal.size()) ? Action{.kind = Action::Proceed}
                                                                 : Action{.kind = Action::ShopBuy, .index = legal[static_cast<size_t>(pick)]});
            break;
        }
        case DecisionKind::Treasure: {
            const StaticVec<RelicId, 4>& relics = RunFlow::TreasureRelics(&sim);
            int32_t n = static_cast<int32_t>(relics.size());
            int32_t pick = policy.NextInt(n + 1) - 1;
            if (portedOnly && pick >= 0 && !IsUsable(ModelKind::Relic, static_cast<uint16_t>(relics[static_cast<size_t>(pick)]))) pick = -1;
            env.Step(pick < 0 ? Action{.kind = Action::Proceed} : Action{.kind = Action::TreasurePick, .index = pick});
            break;
        }
        case DecisionKind::Choice: {
            int32_t pick = ChoiceAnswer(sim, d.request, policy, portedOnly);
            env.Step(Action{.kind = Action::Answer, .index = pick});
            break;
        }
        }
    }
    o.end = "max_steps";
}

}  // namespace

int run_smoke(std::map<std::string, std::string>& opts, CharacterId (*parse_character)(const std::string&)) {
    std::vector<std::string> seeds = split(opts.at("seeds"));
    CharacterId character = parse_character(opts.count("character") ? opts["character"] : "IRONCLAD");
    int ascension = opts.count("ascension") ? std::stoi(opts["ascension"]) : 0;
    uint32_t policySeed = opts.count("policy-seed") ? static_cast<uint32_t>(std::stoul(opts["policy-seed"])) : 1u;
    int maxSteps = opts.count("max-steps") ? std::stoi(opts["max-steps"]) : 5000;
    bool noNeow = opts.count("no-neow") && opts["no-neow"] != "0";
    bool portedOnly = opts.count("ported-only") && opts["ported-only"] != "0";
    bool greedy = opts.count("greedy") && opts["greedy"] != "0";
    int startHp = opts.count("hp") ? std::stoi(opts["hp"]) : 0;
    std::map<std::string, int> ends;
    int errors = 0;
    for (const std::string& seed : seeds) {
        Env env;
        Rng policy(policySeed + static_cast<uint32_t>(std::hash<std::string>{}(seed) & 0xffff));
        Outcome o;
        try {
            if (noNeow) {
                new_run(env.state(), NewRunOptions{.seed = seed, .character = character, .ascension = ascension});
                env.state().run.started_with_neow = false;  // ExtraFields.StartedWithNeow (development aid)
                env.Step(Action{.kind = Action::StartRun});
            } else {
                env.Reset(NewRunOptions{.seed = seed, .character = character, .ascension = ascension});
            }
            if (startHp > 0) {
                CreatureState& c = env.state().creatures[env.state().run.player.creature];
                c.max_hp = c.current_hp = startHp;
            }
            PlayOne(env, policy, maxSteps, portedOnly, greedy, o);
        } catch (const Unported& e) {
            o.end = "unported";
            o.message = e.what();
        } catch (const std::exception& e) {
            o.end = "error";
            o.message = e.what();
            ++errors;
        }
        ++ends[o.end];
        const GameState& g = env.state();
        JsonWriter w;
        w.begin_object();
        w.field("seed", seed);
        w.field("end", o.end);
        if (!o.message.empty()) w.field("message", o.message);
        w.field("steps", o.steps);
        w.field("floor", static_cast<int32_t>(g.run.total_floor));
        w.field("hp", g.creatures[g.run.player.creature].current_hp);
        w.field("gold", g.run.player.gold);
        w.field("deck", static_cast<int32_t>(g.run.player.deck.size()));
        w.end_object();
        std::printf("%s\n", w.str().c_str());
    }
    std::printf("smoke:");
    for (const auto& [k, v] : ends) std::printf(" %s=%d", k.c_str(), v);
    std::printf("\n");
    return errors > 0 ? 1 : 0;
}

}  // namespace sts2
