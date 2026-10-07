// Models/Monsters/TerrorEel.cs
#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"
namespace sts2 {
struct TerrorEel : MonsterModel {
    SIM_MODEL(TerrorEel, MonsterModel)
    int32_t ShriekAmount() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 75, 70); }
    int32_t CrashDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 18, 16); }
    int32_t ThrashDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 4, 3); }
    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b; MoveState crash=b.Move("CRASH_MOVE",SIM_MOVE(TerrorEel,CrashMove),SingleAttackIntent(CrashDamage()));
        MoveState thrash=b.Move("THRASH_MOVE",SIM_MOVE(TerrorEel,ThrashMove),MultiAttackIntent(ThrashDamage(),3),BuffIntent());
        MoveState stun=b.Move("STUN_MOVE",SIM_MOVE(TerrorEel,StunMove),StunIntent());
        MoveState terror=b.Move("TERROR_MOVE",SIM_MOVE(TerrorEel,TerrorMove),DebuffIntent());
        crash.SetFollowUpState(thrash); thrash.SetFollowUpState(crash); stun.SetFollowUpState(terror); terror.SetFollowUpState(crash);
        b.Add(crash); b.Add(thrash); b.Add(stun); b.Add(terror); return b.Build(crash);
    }
    void AfterAddedToRoom() const { PlayerChoiceContext ctx=PlayerChoiceContext::Throwing(); PowerCmd::Apply(PowerId::SHRIEK_POWER,ctx,Creature(),Dec(ShriekAmount()),Creature(),CardModel{}); }
    void CrashMove(const CreatureList&) const { DamageCmd::Attack(CrashDamage()).FromMonster(*this).Execute(nullptr); }
    void ThrashMove(const CreatureList&) const { DamageCmd::Attack(ThrashDamage()).WithHitCount(3).FromMonster(*this).Execute(nullptr); PlayerChoiceContext ctx=PlayerChoiceContext::Throwing(); PowerCmd::Apply(PowerId::VIGOR_POWER,ctx,Creature(),Dec(6),Creature(),CardModel{}); }
    void StunMove(const CreatureList&) const {}
    void TerrorMove(const CreatureList& targets) const { PlayerChoiceContext ctx=PlayerChoiceContext::Throwing(); PowerCmd::Apply(PowerId::VULNERABLE_POWER,ctx,targets,Dec(99),Creature(),CardModel{}); }
};
}
