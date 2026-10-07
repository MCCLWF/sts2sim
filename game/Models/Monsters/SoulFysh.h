// Models/Monsters/SoulFysh.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct SoulFysh : MonsterModel {
    SIM_MODEL(SoulFysh, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    // These overrides only update the Soul Fysh/Beckon music parameters.
    void AfterDeath(PlayerChoiceContext&, sts2::Creature, bool, float) const {}
    void AfterCardChangedPilesLate(CardModel, PileType, AbstractModel) const {}

    int32_t DeGasDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 18, 16);
    }
    int32_t ScreamDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 15, 13);
    }
    int32_t GazeDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 8, 7);
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState beckon = b.Move("BECKON_MOVE", SIM_MOVE(SoulFysh, BeckonMove), StatusIntent(2));
        MoveState deGas = b.Move("DE_GAS_MOVE", SIM_MOVE(SoulFysh, DeGasMove), SingleAttackIntent(DeGasDamage()));
        MoveState gaze = b.Move("GAZE_MOVE", SIM_MOVE(SoulFysh, GazeMove),
                                SingleAttackIntent(GazeDamage()), StatusIntent(1));
        MoveState fade = b.Move("FADE_MOVE", SIM_MOVE(SoulFysh, FadeMove), BuffIntent());
        MoveState scream = b.Move("SCREAM_MOVE", SIM_MOVE(SoulFysh, ScreamMove),
                                  SingleAttackIntent(ScreamDamage()), DebuffIntent());
        beckon.SetFollowUpState(deGas);
        deGas.SetFollowUpState(gaze);
        gaze.SetFollowUpState(fade);
        fade.SetFollowUpState(scream);
        scream.SetFollowUpState(beckon);
        b.Add(beckon);
        b.Add(deGas);
        b.Add(gaze);
        b.Add(fade);
        b.Add(scream);
        return b.Build(beckon);
    }

    void BeckonMove(const CreatureList& targets) const {
        CardPileCmd::AddToCombatAndPreview(CardId::BECKON, targets, PileType::Draw, 1, Player{}, CardPilePosition::Random);
        CardPileCmd::AddToCombatAndPreview(CardId::BECKON, targets, PileType::Discard, 1, Player{});
    }
    void DeGasMove(const CreatureList&) const { DamageCmd::Attack(DeGasDamage()).FromMonster(*this).Execute(nullptr); }
    void GazeMove(const CreatureList& targets) const {
        DamageCmd::Attack(GazeDamage()).FromMonster(*this).Execute(nullptr);
        CardPileCmd::AddToCombatAndPreview(CardId::BECKON, targets, PileType::Discard, 1, Player{});
    }
    void FadeMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::INTANGIBLE_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
    void ScreamMove(const CreatureList& targets) const {
        DamageCmd::Attack(ScreamDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, targets, Dec(3), Creature(), CardModel{});
    }
};

}  // namespace sts2
