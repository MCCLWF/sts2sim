// Public-rule pruning of the actor's action table (a policy-side rule, not a
// game rule: the Env still accepts every action it removes).
//
// Version 1, found necessary by the Q2 policies
// (docs/reports/2026-10-03-nosl-q2-learning-loop.md):
//   - An action that led back to the very same observation (open a card
//     reward and skip it, start a card removal and cancel it) is removed
//     the next time that observation comes up on the same floor, so a
//     policy cannot spin on a free, undone action.
//   - Discarding a potion is offered only while every potion slot is full.
// It reads observations and the actions already taken, nothing else, and it
// never removes the last remaining action. Rows keep their order.
#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "nosl/observation.h"

namespace sts2::nosl {

inline constexpr int32_t kActorFilterNone = 0;
inline constexpr int32_t kActorFilterV1 = 1;

class ActorFilter {
public:
    explicit ActorFilter(int32_t version = kActorFilterNone) : version_(version) {}
    int32_t version() const { return version_; }
    void Reset();
    // Prune `obs` in place (rows, legal actions and the action count).
    void Apply(Observation& obs);
    // The policy took `action` at the observation last given to Apply.
    void OnAction(const Action& action);

private:
    int32_t version_;
    int32_t floor_ = -1;
    uint64_t current_ = 0;  // hash of the unpruned observation last seen
    std::unordered_map<uint64_t, Action> last_action_;           // what was taken from an observation
    std::unordered_map<uint64_t, std::vector<Action>> banned_;   // actions that came straight back to it
};

}  // namespace sts2::nosl
