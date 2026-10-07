#pragma once

#include "engine/sim/alloc.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/Cards/Wither.h"
#include "game/Models/MonsterModel.h"
#include "game/Models/Powers/WitheringPresencePower.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Aeonglass : MonsterModel {
    SIM_MODEL(Aeonglass, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    int32_t EbbDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 26, 22); }
    int32_t EbbBlock() const { return 33; }
    int32_t EyeLasersDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 12, 11); }
    int32_t WitherAmount() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 2, 1); }
    int32_t IncreasingIntensityBaseStrength() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 4, 3); }

    int32_t AdditionalStrength() const { return extra(0); }
    int32_t WitherUpgradeCount() const { return extra(1); }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        for (sts2::Creature player : CombatState().GetOpponentsOf(Creature())) {
            WitheringPresencePower presence(PowerModel(sim_, alloc_power(*sim_->g, PowerId::WITHERING_PRESENCE_POWER)));
            presence.SetTarget(player);
            presence.InitializeCounter();
            PowerCmd::Apply(ctx, presence, Creature(), Dec(6), Creature(), CardModel{});
        }
        PowerCmd::Apply(PowerId::ARTIFACT_POWER, ctx, Creature(), Dec(3), Creature(), CardModel{});
    }
    void AfterDeath(PlayerChoiceContext&, sts2::Creature, bool, float) const {}  // Source hook only changes music.

    void AfterCardGeneratedForCombat(CardModel card, Player) const {
        if (card.CardId_() != CardId::WITHER) return;
        Wither wither(card);
        for (int32_t i = 0; i < WitherUpgradeCount(); ++i) wither.FakeUpgrade();
    }

    void EbbMove(const CreatureList&) const {
        DamageCmd::Attack(EbbDamage()).FromMonster(*this).Execute(nullptr);
        CreatureCmd::GainBlock(Creature(), Dec(EbbBlock()), ValueProp::Move, nullptr);
    }
    void EyeLasersMove(const CreatureList&) const {
        DamageCmd::Attack(EyeLasersDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr);
    }
    void IncreasingIntensityMove(const CreatureList& targets) const {
        for (sts2::Creature target : targets) {
            Player player = target.Player();
            if (!player || !player.PlayerCombatState()) continue;
            for (CardModel card : player.PlayerCombatState().AllCards()) {
                if (card.CardId_() == CardId::WITHER) Wither(card).FakeUpgrade();
            }
        }
        ++extra(1);
        CardPileCmd::AddToCombatAndPreview(CardId::WITHER, targets, PileType::Discard, WitherAmount(), Player{});
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(),
                        Dec(IncreasingIntensityBaseStrength() + AdditionalStrength()), Creature(), CardModel{});
        ++extra(0);
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState ebb = b.Move("EBB_MOVE", SIM_MOVE(Aeonglass, EbbMove),
                               SingleAttackIntent(EbbDamage()), DefendIntent());
        MoveState lasers = b.Move("EYE_LASERS_MOVE", SIM_MOVE(Aeonglass, EyeLasersMove),
                                  MultiAttackIntent(EyeLasersDamage(), 2));
        MoveState intensity = b.Move("INCREASING_INTENSITY_MOVE", SIM_MOVE(Aeonglass, IncreasingIntensityMove),
                                     StatusIntent(WitherAmount()), BuffIntent());
        ebb.SetFollowUpState(lasers);
        lasers.SetFollowUpState(intensity);
        intensity.SetFollowUpState(ebb);
        b.Add(ebb);
        b.Add(lasers);
        b.Add(intensity);
        return b.Build(ebb);
    }
};

}  // namespace sts2
