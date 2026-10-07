// MonsterMoves/MonsterMoveStateMachine/*.cs
//
// A monster's move machine is split in two:
//  - the definition (states, their moves, weights, conditions, intents) is
//    code; MonsterModel::GenerateMoveStateMachine of each monster builds it
//    with MoveMachineBuilder, mirroring the C# construction line by line. The
//    result is interned, so equal definitions are shared across combats;
//  - the run-time part (current state, log, per-state flags) is plain data in
//    MonsterState::machine.
//
// Callbacks cannot capture: they receive the monster handle.
//   MoveState m = b.Move("WHIRL_MOVE", SIM_MOVE(Toadpole, WhirlMove), SingleAttackIntent(WhirlDamage()));
//   cond.AddState(m, SIM_COND(Toadpole, !self.IsFront()));
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "content/generated/enums.h"
#include "core/dec.h"
#include "game/fwd.h"
#include "game/Models/signature_types.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/NativeCallback.h"

namespace sts2 {

// Stable source identity for live-root binding. This belongs to immutable
// registry metadata, never to GameState or a callback's mutable captures.
struct NamedMoveFn {
    MoveFn fn;
    std::string_view name;
    operator MoveFn() const { return fn; }
};

#define SIM_MOVE(Cls, Method) \
    ::sts2::NamedMoveFn{STS2_NATIVE_CALLBACK(Move, Cls, \
        ([](::sts2::MonsterModel m, const ::sts2::CreatureList& t) { Cls(m).Method(t); }), #Method), #Cls "." #Method}
#define SIM_COND(Cls, Expr)                  \
    STS2_NATIVE_CALLBACK(Condition, Cls, ([](::sts2::MonsterModel m) -> bool { \
        Cls self(m);                         \
        return (Expr);                       \
    }), #Expr)
#define SIM_WEIGHT(Cls, Expr)                \
    STS2_NATIVE_CALLBACK(Weight, Cls, ([](::sts2::MonsterModel m) -> float { \
        Cls self(m);                         \
        return static_cast<float>(Expr);     \
    }), #Expr)

// MonsterMoves/Intents: what a move shows. AbstractIntent subclasses map to
// IntentType; attack intents carry their damage (a constant, or a calculator
// receiving the monster, for the C# Func<decimal> constructors).
struct Intent {
    IntentType type{};
    int32_t damage = 0;
    int32_t repeat = 0;                     // MultiAttackIntent; 0 for single attacks
    bool multi = false;                     // MultiAttackIntent (else SingleAttackIntent / DeathBlowIntent)
    IntentDamageFn damageCalc = nullptr;
    IntentRepeatFn repeatCalc = nullptr;
    bool strong = false;                    // DebuffIntent(strong)
    int32_t count = 0;                      // StatusIntent(count)
    friend bool operator==(const Intent&, const Intent&) = default;
};
Intent SingleAttackIntent(int32_t damage);
Intent SingleAttackIntent(IntentDamageFn damageCalc);
Intent MultiAttackIntent(int32_t damage, int32_t repeat);
Intent MultiAttackIntent(int32_t damage, IntentRepeatFn repeatCalc);
Intent DeathBlowIntent(IntentDamageFn damageCalc);
Intent BuffIntent();
Intent DebuffIntent(bool strong = false);
Intent DefendIntent();
Intent StunIntent();
Intent SleepIntent();
Intent EscapeIntent();
Intent HealIntent();
Intent HiddenIntent();
Intent SummonIntent();
Intent CardDebuffIntent();
Intent StatusIntent(int32_t count);
Intent UnknownIntent();

enum class MonsterStateKind : uint8_t { Move, RandomBranch, ConditionalBranch };

struct StateWeight {  // RandomBranchState.StateWeight
    int8_t state = -1;
    MoveRepeatType repeatType{};
    int32_t maxTimes = 0;
    WeightFn weightLambda = nullptr;  // null: constant `weight`
    float weight = 1.0f;
    int32_t cooldown = 0;
    friend bool operator==(const StateWeight&, const StateWeight&) = default;
};

struct ConditionalBranch {
    int8_t state = -1;
    CondFn condition = nullptr;  // null evaluates to 1
    friend bool operator==(const ConditionalBranch&, const ConditionalBranch&) = default;
};

struct MonsterStateDef {
    MonsterStateKind kind = MonsterStateKind::Move;
    std::string id;
    // MoveState
    MoveFn onPerform = nullptr;
    std::string onPerformName;
    std::vector<Intent> intents;
    int8_t followUp = -1;           // FollowUpState, or FollowUpStateId resolved
    std::string followUpId;         // FollowUpStateId naming a state outside this machine
    bool mustPerformOnceBeforeTransitioning = false;
    // RandomBranchState / ConditionalBranchState
    std::vector<StateWeight> weights;
    std::vector<ConditionalBranch> branches;
    friend bool operator==(const MonsterStateDef&, const MonsterStateDef&) = default;
};

struct MoveMachineDef {
    std::vector<MonsterStateDef> states;  // registration order (States dictionary)
    int8_t initial = -1;
    friend bool operator==(const MoveMachineDef&, const MoveMachineDef&) = default;
    int8_t Find(std::string_view id) const;
};

// Move callbacks stored in state (the STUNNED move's logic) by id; 0 = none.
uint16_t register_move_fn(MoveFn fn);
MoveFn registered_move_fn(uint16_t id);

// Interned definitions: index 0 is unused.
uint16_t intern_move_machine(MoveMachineDef def, MonsterId owner);
const MoveMachineDef& move_machine_def(uint16_t index);
bool move_machine_matches_model(uint16_t index, MonsterId owner);

class MoveMachineBuilder;
class RandomBranchState;
class ConditionalBranchState;

// Handles on states while building (the builder owns the definitions).
class MoveState {
public:
    MoveState() = default;
    MoveState(MoveMachineBuilder* b, int8_t i) : b_(b), i_(i) {}
    int8_t index() const { return i_; }
    std::string_view Id() const;
    // FollowUpState = next (any kind of state).
    void SetFollowUpState(MoveState next) const;
    void SetFollowUpState(const RandomBranchState& next) const;
    void SetFollowUpState(const ConditionalBranchState& next) const;
    void SetFollowUpStateId(std::string_view id) const;
    void SetMustPerformOnceBeforeTransitioning(bool v) const;

private:
    MoveMachineBuilder* b_ = nullptr;
    int8_t i_ = -1;
};

class RandomBranchState {
public:
    RandomBranchState() = default;
    RandomBranchState(MoveMachineBuilder* b, int8_t i) : b_(b), i_(i) {}
    int8_t index() const { return i_; }
    // The C# overloads, argument for argument (docs/audits/pitfalls.md §4).
    void AddBranch(MoveState state, int32_t cooldown, MoveRepeatType repeatType, WeightFn weight) const;
    void AddBranch(MoveState state, int32_t cooldown, int32_t maxRepeats, WeightFn weight) const;
    void AddBranch(MoveState state, int32_t maxRepeats, WeightFn weight) const;
    void AddBranch(MoveState state, int32_t cooldown, MoveRepeatType repeatType, float weight) const;
    void AddBranch(MoveState state, MoveRepeatType repeatType, float weight) const;
    void AddBranch(MoveState state, MoveRepeatType repeatType, WeightFn weight) const;
    void AddBranch(MoveState state, int32_t maxRepeats, float weight) const;
    void AddBranch(MoveState state, int32_t cooldown, MoveRepeatType repeatType) const;
    void AddBranch(MoveState state, int32_t maxRepeats) const;
    void AddBranch(MoveState state, MoveRepeatType repeatType) const;

private:
    void Add(StateWeight w) const;
    MoveMachineBuilder* b_ = nullptr;
    int8_t i_ = -1;
};

class ConditionalBranchState {
public:
    ConditionalBranchState() = default;
    ConditionalBranchState(MoveMachineBuilder* b, int8_t i) : b_(b), i_(i) {}
    int8_t index() const { return i_; }
    void AddState(MoveState move, CondFn condition) const;
    void AddState(RandomBranchState branch, CondFn condition) const;

private:
    MoveMachineBuilder* b_ = nullptr;
    int8_t i_ = -1;
};

// `new MoveState(...)` / `new RandomBranchState(...)` / `list.Add(...)` /
// `new MonsterMoveStateMachine(list, initial)`.
class MoveMachineBuilder {
public:
    template <class... I>
    MoveState Move(std::string_view id, NamedMoveFn onPerform, I... intents) {
        MoveState state = Move(id, onPerform.fn, intents...);
        def(state.index()).onPerformName = std::string(onPerform.name);
        return state;
    }

    template <class... I>
    MoveState Move(std::string_view id, MoveFn onPerform, I... intents) {
        MonsterStateDef d;
        d.kind = MonsterStateKind::Move;
        d.id = std::string(id);
        d.onPerform = onPerform;
        (d.intents.push_back(intents), ...);
        return MoveState(this, Create(std::move(d)));
    }
    RandomBranchState RandomBranch(std::string_view id);
    ConditionalBranchState ConditionalBranch(std::string_view id);

    // list.Add(state): the order states are registered in.
    void Add(MoveState s) { order_.push_back(s.index()); }
    void Add(RandomBranchState s) { order_.push_back(s.index()); }
    void Add(ConditionalBranchState s) { order_.push_back(s.index()); }

    template <class S>
    MoveMachineDef Build(S initialState) {
        return Finish(initialState.index());
    }

    MonsterStateDef& def(int8_t i) { return created_[static_cast<size_t>(i)]; }

private:
    int8_t Create(MonsterStateDef d);
    MoveMachineDef Finish(int8_t initial);

    std::vector<MonsterStateDef> created_;  // in construction order
    std::vector<int8_t> order_;             // registration order
};

}  // namespace sts2
