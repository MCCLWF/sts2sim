// The deployed actor on observation tables alone: the actor filter, the
// native forward pass and the action draw. It needs no Env: the caller hands
// it the three tables of nosl.obs v1 and the legal actions they describe.
// nosl::Pilot (a shadow Env) and the game-side encoder (tools/NoslPilot) both
// decide through this one object, so equal tables give equal actions.
//
// `weights` is either one exported policy (python -m nosl.export), which then
// decides everywhere, or a policy bundle (python -m nosl.export_bundle), a
// text file naming up to three exported networks beside it:
//   nosl-policy-bundle 1
//   combat combat.bin      decides inside combat (the public in-combat flag)
//   base base.bin          optional: the base policy outside combat (default: the combat network)
//   strategy strategy.bin  optional: one value per action outside combat
//   margin 0.02            outside combat the highest-valued action replaces the base policy's most
//                          probable action only when its value is more than this above that action's
//   table table.tsv        optional, with a strategy network: values of kinds of action in kinds of decision
//   table_delta 0.01       (lines of context<TAB>label<TAB>value, tools/nosl/card_table.py). A decision whose
//                          context the table holds: when the best label's value is more than table_delta
//                          above the chosen action's label, an action with the best label replaces it (the
//                          one the strategy network values most). Contexts and labels are spelled as in
//                          python/nosl/contexts.py; this actor knows nested choices and rest sites.
//   readiness readiness.tsv   optional, with a strategy network: the readiness rule of python/nosl/readiness.py,
//   readiness_lambda 0.15     applied last. The file holds the door models of tools/nosl/boss_readiness.py
//   readiness_scope cards,upgrade   (python -m nosl.export_bundle writes it): per door its act, where it stands,
//   readiness_chain 0         the HP coefficient, the bosses it knows, per card the coefficients of holding at
//   readiness_table 1         least 1..5 copies and 1..3 upgraded copies, per boss and card their interaction,
//                          and per ascension the chain weights. A card reward, an upgrade target, a shop removal
//                          or a rest action is scored by its table value (readiness_table 1) plus lambda times
//                          what it does to the log-odds of the doors ahead: the act's boss door alone, or with
//                          readiness_chain 1 every door ahead with the file's weights.
//   turn_cap 300           optional (default 300, also without a bundle): a guard, not a rule the policies were
//                          trained with. Once the actor has taken that many decisions in one combat turn it ends
//                          the turn as soon as ending it is legal. A policy that never ends its turn by itself
//                          can spin for ever on a play that costs nothing (found in R4: a free card every time);
//                          in the simulator such a run is cut at a step limit, in the game nothing would stop it.
//                          0 turns the guard off. It reads the turn counter and its own count, nothing else.
//   combat_value combat_value.bin   optional: predicted advantages inside combat (python -m nosl.rollout_distill
//   combat_margin 0.05               --target advantage). When greedy: the policy's action, unless another of its four
//                          most probable actions is predicted to be more than combat_margin better. One more forward
//                          pass; nothing is simulated.
//   guards nodiscard,nobuyfull,takerewards,hold=0.5   optional: python/nosl/guards.py, applied last. Fixed corrections
//                          of plainly wasteful actions: nodiscard (no potion is thrown away while another action
//                          exists), nobuyfull / nopotionbuy (no potion bought with every slot taken / at all),
//                          takerewards (the reward screen is not left with gold, a relic, or a potion there is room
//                          for), hold=X (no potion in a fight that is neither an elite's nor a boss's while HP is at
//                          least X of the maximum and a slot is free), removefirst (the shop's card removal is bought
//                          while the deck holds an unupgraded basic Strike or Defend, and removes one of those). A
//                          forbidden choice falls back to the acting policy's best allowed action.
//                          smith=X: at a rest site a card is upgraded when HP is at least X of the maximum.
//   guards_low 5 hold=0.5,removefirst   optional: the set used instead of `guards` in runs at that ascension or below.
// With a strategy network the decision outside combat is that rule whatever
// `greedy` says; inside combat `greedy` and the policy seed apply as before.
// Every network reads the same observation tables. Nothing is simulated.
#pragma once

#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "nosl/actor_filter.h"
#include "nosl/observation.h"
#include "nosl/policy_net.h"

namespace sts2::nosl {

class Actor {
public:
    Actor(const std::string& weights, int32_t actor_filter, uint64_t policy_seed, bool greedy);

    struct Decision {
        Action action;      // the action taken
        size_t row = 0;     // its row in the filtered table
        size_t rows = 0;    // rows left by the filter
        bool forced = false;  // one row left: no network, no draw
        double prob = 1.0;
        float value = 0;
        bool replaced = false;  // outside combat: the strategy network's action replaced the base policy's
        bool capped = false;    // the turn cap ended the turn: no network was asked
    };
    // Prunes `obs` with the filter (in place), chooses, and records the choice for the filter.
    Decision Decide(Observation& obs);
    // The action really taken at the observation last given to Decide, when it was not Decide's own
    // (a check that follows a recorded run): the filter must remember what happened, not what was proposed.
    void Took(const Action& action) {
        filter_.OnAction(action);
        if (action.kind == Action::EndTurn) turn_decisions_ = 0;
    }

private:
    ActorFilter filter_;
    std::unique_ptr<PolicyNet> net_;       // inside combat; outside it too unless base_ is set
    std::unique_ptr<PolicyNet> base_;      // optional base policy outside combat
    std::unique_ptr<PolicyNet> strategy_;  // optional per-action values outside combat
    float margin_ = 0;
    std::unordered_map<std::string, std::unordered_map<std::string, double>> table_;  // context -> label -> value
    double table_delta_ = 0;
    // The table rule over the row already chosen outside combat (`values`: the strategy network's, per row).
    size_t TableRow(const Observation& obs, const std::vector<float>& values, size_t chosen) const;
    // The readiness rule (python/nosl/readiness.py) over the row the table rule left.
    struct Door {
        std::string name, which;          // which: "last" (the floor before a boss) or "first" (a hallway door)
        int32_t act = 0, floor = 0, passes = 0;
        double hp10 = 0;
        std::unordered_set<int32_t> bosses;
        std::unordered_map<int32_t, std::array<double, 8>> card;                      // card -> n1..n5, u1..u3
        std::unordered_map<int32_t, std::unordered_map<int32_t, double>> card_boss;   // boss -> card -> value
        std::unordered_map<int32_t, double> card_boss_mean;                           // card -> mean over the bosses
    };
    std::vector<Door> doors_;   // in the order of the chain
    std::unordered_map<int32_t, std::unordered_map<std::string, std::pair<double, double>>> door_weights_;  // ascension -> door -> (p, gain)
    double readiness_lambda_ = 0;
    bool readiness_chain_ = false, readiness_table_ = true;
    bool scope_cards_ = false, scope_upgrade_ = false, scope_remove_ = false, scope_rest_ = false;
    size_t ReadinessRow(const Observation& obs, const std::vector<float>& values, size_t chosen) const;
    std::unique_ptr<PolicyNet> combat_value_;   // optional predicted advantages inside combat
    float combat_margin_ = 0.05f;
    // The guards (python/nosl/guards.py) over the row every other rule left; `scores`: the acting policy's logits.
    struct GuardSet {
        bool nodiscard = false, nobuyfull = false, nopotionbuy = false, takerewards = false, removefirst = false, any = false;
        double hold = -1, smith = -1;       // HP fractions; below 0: off
    };
    GuardSet guards_, guards_low_;          // guards_low_ replaces guards_ in runs at ascension guards_low_max_ or below
    int32_t guards_low_max_ = -1;
    static GuardSet ParseGuards(const std::string& list);
    size_t GuardRow(const Observation& obs, size_t chosen, const std::vector<float>& scores) const;
    int32_t turn_cap_ = 300;        // decisions in one combat turn before the turn is ended; 0: no cap
    int32_t turn_ = -1;             // the combat turn the count belongs to
    int32_t turn_decisions_ = 0;
    std::mt19937_64 rng_;
    bool greedy_;
};

}  // namespace sts2::nosl
