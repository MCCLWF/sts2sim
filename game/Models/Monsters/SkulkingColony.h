// Models/Monsters/SkulkingColony.cs
#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"
namespace sts2 {
struct SkulkingColony : MonsterModel {
    SIM_MODEL(SkulkingColony, MonsterModel)
    int32_t InertiaDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 11, 9); }
    int32_t ZoomDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 16, 14); }
    int32_t PiercingStabsDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 8, 7); }
    int32_t InertiaStrengthGain() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 4, 2); }
    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b; MoveState zoom=b.Move("ZOOM_MOVE",SIM_MOVE(SkulkingColony,ZoomMove),SingleAttackIntent(ZoomDamage())); MoveState zoom2=b.Move("ZOOM_MOVE_2",SIM_MOVE(SkulkingColony,ZoomMove),SingleAttackIntent(ZoomDamage())); MoveState inertia=b.Move("INERTIA_MOVE",SIM_MOVE(SkulkingColony,InertiaMove),SingleAttackIntent(InertiaDamage()),BuffIntent()); MoveState stabs=b.Move("PIERCING_STABS_MOVE",SIM_MOVE(SkulkingColony,PiercingStabsMove),MultiAttackIntent(PiercingStabsDamage(),2));
        zoom.SetFollowUpState(zoom2); zoom2.SetFollowUpState(inertia); inertia.SetFollowUpState(stabs); stabs.SetFollowUpState(zoom); b.Add(zoom); b.Add(zoom2); b.Add(inertia); b.Add(stabs); return b.Build(zoom);
    }
    void AfterAddedToRoom() const { PlayerChoiceContext ctx=PlayerChoiceContext::Throwing(); PowerCmd::Apply(PowerId::HARDENED_SHELL_POWER,ctx,Creature(),Dec(20),Creature(),CardModel{}); }
    void InertiaMove(const CreatureList&) const { DamageCmd::Attack(InertiaDamage()).FromMonster(*this).Execute(nullptr); PlayerChoiceContext ctx=PlayerChoiceContext::Throwing(); PowerCmd::Apply(PowerId::STRENGTH_POWER,ctx,Creature(),Dec(InertiaStrengthGain()),Creature(),CardModel{}); }
    void PiercingStabsMove(const CreatureList&) const { DamageCmd::Attack(PiercingStabsDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr); }
    void ZoomMove(const CreatureList&) const { DamageCmd::Attack(ZoomDamage()).FromMonster(*this).Execute(nullptr); }
};
}
