// Models/Monsters/Myte.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Myte : MonsterModel {
    SIM_MODEL(Myte, MonsterModel)

    int32_t BiteDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 15, 13); }
    int32_t SuckDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 6, 4); }
    int32_t SuckStrength() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 3, 2); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState toxic = b.Move("TOXIC_MOVE", SIM_MOVE(Myte, ToxicMove), StatusIntent(2));
        MoveState bite = b.Move("BITE_MOVE", SIM_MOVE(Myte, BiteMove), SingleAttackIntent(BiteDamage()));
        MoveState suck = b.Move("SUCK_MOVE", SIM_MOVE(Myte, SuckMove), SingleAttackIntent(SuckDamage()), BuffIntent());
        ConditionalBranchState initial = b.ConditionalBranch("INIT_MOVE");
        initial.AddState(toxic, SIM_COND(Myte, self.Creature().SlotName() == "first"));
        initial.AddState(suck, SIM_COND(Myte, self.Creature().SlotName() == "second"));
        toxic.SetFollowUpState(bite);
        bite.SetFollowUpState(suck);
        suck.SetFollowUpState(toxic);
        b.Add(initial);
        b.Add(toxic);
        b.Add(bite);
        b.Add(suck);
        return b.Build(initial);
    }

    void ToxicMove(const CreatureList& targets) const {
        CardPileCmd::AddToCombatAndPreview(CardId::TOXIC, targets, PileType::Hand, 2, Player{});
    }
    void BiteMove(const CreatureList&) const { DamageCmd::Attack(BiteDamage()).FromMonster(*this).Execute(nullptr); }
    void SuckMove(const CreatureList&) const {
        DamageCmd::Attack(SuckDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(SuckStrength()), Creature(), CardModel{});
    }
};

}  // namespace sts2
