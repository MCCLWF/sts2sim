#include "search/combatsolver/env_state.h"

#include <stdexcept>

#include "game/Combat/CombatState.h"
#include "game/Entities/Creatures/Creature.h"
#include "game/Entities/Players/Player.h"
#include "game/Entities/Players/PlayerCombatState.h"
#include "engine/sim/state_fields.generated.h"

namespace sts2::combatsolver {
namespace {
constexpr uint8_t kOutcomeStop = execution_boundary_bit(ExecutionBoundaryKind::SafeOutcome);

struct CloneStorage {
    size_t Bytes = sizeof(GameState) + sizeof(Env) + sizeof(Sim);
    template<class T> void operator()(const T& value) {
        if constexpr (!std::is_trivially_copyable_v<T>) visit_state_fields(value, *this);
    }
    template<class T, size_t N> void operator()(const std::array<T, N>& values) {
        for (const auto& value : values) (*this)(value);
    }
    template<class T, size_t N> void operator()(const T (&values)[N]) {
        for (const auto& value : values) (*this)(value);
    }
    template<class T, size_t N> void operator()(const StaticVec<T, N>& values) {
        constexpr size_t inlineSize = std::max(size_t{1}, std::min(N, size_t{16}));
        if (values.size() > inlineSize)
            Bytes += std::max(values.size(), inlineSize * 2) * sizeof(T) + 32;
        for (const auto& value : values) (*this)(value);
    }
    template<class T, uint16_t N> void operator()(const Pool<T, N>& values) {
        Bytes += values.clone_storage_bytes(values.capacity());
        values.visit_storage(*this);
    }
};
}

CombatSearchState CombatSearchState::Capture(const Env& root) {
    // Validate on an owned clone: even lazy native getters cannot modify root.
    CombatSearchState captured(root.Clone());
    captured.ValidateContinuation();
    return captured;
}
void CombatSearchState::ValidateContinuation() const {
    env_->ValidateCombatRoot();
    const auto decision = env_->decision();
    if (decision.kind == DecisionKind::Combat) return;
    const auto operation = env_->PendingOperation();
    if (decision.kind == DecisionKind::Choice && operation
        && operation->kind == Operation::Kind::Action && (operation->stop_mask & kOutcomeStop)) return;
    if (decision.kind == DecisionKind::Boundary && decision.boundary
        && decision.boundary->kind == ExecutionBoundaryKind::SafeOutcome) return;
    // A plain legacy choice root has a replay policy of zero. Answer cannot
    // replace that policy through the current public API, so scoring its
    // post-settlement HP would silently change upstream final ordering.
    throw std::runtime_error("CombatSolver Env continuation not ported: root lacks safe-outcome replay policy");
}
CombatSearchState CombatSearchState::Fork() const {
    CombatSearchState branch(env_->Clone());
    branch.native_hash_ = native_hash_;
    return branch;
}
uint64_t CombatSearchState::NativeHash() const {
    if (!native_hash_) native_hash_ = env_->Hash();
    return *native_hash_;
}
size_t CombatSearchState::CloneStorageEstimate() const {
    CloneStorage estimate;
    estimate(env_->state());
    return estimate.Bytes;
}
CombatOutcome CombatSearchState::Outcome() const {
    const auto decision = env_->decision();
    if (decision.kind != DecisionKind::Boundary) return CombatOutcome::Ongoing;
    if (!decision.boundary || decision.boundary->kind != ExecutionBoundaryKind::SafeOutcome)
        throw std::runtime_error("CombatSolver received an unsupported execution boundary");
    // PendingLoss is authoritative even if an effect subsequently healed HP.
    if (env_->sim().g->combat.pending_loss || Player(&env_->sim(), 1).Creature().IsDead())
        return CombatOutcome::Defeat;
    return CombatOutcome::Victory;
}
CombatFacts CombatSearchState::Facts() const {
    Player player(&env_->sim(), 1);
    return {player.Creature().CurrentHp(), player.Creature().MaxHp(),
        player.PlayerCombatState().TurnNumber(), CombatState(&env_->sim()).RoundNumber(),
        player.PlayerCombatState().Energy(), Outcome()};
}
std::vector<Action> CombatSearchState::LegalActions() const {
    // Env exposes Resume at a terminal boundary. Search terminates here;
    // settlement is a separate explicit operation, never a search edge.
    if (Outcome() != CombatOutcome::Ongoing) return {};
    return env_->LegalActions();
}
CombatSearchState CombatSearchState::Predict(const Action& action) const {
    if (Outcome() != CombatOutcome::Ongoing)
        throw std::invalid_argument("cannot expand a frozen CombatSolver terminal");
    std::unique_ptr<Env> branch;
    if (env_->decision().kind == DecisionKind::Choice) {
        if (action.kind != Action::Answer) throw std::invalid_argument("CombatSolver choice requires Answer");
        branch = env_->Clone();
        branch->Step(action);  // retains the top-level stop mask and answer prefix
    } else {
        if (action.kind != Action::PlayCard && action.kind != Action::EndTurn
            && action.kind != Action::UsePotion && action.kind != Action::DiscardPotion)
            throw std::invalid_argument("unsupported CombatSolver combat action");
        Operation operation;
        operation.kind = Operation::Kind::Action;
        operation.source_hash = NativeHash();
        operation.stop_mask = kOutcomeStop;
        operation.action = action;
        // This private source was coverage-validated at Capture or at the
        // previous successor. Execute still performs ordinary hash/legality
        // validation; ValidateContinuation checks newly reached content once.
        branch = env_->Clone();
        branch->Execute(operation);
    }
    CombatSearchState successor(std::move(branch));
    // Unexpected room/terminal transitions must be mapped explicitly before
    // they can participate in scoring. Never guess from a cleared enemy list.
    successor.ValidateContinuation();
    return successor;
}
std::unique_ptr<Env> CombatSearchState::SettleOutcome() const {
    if (Outcome() == CombatOutcome::Ongoing)
        throw std::invalid_argument("CombatSolver outcome has not been reached");
    return env_->Predict({.kind = Action::Resume});
}
}  // namespace sts2::combatsolver
