// Models/Monsters/Tunneler.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Tunneler : MonsterModel {
    SIM_MODEL(Tunneler, MonsterModel)

    bool IsStunned() const { return extra(0) != 0; }
    void SetIsStunned(bool value) const { extra(0) = value; }
    int32_t BiteDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 15, 13); }
    int32_t BlockGain() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 37, 32); }
    int32_t BelowDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 26, 23); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState bite = b.Move("BITE_MOVE", SIM_MOVE(Tunneler, BiteMove), SingleAttackIntent(BiteDamage()));
        MoveState burrow = b.Move("BURROW_MOVE", SIM_MOVE(Tunneler, BurrowMove), BuffIntent(), DefendIntent());
        MoveState below = b.Move("BELOW_MOVE", SIM_MOVE(Tunneler, BelowMove), SingleAttackIntent(BelowDamage()));
        MoveState dizzy = b.Move("DIZZY_MOVE", SIM_MOVE(Tunneler, StillDizzyMove), StunIntent());
        bite.SetFollowUpState(burrow);
        burrow.SetFollowUpState(below);
        below.SetFollowUpState(below);
        dizzy.SetFollowUpState(bite);
        b.Add(bite);
        b.Add(burrow);
        b.Add(below);
        b.Add(dizzy);
        return b.Build(bite);
    }

    void BiteMove(const CreatureList&) const {
        SetIsStunned(false);
        DamageCmd::Attack(BiteDamage()).FromMonster(*this).Execute(nullptr);
    }
    void BurrowMove(const CreatureList&) const {
        SetIsStunned(false);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::BURROWED_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
        CreatureCmd::GainBlock(Creature(), Dec(BlockGain()), ValueProp::Move, nullptr);
    }
    void BelowMove(const CreatureList&) const {
        SetIsStunned(false);
        DamageCmd::Attack(BelowDamage()).FromMonster(*this).Execute(nullptr);
    }
    void GetStunned() const { SetIsStunned(true); }
    void StillDizzyMove(const CreatureList&) const { SetIsStunned(false); }
};

}  // namespace sts2
