// Models/Monsters/TheInsatiable.cs
#pragma once

#include "engine/sim/alloc.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/Models/Powers/SandpitPower.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct TheInsatiable : MonsterModel {
    SIM_MODEL(TheInsatiable, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);

    int32_t ThrashDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 9, 8); }
    int32_t BiteDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 31, 28); }
    int32_t SalivateStrength() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 3, 2); }

    void AfterDeath(PlayerChoiceContext&, sts2::Creature, bool, float) const {}  // Source hook only changes music.

    void LiquifyMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        for (sts2::Creature target : targets) {
            SandpitPower sandpit(PowerModel(sim_, alloc_power(*sim_->g, PowerId::SANDPIT_POWER)));
            sandpit.SetTarget(target);
            PowerCmd::Apply(ctx, sandpit, Creature(), Dec(4), Creature(), CardModel{});
        }
        for (sts2::Creature target : targets) {
            Player player = target.Player();
            if (!player) player = target.PetOwner();
            if (!player) continue;
            for (int32_t i = 0; i < 6; ++i) {
                CardModel card = CombatState().CreateCard(CardId::FRANTIC_ESCAPE, player);
                PileType pile = i < 3 ? PileType::Draw : PileType::Discard;
                CardPileCmd::AddGeneratedCardToCombat(card, pile, Player{}, CardPilePosition::Random);
            }
        }
    }
    void ThrashMove(const CreatureList&) const { DamageCmd::Attack(ThrashDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr); }
    void BiteMove(const CreatureList&) const { DamageCmd::Attack(BiteDamage()).FromMonster(*this).Execute(nullptr); }
    void SalivateMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(SalivateStrength()), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState liquify = b.Move("LIQUIFY_GROUND_MOVE", SIM_MOVE(TheInsatiable, LiquifyMove), BuffIntent(), StatusIntent(6));
        MoveState thrash = b.Move("THRASH_MOVE", SIM_MOVE(TheInsatiable, ThrashMove), MultiAttackIntent(ThrashDamage(), 2));
        MoveState thrash2 = b.Move("THRASH_MOVE_2", SIM_MOVE(TheInsatiable, ThrashMove), MultiAttackIntent(ThrashDamage(), 2));
        MoveState bite = b.Move("LUNGING_BITE_MOVE", SIM_MOVE(TheInsatiable, BiteMove), SingleAttackIntent(BiteDamage()));
        MoveState salivate = b.Move("SALIVATE_MOVE", SIM_MOVE(TheInsatiable, SalivateMove), BuffIntent());
        liquify.SetFollowUpState(thrash);
        thrash.SetFollowUpState(bite);
        bite.SetFollowUpState(salivate);
        salivate.SetFollowUpState(thrash2);
        thrash2.SetFollowUpState(thrash);
        b.Add(liquify);
        b.Add(bite);
        b.Add(thrash);
        b.Add(thrash2);
        b.Add(salivate);
        return b.Build(liquify);
    }
};

}  // namespace sts2
