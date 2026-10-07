// Models/Monsters/SewerClam.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"
namespace sts2 {
struct SewerClam : MonsterModel {
    SIM_MODEL(SewerClam, MonsterModel)
    int32_t JetDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 11, 10); }
    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState press = b.Move("PRESSURIZE_MOVE", SIM_MOVE(SewerClam, PressurizeMove), BuffIntent());
        MoveState jet = b.Move("JET_MOVE", SIM_MOVE(SewerClam, JetMove), SingleAttackIntent(JetDamage()));
        press.SetFollowUpState(jet); jet.SetFollowUpState(press); b.Add(press); b.Add(jet); return b.Build(jet);
    }
    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        int32_t v = AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 9, 8);
        PowerCmd::Apply(PowerId::PLATING_POWER, ctx, Creature(), Dec(v), Creature(), CardModel{});
    }
    void PressurizeMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(4), Creature(), CardModel{});
    }
    void JetMove(const CreatureList&) const { DamageCmd::Attack(JetDamage()).FromMonster(*this).Execute(nullptr); }
};
}
