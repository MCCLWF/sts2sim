// Models/Monsters/Vantom.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Vantom : MonsterModel {
    SIM_MODEL(Vantom, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    int32_t SlipperyAmt() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 9, 8);
    }
    int32_t InkBlotDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 8, 7);
    }
    int32_t InkyLanceDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 7, 6);
    }
    int32_t DismemberDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 30, 26);
    }

    bool ShouldDisappearFromDoom() const { return false; }

    // C# also updates the boss music track after death; presentation only.
    void AfterDeath(PlayerChoiceContext&, sts2::Creature, bool, float) const {}

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::SLIPPERY_POWER, ctx, Creature(), Dec(SlipperyAmt()), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState inkBlot = b.Move("INK_BLOT_MOVE", SIM_MOVE(Vantom, InkBlotMove), SingleAttackIntent(InkBlotDamage()));
        MoveState inkyLance = b.Move("INKY_LANCE_MOVE", SIM_MOVE(Vantom, InkyLanceMove),
                                     MultiAttackIntent(InkyLanceDamage(), 2));
        MoveState dismember = b.Move("DISMEMBER_MOVE", SIM_MOVE(Vantom, DismemberMove),
                                     SingleAttackIntent(DismemberDamage()), StatusIntent(3));
        MoveState prepare = b.Move("PREPARE_MOVE", SIM_MOVE(Vantom, PrepareMove), BuffIntent());
        inkBlot.SetFollowUpState(inkyLance);
        inkyLance.SetFollowUpState(dismember);
        dismember.SetFollowUpState(prepare);
        prepare.SetFollowUpState(inkBlot);
        b.Add(inkBlot);
        b.Add(inkyLance);
        b.Add(dismember);
        b.Add(prepare);
        return b.Build(inkBlot);
    }

    void InkBlotMove(const CreatureList&) const {
        DamageCmd::Attack(InkBlotDamage()).FromMonster(*this).Execute(nullptr);
    }
    void InkyLanceMove(const CreatureList&) const {
        DamageCmd::Attack(InkyLanceDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr);
    }
    void DismemberMove(const CreatureList& targets) const {
        DamageCmd::Attack(DismemberDamage()).FromMonster(*this).Execute(nullptr);
        CardPileCmd::AddToCombatAndPreview(CardId::WOUND, targets, PileType::Discard, 3, Player{});
    }
    void PrepareMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
};

}  // namespace sts2
