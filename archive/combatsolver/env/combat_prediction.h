// Owned combat-only execution branch. Ordinary Env and its v2 snapshot remain
// the run environment; this separate envelope carries prediction metadata.
#pragma once

#include "env/env.h"

namespace sts2 {

class CombatPredictionSession {
public:
    static std::unique_ptr<CombatPredictionSession> FromGameSnapshot(std::string_view json);
    static std::unique_ptr<CombatPredictionSession> ImportSnapshotJson(std::string_view json);
    std::unique_ptr<CombatPredictionSession> Clone() const;
    std::unique_ptr<CombatPredictionSession> Predict(const Action& action) const;
    std::vector<Action> LegalActions() const;
    uint64_t Hash() const;
    std::string ExportSnapshotJson() const;
    const std::optional<CombatPredictionTerminalStamp>& terminal() const { return boundary_.terminal; }
    const Env& environment() const { return *env_; }

private:
    explicit CombatPredictionSession(std::unique_ptr<Env> env) : env_(std::move(env)) {}
    void ValidateBoundary() const;
    std::unique_ptr<Env> env_;
    CombatPredictionBoundary boundary_;
    // Matches Env's stationary action base, including an explicitly present
    // nonterminal boundary. Answer restores both before replaying the prefix.
    std::optional<CombatPredictionBoundary> replay_boundary_base_;
};

}  // namespace sts2
