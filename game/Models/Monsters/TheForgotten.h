// Models/Monsters/TheForgotten.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct TheForgotten : MonsterModel {
    SIM_MODEL(TheForgotten, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    int32_t DreadDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 15, 13) + Creature().GetPowerAmount(PowerId::DEXTERITY_POWER);
    }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::POSSESS_SPEED_POWER, ctx, Creature(), Dec(1), sts2::Creature{}, CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState miasma = b.Move("MIASMA", SIM_MOVE(TheForgotten, MiasmaMove), DebuffIntent(), DefendIntent(), BuffIntent());
        MoveState dread = b.Move("DREAD", SIM_MOVE(TheForgotten, DreadMove), SingleAttackIntent(SIM_INTENT_DAMAGE(TheForgotten, self.DreadDamage())));
        miasma.SetFollowUpState(dread);
        dread.SetFollowUpState(miasma);
        b.Add(miasma);
        b.Add(dread);
        return b.Build(miasma);
    }

    void MiasmaMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, ctx, targets, Dec(-2), Creature(), CardModel{});
        CreatureCmd::GainBlock(Creature(), Dec(8), ValueProp::Move, nullptr);
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
    void DreadMove(const CreatureList&) const {
        DamageCmd::Attack(DreadDamage()).FromMonster(*this).Execute(nullptr);
    }
};

}  // namespace sts2
