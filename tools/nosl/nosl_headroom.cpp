// nosl_headroom: for every combat of a recorded policy run, what an offline
// combat player gets from the same root.
//
//   nosl_headroom --trajectories FILE.jsonl --out OUT.jsonl [--threads N] [--limit N] [--first N]
//                 [--player teacher] [--teacher-beam 30] [--teacher-nodes 10000] [--teacher-ms 10000]
//                 [--player net --weights W.bin]
//                 [--player fair [--weights W.bin] [--max-turns N]]
//                 [--player rollout --weights W.bin [--rollout-samples 8] [--rollout-top 4] [--rollout-z 1]
//                                   [--rollout-win 3] [--rollout-lose 1] [--rollout-potion-hp 4]]
//                 [--player critic --weights W.bin --critic CRITIC.bin [--rollout-samples 4] [--rollout-top 4] [--rollout-z 1]]
//                 [--kinds boss,elite,normal,weak] [--acts 0,1] [--max-combats-per-run N]
//
// FILE holds {seed, ascension, actor_filter, action_rows} lines (python -m
// nosl.evaluate --trajectories, or an experiment's evaluation rows). Each run
// is replayed with the sampler's semantics. At the first decision of each
// combat the Env is cloned and the offline player finishes that combat on the
// clone; the replay then goes on with the recorded policy. One line per
// combat: the root (how many recorded rows precede it, act, floor, encounter, HP, deck, relics, potions), the
// recorded policy's result and the offline player's result.
//
// nosl_combat_diag asks a related question with one solver call per combat and
// reports the solver's projected end. This tool plays the combat out action
// by action (solving again when a route is partial or stops being legal), so
// its result is what happened in the Env, and it also records potions used,
// the deck at the root, and can compare a second public policy.
//
// The offline player is a diagnostic, never a NoSL result. `teacher` is the
// native CombatSolver route search on the true state: it sees the real draw
// order and the enemy's future rolls, so its result bounds what better play
// in this one hidden world would give; it is not what a public-information
// player can expect. `net` is another exported policy (python -m nosl.export)
// taking its most probable action behind actor filter 1, on the public
// observation only: it compares two public policies' combat play from
// identical roots. It starts each combat with a fresh actor filter.
//
// `fair` is the same solver without the foresight. Before it plans, what the
// player cannot see is resampled on a copy: the order of the draw and discard
// piles (Env::Determinize) and every random stream a combat draws from (the
// shuffle, card generation, targets, enemy moves, each monster's own). The
// plan is made on that copy and followed in the real Env only while the two
// show the same public observation; as soon as they differ (a new hand, a
// drawn card, a different enemy roll) it resamples and plans again. Its
// result is what this search gets from public information. Two limits: a
// card publicly known to be on top of the draw pile is resampled away with
// the rest; and a plan cannot be made while a choice inside an effect is
// pending (nothing can be resampled there, and the solver does not start from
// such a state), so that one answer is given by the public policy of
// --weights when there is one, else it is the first legal answer. Those
// answers are counted (`unfair` holds their number: they are not the
// solver's).
//
// `rollout` is one step of policy improvement over the public policy of
// --weights, also without foresight (the rollout algorithm of Tesauro and
// Galperin, 1996). At each decision the policy's `--rollout-top` most
// probable actions are each tried on `--rollout-samples` resampled copies
// (the same resampling as `fair`, the same copies for every action) and the
// policy itself, greedy, finishes the combat on each copy. A finished copy is
// worth `--rollout-win` plus the fraction of maximum HP left, less
// `--rollout-potion-hp` HP for each potion spent; a death is worth
// `--rollout-lose` times the fraction of the enemies' HP that was gone, so
// that a fight the policy loses still ranks its actions (the values are a
// diagnostic's, not a training signal). The policy's own action is kept unless another one's mean is
// higher by more than `--rollout-z` standard errors of the paired
// difference. It answers one question: from these roots and with this deck,
// how much better than the policy does a player get by looking ahead with
// the policy as its own model of the future. Choices inside an effect are
// answered by the policy, as for `fair` (`unfair` counts them); `solves`
// counts the decisions that were rolled out and `changed` those where the
// rollouts replaced the policy's action; `changes` lists them (what the
// player saw, the policy's action, the action taken, the mean gain).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "core/rng.h"
#include "core/xxhash64.h"
#include "engine/json.h"
#include "engine/sim/errors.h"
#include "game/All.h"
#include "nosl/actor_filter.h"
#include "nosl/observation.h"
#include "nosl/policies.h"
#include "nosl/policy_net.h"
#include "tools/nosl/rollout.h"

using namespace sts2;
namespace rollout = sts2::nosl::rollout;
using rollout::FightKind;
using rollout::Hp;
using rollout::Parse;
using rollout::Resample;
using rollout::Trajectory;
using Clock = std::chrono::steady_clock;

namespace {

struct Result {
    bool ran = false, survived = false;
    int32_t hp_after = 0, potions = 0, actions = 0, end_turns = 0, solves = 0, fallback = 0, unfair = 0, changed = 0;
    bool first_solve_won = false;
    double ms = 0;
    std::string error;
    std::vector<std::string> changes;  // --player rollout: one JSON object per decision the rollouts changed
};

// The offline player finishes the combat `root` is in, on a copy.
Result PlayOffline(const Env& root, const std::string& player, const nosl::TeacherOptions& options, int64_t max_steps) {
    Result r;
    r.ran = true;
    const auto begin = Clock::now();
    try {
        std::unique_ptr<Env> env = root.Clone();
        std::unique_ptr<nosl::Policy> policy = nosl::MakePolicy(player, options);
        policy->Reset(1);
        for (int64_t step = 0;; ++step) {
            if (env->decision().kind == DecisionKind::Ended || !env->state().combat.is_in_progress) break;
            if (step >= max_steps) { r.error = "step_limit"; break; }
            const std::vector<Action> legal = env->LegalActions();
            if (legal.empty()) { r.error = "no_legal_action"; break; }
            const Action action = policy->Choose(*env, legal);
            if (action.kind == Action::UsePotion) ++r.potions;
            if (action.kind == Action::EndTurn) ++r.end_turns;
            ++r.actions;
            env->Step(action);
        }
        r.hp_after = Hp(*env);
        r.survived = r.hp_after > 0 && r.error.empty();
        r.solves = static_cast<int32_t>(policy->stats.solves.size());
        r.fallback = policy->stats.teacher_fallback;
        r.first_solve_won = !policy->stats.solves.empty() && policy->stats.solves.front().won;
    } catch (const std::exception& e) {
        r.error = std::string(e.what()).substr(0, 160);
    }
    r.ms = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    return r;
}

// A public policy finishes the combat `root` is in, on a copy: greedy, sampler semantics.
Result PlayNet(const Env& root, const nosl::PolicyNet& net, int64_t max_steps) {
    Result r;
    r.ran = true;
    const auto begin = Clock::now();
    try {
        std::unique_ptr<Env> env = root.Clone();
        nosl::ActorFilter filter(nosl::kActorFilterV1);
        nosl::Observation obs;
        std::vector<float> logits;
        float value = 0;
        for (int64_t step = 0;; ++step) {
            if (env->decision().kind == DecisionKind::Ended || !env->state().combat.is_in_progress) break;
            if (step >= max_steps) { r.error = "step_limit"; break; }
            nosl::Observe(*env, obs);
            filter.Apply(obs);
            if (obs.legal.empty()) { r.error = "no_legal_action"; break; }
            size_t row = 0;
            if (obs.legal.size() > 1) {
                net.Forward(obs, logits, value);
                for (size_t i = 1; i < logits.size(); ++i) if (logits[i] > logits[row]) row = i;
            }
            const Action action = obs.legal[row];
            if (action.kind == Action::UsePotion) ++r.potions;
            if (action.kind == Action::EndTurn) ++r.end_turns;
            ++r.actions;
            filter.OnAction(action);
            env->Step(action);
        }
        r.hp_after = Hp(*env);
        r.survived = r.hp_after > 0 && r.error.empty();
    } catch (const std::exception& e) {
        r.error = std::string(e.what()).substr(0, 160);
    }
    r.ms = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    return r;
}

bool SamePublic(const Env& a, const Env& b, nosl::Observation& oa, nosl::Observation& ob) {
    nosl::Observe(a, oa);
    nosl::Observe(b, ob);
    return oa.global == ob.global && oa.tokens == ob.tokens && oa.actions == ob.actions;
}

// The solver on resampled copies finishes the combat `root` is in (see the file comment).
Result PlayFair(const Env& root, const nosl::TeacherOptions& options, const nosl::PolicyNet* net, int64_t max_steps,
                uint64_t seed) {
    Result r;
    r.ran = true;
    const auto begin = Clock::now();
    try {
        std::unique_ptr<Env> env = root.Clone();
        std::unique_ptr<Env> world;
        std::vector<Action> route;
        size_t cursor = 0;
        nosl::Observation oa, ob;
        nosl::ActorFilter filter(nosl::kActorFilterV1);
        std::vector<float> logits;
        float value = 0;
        for (int64_t step = 0;; ++step) {
            if (env->decision().kind == DecisionKind::Ended || !env->state().combat.is_in_progress) break;
            if (step >= max_steps) { r.error = "step_limit"; break; }
            const std::vector<Action> legal = env->LegalActions();
            if (legal.empty()) { r.error = "no_legal_action"; break; }
            bool plan = !world || cursor >= route.size() ||
                        std::find(legal.begin(), legal.end(), route[cursor]) == legal.end();
            if (!plan) {
                try { plan = !SamePublic(*env, *world, oa, ob); } catch (const std::exception&) { plan = true; }
            }
            if (plan && env->decision().kind == DecisionKind::Choice) {
                // No plan can be made here: one answer from the public policy, then plan again.
                ++r.unfair;
                Action answer = legal.front();
                if (net) {
                    nosl::Observe(*env, oa);
                    filter.Apply(oa);
                    size_t row = 0;
                    if (oa.legal.size() > 1) {
                        net->Forward(oa, logits, value);
                        for (size_t i = 1; i < logits.size(); ++i) if (logits[i] > logits[row]) row = i;
                    }
                    answer = oa.legal[row];
                }
                ++r.actions;
                env->Step(answer);
                world.reset();
                continue;
            }
            if (plan) {
                world = env->Clone();
                Resample(*world, seed + 0x100000001B3ull * static_cast<uint64_t>(++r.solves));
                route = combatsolver::CombatSolver::Solve(*world, options.config).Actions;
                cursor = 0;
            }
            Action action;
            if (cursor < route.size() && std::find(legal.begin(), legal.end(), route[cursor]) != legal.end()) {
                action = route[cursor++];
            } else {
                ++r.fallback;
                const auto end = std::find_if(legal.begin(), legal.end(), [](const Action& a) { return a.kind == Action::EndTurn; });
                action = end != legal.end() ? *end : legal.front();
                world.reset();
            }
            if (action.kind == Action::UsePotion) ++r.potions;
            if (action.kind == Action::EndTurn) ++r.end_turns;
            ++r.actions;
            env->Step(action);
            if (world) {
                try { world->Step(action); } catch (const std::exception&) { world.reset(); }
            }
        }
        r.hp_after = Hp(*env);
        r.survived = r.hp_after > 0 && r.error.empty();
    } catch (const std::exception& e) {
        r.error = std::string(e.what()).substr(0, 160);
    }
    r.ms = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    return r;
}

// "PlayCard STRIKE_SILENT+ Attack" and the like, for the log of changed decisions.
std::string Describe(Env& env, const Action& a) {
    static const char* kinds[] = {"StartRun", "MapMove", "PlayCard", "EndTurn", "TakeReward", "Proceed", "EventOption",
                                  "RestOption", "ShopBuy", "TreasurePick", "UsePotion", "DiscardPotion", "Answer", "Resume"};
    static const char* types[] = {"None", "Attack", "Skill", "Power", "Status", "Curse", "Quest"};
    std::string text = kinds[a.kind];
    Player player(&env.sim(), 1);
    if (a.kind == Action::PlayCard) {
        const auto cards = player.PlayerCombatState().Hand().Cards();
        if (a.index >= 0 && static_cast<size_t>(a.index) < cards.size()) {
            CardModel card = cards[static_cast<size_t>(a.index)];
            text += " " + std::string(name(card.CardId_()));
            for (int32_t i = 0; i < card.CurrentUpgradeLevel(); ++i) text += "+";
            const size_t type = static_cast<size_t>(card.Type());
            text += std::string(" ") + (type < std::size(types) ? types[type] : "?");
        }
    } else if (a.kind == Action::UsePotion) {
        if (PotionModel potion = player.GetPotionAtSlotIndex(a.index)) text += " " + std::string(name(potion.PotionId_()));
    }
    if (a.target >= 0) text += " @" + std::to_string(a.target);
    return text;
}

// The public policy improved by one step of lookahead with its critic finishes the combat `root` is in.
// Each of the policy's `ro.top` most probable actions is taken on `ro.samples` resampled copies; a copy is worth
// what the critic says at the next decision it stops at, or, when the action ended the combat, what the trainer
// would have paid (nosl.combat_ppo --reward combat: (3 + HP change since the combat began / max HP - 0.06 per
// potion) / 3 for a survived combat, 0 for a death). No rollout: it asks whether the critic alone tells actions
// apart. `solves` counts the decisions looked at and `changed` those where the policy's action was replaced.
Result PlayCritic(const Env& root, const nosl::PolicyNet& net, const nosl::PolicyNet& critic, const rollout::Options& ro,
                  int64_t max_steps, uint64_t seed) {
    Result r;
    r.ran = true;
    const auto begin = Clock::now();
    try {
        std::unique_ptr<Env> env = root.Clone();
        nosl::ActorFilter filter(nosl::kActorFilterV1);
        nosl::Observation obs, seen;
        std::vector<float> logits, unused;
        float value = 0;
        const int32_t max_hp = std::max(1, env->state().creatures[env->state().run.player.creature].max_hp);
        const int32_t hp_start = Hp(*env);
        auto worth = [&](Env& world, nosl::ActorFilter f, const Action& first) {
            f.OnAction(first);
            world.Step(first);
            for (int guard = 0; guard < 200; ++guard) {
                if (world.decision().kind == DecisionKind::Ended || !world.state().combat.is_in_progress) break;
                nosl::Observe(world, seen);
                f.Apply(seen);
                if (seen.legal.size() != 1) {
                    float v = 0;
                    critic.Forward(seen, unused, v);
                    return static_cast<double>(v);
                }
                f.OnAction(seen.legal[0]);   // a decision with one action is taken without asking, as the sampler does
                world.Step(seen.legal[0]);
            }
            const int32_t hp = Hp(world);
            if (hp <= 0) return 0.0;
            const int32_t potions = r.potions + (first.kind == Action::UsePotion);
            return (3.0 + static_cast<double>(hp - hp_start) / max_hp - 0.06 * potions) / 3.0;
        };
        for (int64_t step = 0;; ++step) {
            if (env->decision().kind == DecisionKind::Ended || !env->state().combat.is_in_progress) break;
            if (step >= max_steps) { r.error = "step_limit"; break; }
            nosl::Observe(*env, obs);
            filter.Apply(obs);
            if (obs.legal.empty()) { r.error = "no_legal_action"; break; }
            size_t row = 0;
            if (obs.legal.size() > 1) {
                net.Forward(obs, logits, value);
                std::vector<size_t> order(logits.size());
                for (size_t i = 0; i < order.size(); ++i) order[i] = i;
                std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return logits[a] > logits[b]; });
                row = order[0];
                if (env->decision().kind == DecisionKind::Choice) {
                    ++r.unfair;
                } else {
                    order.resize(std::min(order.size(), static_cast<size_t>(std::max(1, ro.top))));
                    ++r.solves;
                    rollout::Trial trial;
                    trial.rows = order;
                    trial.worth.resize(order.size());
                    for (int32_t n = 0; n < ro.samples; ++n) {
                        std::vector<double> sample;
                        try {
                            for (size_t candidate : order) {
                                std::unique_ptr<Env> world = env->Clone();
                                Resample(*world, seed + 0x100000001B3ull * static_cast<uint64_t>(step + 1) + static_cast<uint64_t>(n));
                                sample.push_back(worth(*world, filter, obs.legal[candidate]));
                            }
                        } catch (const std::exception&) { continue; }
                        for (size_t c = 0; c < order.size(); ++c) trial.worth[c].push_back(sample[c]);
                    }
                    row = trial.rows[trial.best(ro.z)];
                    if (row != order[0]) ++r.changed;
                }
            }
            const Action action = obs.legal[row];
            if (action.kind == Action::UsePotion) ++r.potions;
            if (action.kind == Action::EndTurn) ++r.end_turns;
            ++r.actions;
            filter.OnAction(action);
            env->Step(action);
        }
        r.hp_after = Hp(*env);
        r.survived = r.hp_after > 0 && r.error.empty();
    } catch (const std::exception& e) {
        r.error = std::string(e.what()).substr(0, 160);
    }
    r.ms = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    return r;
}

// The public policy improved by rollouts of itself finishes the combat `root` is in (see the file comment).
Result PlayRollout(const Env& root, const nosl::PolicyNet& net, const rollout::Options& ro, int64_t max_steps, uint64_t seed) {
    Result r;
    r.ran = true;
    const auto begin = Clock::now();
    try {
        std::unique_ptr<Env> env = root.Clone();
        nosl::ActorFilter filter(nosl::kActorFilterV1);
        nosl::Observation obs;
        std::vector<float> logits;
        float value = 0;
        const int32_t max_hp = env->state().creatures[env->state().run.player.creature].max_hp;
        for (int64_t step = 0;; ++step) {
            if (env->decision().kind == DecisionKind::Ended || !env->state().combat.is_in_progress) break;
            if (step >= max_steps) { r.error = "step_limit"; break; }
            nosl::Observe(*env, obs);
            filter.Apply(obs);
            if (obs.legal.empty()) { r.error = "no_legal_action"; break; }
            size_t row = 0;
            if (obs.legal.size() > 1) {
                net.Forward(obs, logits, value);
                std::vector<size_t> order(logits.size());
                for (size_t i = 0; i < order.size(); ++i) order[i] = i;
                std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return logits[a] > logits[b]; });
                row = order[0];
                if (env->decision().kind == DecisionKind::Choice) {
                    ++r.unfair;  // nothing can be resampled here: the policy's answer stands
                } else {
                    ++r.solves;
                    const rollout::Trial trial = rollout::Try(*env, filter, obs, logits, net, ro, max_hp,
                                                              seed + 0x100000001B3ull * static_cast<uint64_t>(step + 1));
                    double best = 0;
                    row = trial.rows[trial.best(ro.z, &best)];
                    if (row != order[0]) {
                        ++r.changed;
                        // What the player saw: HP, block, energy, and the damage the enemies show for this turn.
                        int32_t incoming = 0;
                        for (size_t i = 0; i < obs.token_count(); ++i) {
                            const int32_t* t = &obs.tokens[i * nosl::kTokenWidth];
                            if (t[nosl::kColType] == static_cast<int32_t>(nosl::TokenType::Enemy) && t[nosl::kColN7] && t[nosl::kColN3] > 0)
                                incoming += t[nosl::kColN3] * std::max(1, t[nosl::kColN4]);
                        }
                        JsonWriter w;
                        w.begin_object().field("turn", obs.global[nosl::kGTurn]).field("hp", obs.global[nosl::kGHp])
                            .field("block", obs.global[nosl::kGBlock]).field("energy", obs.global[nosl::kGEnergy])
                            .field("hand", obs.global[nosl::kGHandCount]).field("incoming", incoming)
                            .field("policy", Describe(*env, obs.legal[order[0]])).field("taken", Describe(*env, obs.legal[row]))
                            .field("gain", best).end_object();
                        r.changes.push_back(w.str());
                    }
                }
            }
            const Action action = obs.legal[row];
            if (action.kind == Action::UsePotion) ++r.potions;
            if (action.kind == Action::EndTurn) ++r.end_turns;
            ++r.actions;
            filter.OnAction(action);
            env->Step(action);
        }
        r.hp_after = Hp(*env);
        r.survived = r.hp_after > 0 && r.error.empty();
    } catch (const std::exception& e) {
        r.error = std::string(e.what()).substr(0, 160);
    }
    r.ms = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    return r;
}

struct Options {
    std::string player = "teacher";
    std::shared_ptr<const nosl::PolicyNet> net;       // --player net
    std::shared_ptr<const nosl::PolicyNet> fair_net;  // --player fair: answers choices inside effects
    std::shared_ptr<const nosl::PolicyNet> rollout_net;  // --player rollout
    std::shared_ptr<const nosl::PolicyNet> critic_policy, critic_net;  // --player critic
    rollout::Options rollout;
    nosl::TeacherOptions teacher;
    std::set<std::string> kinds;
    std::set<int32_t> acts;  // act indexes the offline player plays (empty: all)
    int32_t max_combats_per_run = 1 << 30;
};

void Replay(const Trajectory& t, const Options& options, std::vector<std::string>& lines) {
    Env env;
    env.Reset(NewRunOptions{.seed = t.seed, .character = CharacterId::SILENT, .ascension = t.ascension});
    nosl::ActorFilter filter(t.filter);
    nosl::Observation obs;
    struct Open {
        bool open = false;
        JsonWriter head;       // the root's fields, still an open object
        Result offline;
        int32_t hp_before = 0, potions = 0, end_turns = 0, actions = 0;
    } combat;
    int32_t combats_done = 0;
    int32_t consumed = 0;  // recorded action rows taken so far
    auto close = [&] {
        if (!combat.open) return;
        combat.open = false;
        const bool survived = Hp(env) > 0;  // a death also ends the combat
        JsonWriter& w = combat.head;
        w.key("recorded").begin_object().field("survived", survived).field("hp_after", survived ? Hp(env) : 0)
            .field("potions", combat.potions).field("end_turns", combat.end_turns).field("actions", combat.actions).end_object();
        const Result& o = combat.offline;
        if (o.ran) {
            w.key("offline").begin_object().field("survived", o.survived).field("hp_after", o.hp_after)
                .field("potions", o.potions).field("end_turns", o.end_turns).field("actions", o.actions)
                .field("solves", o.solves).field("fallback", o.fallback).field("unfair", o.unfair).field("changed", o.changed)
                .field("first_solve_won", o.first_solve_won)
                .field("ms", o.ms).field("error", o.error);
            if (!o.changes.empty()) {
                w.key("changes").begin_array();
                for (const std::string& change : o.changes) w.raw_json(change);
                w.end_array();
            }
            w.end_object();
        }
        w.end_object();
        lines.push_back(w.str());
    };
    auto track = [&] {
        GameState& g = env.state();
        if (g.combat.is_in_progress && !combat.open && env.decision().kind != DecisionKind::Ended) {
            combat = Open{};
            combat.open = true;
            combat.hp_before = Hp(env);
            const std::string encounter(name(g.combat.encounter));
            const std::string kind = FightKind(encounter);
            Sim* sim = &env.sim();
            Player player(sim, 1);
            JsonWriter& w = combat.head;
            w.begin_object().field("seed", t.seed).field("row_index", consumed).field("act", g.run.current_act_index).field("floor", g.run.total_floor)
                .field("encounter", encounter).field("fight", kind).field("hp_before", combat.hp_before)
                .field("max_hp", player.Creature().MaxHp()).field("gold", player.Gold());
            w.key("deck").begin_array();
            for (CardModel card : player.Deck().Cards()) {
                std::string label(name(card.CardId_()));
                for (int32_t i = 0; i < card.CurrentUpgradeLevel(); ++i) label += "+";
                w.value(label);
            }
            w.end_array().key("relics").begin_array();
            for (RelicModel relic : player.Relics()) w.value(std::string(name(relic.RelicId_())));
            w.end_array().key("potions").begin_array();
            for (int32_t slot = 0; slot < player.MaxPotionCount(); ++slot)
                if (PotionModel potion = player.GetPotionAtSlotIndex(slot)) w.value(std::string(name(potion.PotionId_())));
            w.end_array();
            if ((options.kinds.empty() || options.kinds.count(kind)) && (options.acts.empty() || options.acts.count(g.run.current_act_index)) &&
                combats_done < options.max_combats_per_run) {
                const uint64_t combat_seed = xxhash64(t.seed.data(), t.seed.size(), static_cast<uint64_t>(combats_done));
                combat.offline = options.net ? PlayNet(env, *options.net, 4000)
                                 : options.critic_net ? PlayCritic(env, *options.critic_policy, *options.critic_net, options.rollout, 4000, combat_seed)
                                 : options.rollout_net ? PlayRollout(env, *options.rollout_net, options.rollout, 4000, combat_seed)
                                 : options.player == "fair"
                                     ? PlayFair(env, options.teacher, options.fair_net.get(), 4000, combat_seed)
                                     : PlayOffline(env, options.player, options.teacher, 4000);
                ++combats_done;
            }
        } else if (!g.combat.is_in_progress && combat.open) {
            close();
        }
    };
    auto count = [&](const Action& action) {
        if (!combat.open) return;
        ++combat.actions;
        if (action.kind == Action::UsePotion) ++combat.potions;
        if (action.kind == Action::EndTurn) ++combat.end_turns;
    };
    auto forced = [&] {
        for (;;) {
            track();
            if (env.decision().kind == DecisionKind::Ended) return;
            nosl::Observe(env, obs);
            filter.Apply(obs);
            if (obs.legal.size() != 1) return;
            count(obs.legal[0]);
            filter.OnAction(obs.legal[0]);
            env.Step(obs.legal[0]);
        }
    };
    forced();
    for (int32_t row : t.rows) {
        if (env.decision().kind == DecisionKind::Ended) break;
        if (row < 0 || static_cast<size_t>(row) >= obs.legal.size()) throw GameError("action row out of range");
        const Action action = obs.legal[static_cast<size_t>(row)];
        count(action);
        filter.OnAction(action);
        env.Step(action);
        ++consumed;
        forced();
    }
    close();  // a run that ended inside a combat
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::map<std::string, std::string> opt;
        for (int i = 1; i + 1 < argc; i += 2) opt[std::string(argv[i]).substr(2)] = argv[i + 1];
        if (!opt.count("trajectories") || !opt.count("out")) throw std::invalid_argument("--trajectories and --out are required");
        auto number = [&](const char* key, int fallback) { return opt.count(key) ? std::stoi(opt[key]) : fallback; };
        Options options;
        if (opt.count("player")) options.player = opt["player"];
        if (options.player == "net") {
            if (!opt.count("weights")) throw std::invalid_argument("--player net needs --weights");
            options.net = nosl::PolicyNet::Load(opt["weights"]);
        }
        if (options.player == "fair" && opt.count("weights")) options.fair_net = nosl::PolicyNet::Load(opt["weights"]);
        if (options.player == "critic") {
            if (!opt.count("weights") || !opt.count("critic")) throw std::invalid_argument("--player critic needs --weights and --critic");
            options.critic_policy = nosl::PolicyNet::Load(opt["weights"]);
            options.critic_net = nosl::PolicyNet::Load(opt["critic"]);
            options.rollout.samples = number("rollout-samples", 4);
            options.rollout.top = number("rollout-top", 4);
            if (opt.count("rollout-z")) options.rollout.z = std::stod(opt["rollout-z"]);
        }
        if (options.player == "rollout") {
            if (!opt.count("weights")) throw std::invalid_argument("--player rollout needs --weights");
            options.rollout_net = nosl::PolicyNet::Load(opt["weights"]);
            options.rollout.samples = number("rollout-samples", 8);
            options.rollout.top = number("rollout-top", 4);
            if (opt.count("rollout-z")) options.rollout.z = std::stod(opt["rollout-z"]);
            if (opt.count("rollout-win")) options.rollout.win = std::stod(opt["rollout-win"]);
            if (opt.count("rollout-lose")) options.rollout.lose = std::stod(opt["rollout-lose"]);
            if (opt.count("rollout-potion-hp")) options.rollout.potion_hp = std::stod(opt["rollout-potion-hp"]);
        }
        if (opt.count("max-turns")) options.teacher.config.MaxTurns = number("max-turns", 64);
        options.teacher.config.Profile.BeamWidth = number("teacher-beam", 30);
        options.teacher.config.Profile.MaxExpandedNodes = number("teacher-nodes", 10000);
        options.teacher.config.Profile.SoftTimeBudgetMilliseconds = number("teacher-ms", 10000);
        options.teacher.config.TransitionCacheBytes = static_cast<size_t>(number("teacher-cache-mib", 128)) * 1024 * 1024;
        options.max_combats_per_run = number("max-combats-per-run", 1 << 30);
        if (opt.count("kinds")) {
            std::istringstream kinds(opt["kinds"]);
            for (std::string kind; std::getline(kinds, kind, ',');) options.kinds.insert(kind);
        }
        if (opt.count("acts")) {
            std::istringstream acts(opt["acts"]);
            for (std::string act; std::getline(acts, act, ',');) options.acts.insert(std::stoi(act));
        }
        std::vector<Trajectory> runs;
        std::ifstream in(opt["trajectories"]);
        for (std::string line; std::getline(in, line);) if (!line.empty()) runs.push_back(Parse(line));
        const size_t first = static_cast<size_t>(number("first", 0));
        if (first) runs.erase(runs.begin(), runs.begin() + static_cast<std::ptrdiff_t>(std::min(first, runs.size())));
        if (opt.count("limit") && runs.size() > static_cast<size_t>(number("limit", 0))) runs.resize(static_cast<size_t>(number("limit", 0)));
        std::ofstream out(opt["out"]);
        if (!out) throw std::runtime_error("cannot write " + opt["out"]);
        { Env warm; warm.Reset(NewRunOptions{.seed = "NTWARMUP", .character = CharacterId::SILENT, .ascension = 10}); }
        std::atomic<size_t> next{0};
        std::atomic<int> failed{0};
        std::mutex mutex;
        auto worker = [&] {
            for (;;) {
                const size_t i = next.fetch_add(1);
                if (i >= runs.size()) return;
                std::vector<std::string> lines;
                try { Replay(runs[i], options, lines); }
                catch (const std::exception& e) { ++failed; std::lock_guard<std::mutex> lock(mutex); std::cerr << runs[i].seed << ": " << e.what() << '\n'; }
                std::lock_guard<std::mutex> lock(mutex);
                for (const std::string& line : lines) out << line << '\n';
                out.flush();
            }
        };
        std::vector<std::thread> pool;
        for (int t = 0; t < std::max(1, number("threads", 1)); ++t) pool.emplace_back(worker);
        for (auto& t : pool) t.join();
        std::cout << "{\"runs\":" << runs.size() << ",\"failed\":" << failed.load() << "}" << std::endl;
        return failed ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "nosl_headroom: " << e.what() << '\n';
        return 2;
    }
}
