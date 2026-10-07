// Models/Monsters/CrossbowRubyRaider.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct CrossbowRubyRaider : MonsterModel {
    SIM_MODEL(CrossbowRubyRaider, MonsterModel)

    bool IsCrossbowReloaded() const { return extra(0) != 0; }
    void SetIsCrossbowReloaded(bool value) const { extra(0) = value; }
    int32_t FireDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 16, 14); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState fire = b.Move("FIRE_MOVE", SIM_MOVE(CrossbowRubyRaider, FireMove), SingleAttackIntent(FireDamage()));
        MoveState reload = b.Move("RELOAD_MOVE", SIM_MOVE(CrossbowRubyRaider, ReloadMove), DefendIntent());
        fire.SetFollowUpState(reload);
        reload.SetFollowUpState(fire);
        b.Add(reload);
        b.Add(fire);
        return b.Build(reload);
    }

    void FireMove(const CreatureList& targets) const {
        DamageCmd::Attack(FireDamage()).FromMonster(*this).Execute(nullptr);
        SetIsCrossbowReloaded(false);
    }
    void ReloadMove(const CreatureList& targets) const {
        CreatureCmd::GainBlock(Creature(), Dec(3), ValueProp::Move, nullptr);
        SetIsCrossbowReloaded(true);
    }
};

}  // namespace sts2
