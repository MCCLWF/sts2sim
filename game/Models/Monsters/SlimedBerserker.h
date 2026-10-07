// Models/Monsters/SlimedBerserker.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct SlimedBerserker : MonsterModel {
    SIM_MODEL(SlimedBerserker, MonsterModel)

    int32_t PummelingDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 5, 4); }
    int32_t SmotherDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 33, 30); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState vomit = b.Move("VOMIT_ICHOR_MOVE", SIM_MOVE(SlimedBerserker, VomitIchorMove), StatusIntent(10));
        MoveState hug = b.Move("LEECHING_HUG_MOVE", SIM_MOVE(SlimedBerserker, LeechingHugMove), DebuffIntent(), BuffIntent());
        MoveState smother = b.Move("SMOTHER_MOVE", SIM_MOVE(SlimedBerserker, SmotherMove), SingleAttackIntent(SmotherDamage()));
        MoveState pummel = b.Move("FURIOUS_PUMMELING_MOVE", SIM_MOVE(SlimedBerserker, FuriousPummelingMove), MultiAttackIntent(PummelingDamage(), 4));
        vomit.SetFollowUpState(pummel);
        pummel.SetFollowUpState(hug);
        hug.SetFollowUpState(smother);
        smother.SetFollowUpState(vomit);
        b.Add(vomit);
        b.Add(smother);
        b.Add(hug);
        b.Add(pummel);
        return b.Build(vomit);
    }

    void VomitIchorMove(const CreatureList& targets) const {
        CardPileCmd::AddToCombatAndPreview(CardId::SLIMED, targets, PileType::Discard, 10, Player{});
    }

    void LeechingHugMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, targets, Dec(3), sts2::Creature{}, CardModel{});
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(3), Creature(), CardModel{});
    }

    void FuriousPummelingMove(const CreatureList&) const {
        DamageCmd::Attack(PummelingDamage()).WithHitCount(4).FromMonster(*this).Execute(nullptr);
    }

    void SmotherMove(const CreatureList&) const {
        DamageCmd::Attack(SmotherDamage()).FromMonster(*this).Execute(nullptr);
    }
};

}  // namespace sts2
