// Models/Monsters/KinPriest.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct KinPriest : MonsterModel {
    SIM_MODEL(KinPriest, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);

    int32_t OrbOfFrailtyDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 9, 8); }
    int32_t OrbOfWeaknessDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 9, 8); }
    int32_t BeamDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 3, 3); }
    int32_t RitualStrength() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 3, 2); }

    void AfterDeath(PlayerChoiceContext&, sts2::Creature, bool, float) const {}  // source hook is dialogue/music only

    void OrbOfFrailtyMove(const CreatureList& targets) const {
        DamageCmd::Attack(OrbOfFrailtyDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, targets, Dec(1), Creature(), CardModel{});
    }
    void OrbOfWeaknessMove(const CreatureList& targets) const {
        DamageCmd::Attack(OrbOfWeaknessDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, targets, Dec(1), Creature(), CardModel{});
    }
    void BeamMove(const CreatureList&) const { DamageCmd::Attack(BeamDamage()).WithHitCount(3).FromMonster(*this).Execute(nullptr); }
    void RitualMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(RitualStrength()), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState frailty = b.Move("ORB_OF_FRAILTY_MOVE", SIM_MOVE(KinPriest, OrbOfFrailtyMove),
                                   SingleAttackIntent(OrbOfFrailtyDamage()), DebuffIntent());
        MoveState weakness = b.Move("ORB_OF_WEAKNESS_MOVE", SIM_MOVE(KinPriest, OrbOfWeaknessMove),
                                    SingleAttackIntent(OrbOfWeaknessDamage()), DebuffIntent());
        MoveState beam = b.Move("BEAM_MOVE", SIM_MOVE(KinPriest, BeamMove), MultiAttackIntent(BeamDamage(), 3));
        MoveState ritual = b.Move("RITUAL_MOVE", SIM_MOVE(KinPriest, RitualMove), BuffIntent());
        frailty.SetFollowUpState(weakness);
        weakness.SetFollowUpState(beam);
        beam.SetFollowUpState(ritual);
        ritual.SetFollowUpState(frailty);
        b.Add(frailty);
        b.Add(weakness);
        b.Add(beam);
        b.Add(ritual);
        return b.Build(frailty);
    }
};

}  // namespace sts2
