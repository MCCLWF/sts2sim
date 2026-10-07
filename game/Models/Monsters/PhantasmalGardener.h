// Models/Monsters/PhantasmalGardener.cs
#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"
namespace sts2 {
struct PhantasmalGardener : MonsterModel {
    SIM_MODEL(PhantasmalGardener, MonsterModel)
    bool ShouldFadeAfterDeath() const { return false; }
    int32_t EnlargeTriggers() const { return extra(0); }
    void SetEnlargeTriggers(int32_t v) const { extra(0) = v; }
    int32_t BiteDamage() const { return 5; }
    int32_t LashDamage() const { return 7; }
    int32_t FlailRepeat() const { return 3; }
    int32_t EnlargeStr() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 3, 2); }
    int32_t SkittishAmount() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 7, 6); }
    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b; MoveState bite=b.Move("BITE_MOVE",SIM_MOVE(PhantasmalGardener,BiteMove),SingleAttackIntent(BiteDamage())); MoveState lash=b.Move("LASH_MOVE",SIM_MOVE(PhantasmalGardener,LashMove),SingleAttackIntent(LashDamage())); MoveState flail=b.Move("FLAIL_MOVE",SIM_MOVE(PhantasmalGardener,FlailMove),MultiAttackIntent(1,FlailRepeat())); MoveState enlarge=b.Move("ENLARGE_MOVE",SIM_MOVE(PhantasmalGardener,EnlargeMove),BuffIntent()); ConditionalBranchState init=b.ConditionalBranch("INIT_MOVE");
        init.AddState(flail,SIM_COND(PhantasmalGardener,self.Creature().SlotName()=="first")); init.AddState(bite,SIM_COND(PhantasmalGardener,self.Creature().SlotName()=="second")); init.AddState(lash,SIM_COND(PhantasmalGardener,self.Creature().SlotName()=="third")); init.AddState(enlarge,SIM_COND(PhantasmalGardener,self.Creature().SlotName()=="fourth"));
        bite.SetFollowUpState(lash); lash.SetFollowUpState(flail); flail.SetFollowUpState(enlarge); enlarge.SetFollowUpState(bite); b.Add(init); b.Add(bite); b.Add(lash); b.Add(enlarge); b.Add(flail); return b.Build(init);
    }
    void AfterAddedToRoom() const { PlayerChoiceContext ctx=PlayerChoiceContext::Throwing(); PowerCmd::Apply(PowerId::SKITTISH_POWER,ctx,Creature(),Dec(SkittishAmount()),Creature(),CardModel{}); }
    void BiteMove(const CreatureList&) const { DamageCmd::Attack(BiteDamage()).FromMonster(*this).Execute(nullptr); }
    void LashMove(const CreatureList&) const { DamageCmd::Attack(LashDamage()).FromMonster(*this).Execute(nullptr); }
    void FlailMove(const CreatureList&) const { DamageCmd::Attack(1).WithHitCount(FlailRepeat()).FromMonster(*this).Execute(nullptr); }
    void EnlargeMove(const CreatureList&) const { PlayerChoiceContext ctx=PlayerChoiceContext::Throwing(); PowerCmd::Apply(PowerId::STRENGTH_POWER,ctx,Creature(),Dec(EnlargeStr()),Creature(),CardModel{}); SetEnlargeTriggers(EnlargeTriggers()+1); }
};
}
