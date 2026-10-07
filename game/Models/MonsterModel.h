// Models/MonsterModel.cs. The instance lives in its creature's state
// (CreatureState::monster), so the handle index is the creature's.
#pragma once

#include <cstdint>
#include <string_view>

#include "core/rng.h"
#include "game/Models/AbstractModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

// MonsterModel.NextMove: a MoveState of the machine, or the STUNNED move.
class NextMove {
public:
    NextMove() = default;
    NextMove(Sim* sim, CreatureRef creature) : sim_(sim), creature_(creature) {}
    std::string_view Id() const;
    bool IsStunned() const;
    const MonsterStateDef* Def() const;  // null for STUNNED / UNSET_MOVE
    StaticVec<Intent, 4> Intents() const;
    bool CanTransitionAway() const;

private:
    MonsterModel Monster() const;
    Sim* sim_ = nullptr;
    CreatureRef creature_ = 0;
};

class MonsterModel : public AbstractModel {
public:
    static constexpr std::string_view stunnedMoveId = "STUNNED";
    // The synthetic MoveState is outside the immutable machine registry.
    // Only machine.current uses this index; NextMove has its own stunned flag.
    static constexpr int8_t stunnedStateIndex = -2;

    MonsterModel() = default;
    MonsterModel(Sim* sim, CreatureRef creature) : AbstractModel(sim, ModelKind::Monster, creature) {}

    MonsterState& st() const { return sim_->g->creatures[index_].monster; }
    MonsterId MonsterId_() const { return st().id; }
    const MonsterData& data() const { return sts2::data(st().id); }
    // ModelDb.Monster<T>().ToMutable(): a fresh monster in its own creature
    // pool slot, not yet a creature in any combat (CombatState::CreateCreature).
    static MonsterModel ToMutable(Sim* sim, MonsterId id);

    sts2::Creature Creature() const;
    sts2::CombatState CombatState() const;
    // MinInitialHp / MaxInitialHp: AscensionHelper.GetValueIfAscension(ToughEnemies, ...)
    // as exported per monster (MonsterData). Monsters whose HP depends on
    // their own fields must override these by hand.
    int32_t MinInitialHp() const;
    int32_t MaxInitialHp() const;
    sts2::Rng& Rng() const { return st().rng; }
    RunRngSet& RunRng() const { return sim_->g->run.rng; }
    bool IsPerformingMove() const { return st().is_performing_move; }
    bool SpawnedThisTurn() const { return st().spawned_this_turn; }
    sts2::NextMove NextMove() const { return sts2::NextMove(sim_, index_); }
    bool IntendsToAttack() const;

    // --- move machine
    bool HasMoveStateMachine() const { return st().machine.exists; }
    const MoveMachineDef& MachineDef() const;
    void SetUpForCombat();                       // MoveStateMachine = GenerateMoveStateMachine()
    void RollMove(const CreatureList& targets);  // NextMove = machine.RollMove(..., RunRng.MonsterAi)
    void SetMoveImmediate(std::string_view stateId, bool forceTransition = false);
    // Creature.StunInternal: NextMove = new MoveState("STUNNED", stunMove, StunIntent())
    // { FollowUpStateId = nextMoveId, MustPerformOnceBeforeTransitioning = true }.
    void SetMoveImmediateStunned(MoveFn stunMove, std::string_view nextMoveId);
    void PerformMove();
    // C# clears MoveStateMachine but keeps the independent NextMove object.
    // Retain its definition so NextMove.Intents remains readable after death.
    void ResetStateMachine() { st().machine.exists = false; }
    void OnSideSwitch() { st().spawned_this_turn = false; }
    std::string_view LastLoggedMoveId() const;  // StateLog.Last().Id

    // --- overridable members (MonsterModel.def)
#define SIM_MEMBER(Base, Ret, Name, Params, Args, Default) Ret Name Params const;
#define SIM_MEMBER_CUSTOM(Base, Ret, Name, Params, Args, Default) \
    Ret Name Params const;                                          \
    Ret Name##_Default Params const;
#include "game/Models/MonsterModel.def"
#undef SIM_MEMBER
#undef SIM_MEMBER_CUSTOM

protected:
    int32_t& extra(int i) const { return st().extra[i]; }

private:
    friend class sts2::NextMove;
    void FindNextMoveState(sts2::Creature owner, sts2::Rng& rng, bool logMove);
    void SetCurrentState(int8_t state);
    int8_t GetNextState(int8_t state, sts2::Rng& rng);
    float GetStateWeight(const StateWeight& w);
};

}  // namespace sts2
