#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/Models/Powers/DampenPower.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct MagiKnight : MonsterModel {
    SIM_MODEL(MagiKnight, MonsterModel)

    int32_t MinInitialHp() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 89, 82); }
    int32_t MaxInitialHp() const { return MinInitialHp(); }
    int32_t PowerShieldDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 7, 6); }
    int32_t PowerShieldBlock() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 9, 5); }
    int32_t SpearDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 11, 10); }
    int32_t BombDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 40, 35); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState shield = b.Move("POWER_SHIELD_MOVE", SIM_MOVE(MagiKnight, PowerShieldMove),
                                  SingleAttackIntent(PowerShieldDamage()), DefendIntent());
        MoveState dampen = b.Move("DAMPEN_MOVE", SIM_MOVE(MagiKnight, DampenMove), DebuffIntent());
        MoveState prep = b.Move("PREP_MOVE", SIM_MOVE(MagiKnight, PrepMove), DefendIntent());
        MoveState bomb = b.Move("MAGIC_BOMB", SIM_MOVE(MagiKnight, MagicBombMove), SingleAttackIntent(BombDamage()));
        MoveState spear = b.Move("RAM_MOVE", SIM_MOVE(MagiKnight, SpearMove), SingleAttackIntent(SpearDamage()));
        shield.SetFollowUpState(dampen);
        dampen.SetFollowUpState(spear);
        spear.SetFollowUpState(prep);
        prep.SetFollowUpState(bomb);
        bomb.SetFollowUpState(spear);
        b.Add(shield);
        b.Add(dampen);
        b.Add(spear);
        b.Add(prep);
        b.Add(bomb);
        return b.Build(shield);
    }

    void PowerShieldMove(const CreatureList&) const {
        DamageCmd::Attack(PowerShieldDamage()).FromMonster(*this).Execute(nullptr);
        CreatureCmd::GainBlock(Creature(), Dec(PowerShieldBlock()), ValueProp::Move, nullptr);
    }

    void DampenMove(const CreatureList& targets) const {
        for (sts2::Creature target : targets) {
            PowerModel existing = target.GetPower(PowerId::DAMPEN_POWER);
            if (existing) {
                DampenPower(existing).AddCaster(Creature());
                continue;
            }
            PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
            PowerModel applied = PowerCmd::Apply(PowerId::DAMPEN_POWER, ctx, target, Dec(1), this->Creature(), CardModel{});
            if (applied) DampenPower(applied).AddCaster(Creature());
        }
    }

    void PrepMove(const CreatureList&) const {
        CreatureCmd::GainBlock(Creature(), Dec(PowerShieldBlock()), ValueProp::Move, nullptr);
    }

    void MagicBombMove(const CreatureList&) const {
        DamageCmd::Attack(BombDamage()).FromMonster(*this).Execute(nullptr);
    }

    void SpearMove(const CreatureList&) const {
        DamageCmd::Attack(SpearDamage()).FromMonster(*this).Execute(nullptr);
    }
};

}  // namespace sts2
