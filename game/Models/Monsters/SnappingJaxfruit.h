// Models/Monsters/SnappingJaxfruit.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct SnappingJaxfruit : MonsterModel {
    SIM_MODEL(SnappingJaxfruit, MonsterModel)

    int32_t EnergyDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 4, 3); }

    void AfterAddedToRoom() const {}
    void BeforeRemovedFromRoom() const {}

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState energy = b.Move("ENERGY_ORB_MOVE", SIM_MOVE(SnappingJaxfruit, EnergyOrb), SingleAttackIntent(EnergyDamage()), BuffIntent());
        energy.SetFollowUpState(energy);
        b.Add(energy);
        return b.Build(energy);
    }

    void EnergyOrb(const CreatureList& targets) const {
        DamageCmd::Attack(EnergyDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
};

}  // namespace sts2
