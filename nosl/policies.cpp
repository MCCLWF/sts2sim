// See policies.h.
#include "nosl/policies.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>

#include "engine/sim/errors.h"
#include "game/All.h"
#include "game/Events/EventOption.h"
#include "game/Runs/RunFlow.h"

namespace sts2::nosl {

const char* PolicyClassName(PolicyClass c) {
    switch (c) {
    case PolicyClass::PublicNoSearch: return "public_no_search";
    case PolicyClass::SimulatedLookahead: return "simulated_lookahead";
    case PolicyClass::PrivilegedTeacher: return "privileged_teacher";
    }
    return "unknown";
}

namespace {

using Rand = std::mt19937_64;

const Action& Pick(Rand& rng, const std::vector<Action>& from) {
    return from[std::uniform_int_distribution<size_t>(0, from.size() - 1)(rng)];
}

std::vector<Action> OfKind(const std::vector<Action>& legal, Action::Kind kind) {
    std::vector<Action> out;
    for (const Action& a : legal) if (a.kind == kind) out.push_back(a);
    return out;
}

bool InCombat(Env& env) { return env.state().combat.is_in_progress; }

class RandomPolicy : public Policy {
public:
    PolicyClass Class() const override { return PolicyClass::PublicNoSearch; }
    void Reset(uint64_t seed) override { rng_.seed(seed); }
    Action Choose(Env&, const std::vector<Action>& legal) override { return Pick(rng_, legal); }

private:
    Rand rng_;
};

// Out-of-combat rule v0. It reads what the screen shows: the legal options,
// HP, and the event's own death warning. It never uses or discards potions
// outside combat and never simulates.
class Rule0Strategic {
public:
    void Reset() { last_top_ = DecisionKind::Ended; reward_tried_ = 0; }

    Action Choose(Env& env, const std::vector<Action>& legal, Rand& rng) {
        const DecisionKind kind = env.decision().kind;
        const DecisionKind before = last_top_;
        if (kind != DecisionKind::Choice) last_top_ = kind;
        Sim* sim = &env.sim();
        switch (kind) {
        case DecisionKind::Map: return Pick(rng, OfKind(legal, Action::MapMove));
        case DecisionKind::Rewards: {
            // A skipped card reward stays takable, so each reward is tried once.
            if (before != DecisionKind::Rewards) reward_tried_ = 0;
            for (const Action& a : legal) {
                if (a.kind != Action::TakeReward || a.index < 0 || a.index >= 64) continue;
                if (reward_tried_ >> a.index & 1) continue;
                reward_tried_ |= uint64_t{1} << a.index;
                return a;
            }
            return Action{.kind = Action::Proceed};
        }
        case DecisionKind::Event: {
            const EventOptionList& options = RunFlow::EventOptions(sim);
            std::vector<Action> all = OfKind(legal, Action::EventOption), safe;
            for (const Action& a : all)
                if (!WillKillPlayer(options[static_cast<size_t>(a.index)], Player(sim, 1))) safe.push_back(a);
            return Pick(rng, safe.empty() ? all : safe);
        }
        case DecisionKind::RestSite: {
            Creature c = Player(sim, 1).Creature();
            const bool low = c.CurrentHp() * 10 < c.MaxHp() * 6;
            auto options = RunFlow::RestSiteOptions(sim);
            for (RestSiteOptionId want : {low ? RestSiteOptionId::Heal : RestSiteOptionId::Smith,
                                          low ? RestSiteOptionId::Smith : RestSiteOptionId::Heal})
                for (const Action& a : legal)
                    if (a.kind == Action::RestOption && options[static_cast<size_t>(a.index)].id == want) return a;
            std::vector<Action> rest = OfKind(legal, Action::RestOption);
            if (!rest.empty()) return Pick(rng, rest);
            return Action{.kind = Action::Proceed};
        }
        case DecisionKind::Shop: {
            std::vector<Action> buys = OfKind(legal, Action::ShopBuy);
            const size_t k = std::uniform_int_distribution<size_t>(0, buys.size())(rng);
            return k == buys.size() ? Action{.kind = Action::Proceed} : buys[k];
        }
        case DecisionKind::Treasure: {
            std::vector<Action> picks = OfKind(legal, Action::TreasurePick);
            return picks.empty() ? Action{.kind = Action::Proceed} : picks.front();
        }
        default: return Pick(rng, legal);  // Choice, Boundary
        }
    }

private:
    DecisionKind last_top_ = DecisionKind::Ended;
    uint64_t reward_tried_ = 0;
};

std::vector<Action> CombatPlays(const std::vector<Action>& legal) {
    std::vector<Action> out;
    for (const Action& a : legal)
        if (a.kind == Action::PlayCard || a.kind == Action::UsePotion) out.push_back(a);
    return out;
}

class Rule0Policy : public Policy {
public:
    PolicyClass Class() const override { return PolicyClass::PublicNoSearch; }
    void Reset(uint64_t seed) override { rng_.seed(seed); strategic_.Reset(); }
    Action Choose(Env& env, const std::vector<Action>& legal) override {
        if (env.decision().kind == DecisionKind::Combat) {
            std::vector<Action> plays = CombatPlays(legal);
            return plays.empty() ? Action{.kind = Action::EndTurn} : Pick(rng_, plays);
        }
        if (InCombat(env)) return Pick(rng_, legal);
        return strategic_.Choose(env, legal, rng_);
    }

private:
    Rand rng_;
    Rule0Strategic strategic_;
};

// The smoke command's one-step lookahead: each candidate runs on a copy of
// the true state, followed by the end of the turn.
double LookaheadScore(Env& tmp) {
    Sim& sim = tmp.sim();
    Creature player = Player(&sim, 1).Creature();
    if (player.IsDead()) return -1e9;
    if (!sim.g->combat.is_in_progress) return 1e6 + player.CurrentHp();
    double enemies = 0;
    for (Creature c : CombatState(&sim).Enemies())
        if (c.IsAlive()) enemies += c.CurrentHp() + c.Block() + 10.0;
    return player.CurrentHp() - enemies;
}

void SettleChoices(Env& tmp, Rand& rng) {
    for (int guard = 0; guard < 64 && tmp.decision().kind == DecisionKind::Choice; ++guard) {
        std::vector<Action> legal = tmp.LegalActions();
        if (legal.empty()) return;
        tmp.Step(Pick(rng, legal));
    }
}

Action LookaheadCombatAction(Env& env, const std::vector<Action>& legal, Rand& rng) {
    std::vector<Action> candidates = CombatPlays(legal);
    candidates.push_back(Action{.kind = Action::EndTurn});
    double best = -1e18;
    std::vector<Action> bests;
    for (const Action& a : candidates) {
        double score;
        try {
            std::unique_ptr<Env> tmp = env.Clone();
            tmp->Step(a);
            SettleChoices(*tmp, rng);
            if (a.kind != Action::EndTurn && tmp->decision().kind == DecisionKind::Combat) {
                tmp->Step(Action{.kind = Action::EndTurn});
                SettleChoices(*tmp, rng);
            }
            score = LookaheadScore(*tmp);
        } catch (const Unported&) {
            score = -1e12;
        }
        if (score > best + 1e-9) { best = score; bests.clear(); }
        if (score >= best - 1e-9) bests.push_back(a);
    }
    return Pick(rng, bests);
}

class LookaheadPolicy : public Policy {
public:
    PolicyClass Class() const override { return PolicyClass::SimulatedLookahead; }
    void Reset(uint64_t seed) override { rng_.seed(seed); strategic_.Reset(); }
    Action Choose(Env& env, const std::vector<Action>& legal) override {
        if (env.decision().kind == DecisionKind::Combat) return LookaheadCombatAction(env, legal, rng_);
        if (InCombat(env)) return Pick(rng_, legal);
        return strategic_.Choose(env, legal, rng_);
    }

private:
    Rand rng_;
    Rule0Strategic strategic_;
};

// Combat by the native CombatSolver: one whole-combat route from the true
// state, executed action by action. A route action that is not legal when
// its turn comes is counted and the combat is solved again from there.
class TeacherPolicy : public Policy {
public:
    explicit TeacherPolicy(const TeacherOptions& options) : options_(options) {}
    PolicyClass Class() const override { return PolicyClass::PrivilegedTeacher; }
    void Reset(uint64_t seed) override {
        rng_.seed(seed); strategic_.Reset(); route_.clear(); cursor_ = 0; was_in_combat_ = false;
    }
    Action Choose(Env& env, const std::vector<Action>& legal) override {
        const bool in_combat = InCombat(env);
        if (!in_combat) { was_in_combat_ = false; return strategic_.Choose(env, legal, rng_); }
        if (!was_in_combat_) { route_.clear(); cursor_ = 0; }
        was_in_combat_ = true;
        if (cursor_ < route_.size()) {
            const Action& next = route_[cursor_];
            if (std::find(legal.begin(), legal.end(), next) != legal.end()) { ++cursor_; return next; }
            ++stats.route_illegal;
        }
        route_.clear(); cursor_ = 0;
        try {
            const auto begin = std::chrono::steady_clock::now();
            combatsolver::CombatSearchResult result = combatsolver::CombatSolver::Solve(env, options_.config);
            TeacherSolve solve;
            solve.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
            solve.nodes = result.Metrics.ExpandedNodes;
            solve.route_actions = static_cast<int32_t>(result.Actions.size());
            solve.won = result.Quality.Won;
            solve.stop_reason = result.Metrics.StopReason;
            stats.solves.push_back(std::move(solve));
            route_ = std::move(result.Actions);
        } catch (const std::exception& e) {
            ++stats.teacher_errors[std::string(e.what()).substr(0, 120)];
        }
        if (!route_.empty() && std::find(legal.begin(), legal.end(), route_[0]) != legal.end()) {
            cursor_ = 1;
            return route_[0];
        }
        route_.clear();
        ++stats.teacher_fallback;
        if (env.decision().kind == DecisionKind::Combat) return LookaheadCombatAction(env, legal, rng_);
        return Pick(rng_, legal);
    }

private:
    TeacherOptions options_;
    Rand rng_;
    Rule0Strategic strategic_;
    std::vector<Action> route_;
    size_t cursor_ = 0;
    bool was_in_combat_ = false;
};

}  // namespace

std::unique_ptr<Policy> MakePolicy(const std::string& name, const TeacherOptions& teacher) {
    if (name == "random") return std::make_unique<RandomPolicy>();
    if (name == "rule0") return std::make_unique<Rule0Policy>();
    if (name == "lookahead") return std::make_unique<LookaheadPolicy>();
    if (name == "teacher") return std::make_unique<TeacherPolicy>(teacher);
    throw std::invalid_argument("unknown policy " + name);
}

}  // namespace sts2::nosl
