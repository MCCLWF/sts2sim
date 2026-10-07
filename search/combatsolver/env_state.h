#pragma once

#include <memory>
#include <string>
#include <vector>

#include "env/env.h"

namespace sts2::combatsolver {

enum class CombatOutcome { Ongoing, Victory, Defeat };

struct CombatFacts {
    int32_t PlayerHp;
    int32_t PlayerMaxHp;
    int32_t PlayerTurn;
    int32_t Round;
    int32_t Energy;
    CombatOutcome Outcome;
};

// A privileged search consumer. Every state owns a deep Env branch; callers
// receive only values or independent clones, never the internal mutable Sim.
class CombatSearchState {
public:
    static CombatSearchState Capture(const Env& root);
    CombatSearchState(CombatSearchState&&) noexcept = default;
    CombatSearchState& operator=(CombatSearchState&&) noexcept = default;
    CombatSearchState(const CombatSearchState&) = delete;
    CombatSearchState& operator=(const CombatSearchState&) = delete;

    CombatSearchState Fork() const;
    CombatSearchState Predict(const Action& action) const;
    std::vector<Action> LegalActions() const;
    Decision DecisionPoint() const { return env_->decision(); }
    CombatFacts Facts() const;
    CombatOutcome Outcome() const;
    uint64_t NativeHash() const;
    // Conservative native payload estimate for an ordinary owned clone;
    // excludes allocator-specific RSS and shared immutable replay storage.
    size_t CloneStorageEstimate() const;
    std::string ExportSnapshotJson() const { return env_->ExportSnapshotJson(); }
    std::unique_ptr<Env> CloneEnv() const { return env_->Clone(); }
    // Resume is explicit: the frozen search terminal retains combat facts;
    // downstream execution still applies ordinary victory hooks and rewards.
    std::unique_ptr<Env> SettleOutcome() const;

private:
    explicit CombatSearchState(std::unique_ptr<Env> env) : env_(std::move(env)) {}
    void ValidateContinuation() const;
    std::unique_ptr<Env> env_;
    mutable std::optional<uint64_t> native_hash_;
};

}  // namespace sts2::combatsolver
