// Models/Monsters/AxeRubyRaider.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct AxeRubyRaider : MonsterModel {
    SIM_MODEL(AxeRubyRaider, MonsterModel)

    int32_t SwingDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 6, 5); }
    int32_t SwingBlock() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 6, 5); }
    int32_t BigSwingDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 13, 12); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState swing1 = b.Move("SWING_1", SIM_MOVE(AxeRubyRaider, SwingMove), SingleAttackIntent(SwingDamage()), DefendIntent());
        MoveState swing2 = b.Move("SWING_2", SIM_MOVE(AxeRubyRaider, SwingMove), SingleAttackIntent(SwingDamage()), DefendIntent());
        MoveState big = b.Move("BIG_SWING", SIM_MOVE(AxeRubyRaider, BigSwingMove), SingleAttackIntent(BigSwingDamage()));
        swing1.SetFollowUpState(swing2);
        swing2.SetFollowUpState(big);
        big.SetFollowUpState(swing1);
        b.Add(swing1);
        b.Add(swing2);
        b.Add(big);
        return b.Build(swing1);
    }

    void SwingMove(const CreatureList& targets) const {
        DamageCmd::Attack(SwingDamage()).FromMonster(*this).Execute(nullptr);
        CreatureCmd::GainBlock(Creature(), Dec(SwingBlock()), ValueProp::Move, nullptr);
    }
    void BigSwingMove(const CreatureList& targets) const { DamageCmd::Attack(BigSwingDamage()).FromMonster(*this).Execute(nullptr); }
};

}  // namespace sts2
