// The decision loop over a run (M1 minimal form of the M3 environment).
//
// The run waits for the player at top-level decision points (RunData.phase:
// the map, a combat's play phase, the rewards screen, ...). A top-level action
// runs from a snapshot of the state at that point. When the game needs a
// choice in the middle of an effect (Sim::Choose: pick a card reward, ...), it
// throws NeedChoice; the environment keeps the snapshot and the answers so
// far, asks the agent, and re-runs the action from the snapshot with one more
// answer (docs/design-engine.md §6).
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "engine/map.h"
#include "engine/run_setup.h"
#include "engine/sim/sim.h"
#include "env/operation.h"

namespace sts2 {

enum class DecisionKind : uint8_t {
    Map,      // choose the next map point
    Combat,   // play a card or end the turn
    Rewards,  // take a reward or proceed
    Event,    // choose an event option
    RestSite, // choose a rest site option, or proceed
    Shop,     // buy an entry, or leave (Proceed)
    Treasure, // pick the relic, or skip (Proceed)
    Choice,   // answer a choice inside an effect (Decision::request)
    Ended,    // the run is over
    Boundary, // suspended native operation; only Resume is legal
};

struct Decision {
    DecisionKind kind = DecisionKind::Ended;
    ChoiceRequest request;  // DecisionKind::Choice
    std::optional<NeedBoundary> boundary;  // DecisionKind::Boundary
};

class Env {
public:
    static constexpr int kObservationSize = 2048;
    Env();
    ~Env();

    // A new run, then its start (EnterAct(0)) up to the first decision.
    void Reset(const NewRunOptions& options);
    // Test/benchmark entry: create a new run and enter a fixed encounter at
    // map coordinate (0, 0), stopping at the first player combat decision.
    // This is a test/benchmark entry, separate from normal run-level Reset.
    void ResetCombat(const NewRunOptions& options, EncounterId encounter, int32_t act_index = 0);
    Decision decision() const;
    std::optional<Operation> PendingOperation() const { return pending_operation_; }
    // Diagnostic from the last execution only; snapshot restore recomputes it
    // when a pending prefix completes. It is not hashed or serialized.
    const OperationResult& LastOperationResult() const { return last_operation_result_; }
    void Step(const Action& action);  // a top-level action, or Action::Answer to a pending choice
    // A validated native operation. Choice continuation uses ordinary Answer
    // actions and replays this exact command, including its RNG stream/batch.
    void Execute(const Operation& operation);
    std::unique_ptr<Env> PredictOperation(const Operation& operation) const;

    // M3 environment surface. The returned actions are in the same stable
    // order used by the CLI/oracle protocol; they are legal at the current
    // decision point only.
    std::vector<Action> LegalActions() const;
    // Pure one-action branch for generic consumers. The source is
    // unchanged; unported models in a combat source or successor, or reached
    // during the transition, throw Unported.
    std::unique_ptr<Env> Predict(const Action& action) const;
    // Conservative check for unported models already present in an active
    // combat decision or in-combat choice.
    // Future generated content is still checked when a transition reaches it.
    void ValidateCombatRoot() const;

    // Mask over the stable LegalActions() ordering, padded to at least 512.
    // Slot i is one when
    // LegalActions()[i] is available and zero otherwise.
    static constexpr size_t kActionMaskSize = 512;
    std::vector<uint8_t> ActionMask() const;

    // Clone is a deep copy of both the state and an in-progress choice prefix.
    std::unique_ptr<Env> Clone() const;

    // Stable logical-state hash. It includes a pending choice request when
    // one exists, but never exposes RNG state through the observation API.
    uint64_t Hash() const;

    // Privileged diagnostic/import-contract surface. Includes hidden RNG,
    // internal fields and choice replay state; never use as an agent observation.
    // Registry indices belong to this loaded native runtime, not a portable save.
    std::string ExportSnapshotJson() const;
    // Full canonical transport with native callback/definition identities.
    // Cross-process restore requires the identical native build fingerprint.
    // The wire hash is distinct from this runtime's Hash/reference binding.
    std::string ExportPortableSnapshotJson() const;
    // Transactional restore of a complete run/choice snapshot from this
    // loaded native runtime. Unknown schema, missing fields or invalid storage
    // are errors; active unported content remains a separate Unported result.
    static std::unique_ptr<Env> ImportSnapshotJson(std::string_view json);

    // Randomize hidden combat piles and future run queues in-place. The visible
    // hand, board, map, active room and all visible legal actions remain unchanged.
    void Determinize(uint64_t seed);

    // Integer observation encoding for agents. Card identities are included
    // only for the visible hand; draw/discard identities and RNG state are
    // deliberately omitted. The layout is documented in docs/design-engine.md.
    std::vector<int32_t> Observe() const;

    GameState& state() { return *g_; }
    Sim& sim() { return *sim_; }
    Sim& sim() const { return *sim_; }

private:
    Env(const Env& source);
    void Run(bool replay);

    std::unique_ptr<GameState> g_;
    // Immutable top-level decision snapshot. Branch clones can share it until
    // one starts a new action; choice-prefix replay only reads this snapshot.
    std::shared_ptr<const GameState> base_;
    std::unique_ptr<Sim> sim_;
    std::optional<Action> pending_;    // the top-level action being answered
    std::optional<Operation> pending_operation_;
    OperationResult last_operation_result_;
    std::optional<NeedBoundary> boundary_;
    uint64_t boundary_skipped_ = 0;
    std::vector<int32_t> answers_;
    std::optional<ChoiceRequest> choice_;
};

// M3 batch wrapper. Each member remains an independent Env; batching only
// shares the caller's step loop and never shares mutable simulation state.
class VectorEnv {
public:
    explicit VectorEnv(size_t count = 0);

    size_t size() const { return envs_.size(); }
    Env& at(size_t index);
    const Env& at(size_t index) const;
    void Reset(std::span<const NewRunOptions> options);
    void Step(size_t index, const Action& action);
    std::vector<uint64_t> Hashes() const;
    std::vector<std::vector<int32_t>> Observe() const;
    std::vector<std::vector<Action>> LegalActions() const;

private:
    std::vector<std::unique_ptr<Env>> envs_;
};

}  // namespace sts2
