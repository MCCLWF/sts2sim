// Models/Monsters/PunchConstruct.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"
namespace sts2 {
struct PunchConstruct : MonsterModel {
    SIM_MODEL(PunchConstruct, MonsterModel)
    bool StartsWithFastPunch() const { return extra(0) != 0; }
    void SetStartsWithFastPunch(bool v) const { extra(0) = v; }
    int32_t StartingHpReduction() const { return extra(1); }
    void SetStartingHpReduction(int32_t v) const { extra(1) = v; }
    int32_t StrongPunchDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 16, 14); }
    int32_t FastPunchDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 6, 5); }
    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState ready = b.Move("READY_MOVE", SIM_MOVE(PunchConstruct, ReadyMove), DefendIntent());
        MoveState strong = b.Move("STRONG_PUNCH_MOVE", SIM_MOVE(PunchConstruct, StrongPunchMove), SingleAttackIntent(StrongPunchDamage()));
        MoveState fast = b.Move("FAST_PUNCH_MOVE", SIM_MOVE(PunchConstruct, FastPunchMove), MultiAttackIntent(FastPunchDamage(), 2), DebuffIntent());
        ready.SetFollowUpState(fast); fast.SetFollowUpState(strong); strong.SetFollowUpState(ready);
        b.Add(ready); b.Add(fast); b.Add(strong);
        return b.Build(StartsWithFastPunch() ? fast : ready);
    }
    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::ARTIFACT_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
        if (StartingHpReduction() > 0) Creature().SetCurrentHpInternal(std::max(1, Creature().CurrentHp() - StartingHpReduction()));
    }
    void ReadyMove(const CreatureList&) const { CreatureCmd::GainBlock(Creature(), Dec(10), ValueProp::Move, nullptr); }
    void StrongPunchMove(const CreatureList&) const { DamageCmd::Attack(StrongPunchDamage()).FromMonster(*this).Execute(nullptr); }
    void FastPunchMove(const CreatureList& targets) const {
        DamageCmd::Attack(FastPunchDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, targets, Dec(1), Creature(), CardModel{});
    }
};
}
