// Models/Monsters/GremlinMerc.cs
#pragma once

#include "engine/sim/alloc.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/Models/Powers/SurprisePower.h"
#include "game/Models/Powers/ThieveryPower.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct GremlinMerc : MonsterModel {
    SIM_MODEL(GremlinMerc, MonsterModel)

    int32_t GimmeDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 8, 7); }
    int32_t GimmeRepeat() const { return 2; }
    int32_t DoubleSmashDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 7, 6); }
    int32_t DoubleSmashRepeat() const { return 2; }
    int32_t HeheDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 9, 8); }

    bool HasSpoken() const { return extra(0) != 0; }
    void SetHasSpoken(bool value) const { extra(0) = value; }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::SURPRISE_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
        for (Player player : CombatState().Players()) {
            ThieveryPower thievery(PowerModel(sim_, alloc_power(*sim_->g, PowerId::THIEVERY_POWER)));
            thievery.SetTarget(player.Creature());
            PowerCmd::Apply(ctx, thievery, Creature(), Dec(20), Creature(), CardModel{});
        }
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState gimme = b.Move("GIMME_MOVE", SIM_MOVE(GremlinMerc, GimmeMove), MultiAttackIntent(GimmeDamage(), GimmeRepeat()));
        MoveState doubleSmash = b.Move("DOUBLE_SMASH_MOVE", SIM_MOVE(GremlinMerc, DoubleSmashMove),
                                       MultiAttackIntent(DoubleSmashDamage(), DoubleSmashRepeat()), DebuffIntent());
        MoveState hehe = b.Move("HEHE_MOVE", SIM_MOVE(GremlinMerc, HeheMove), SingleAttackIntent(HeheDamage()), BuffIntent());
        gimme.SetFollowUpState(doubleSmash);
        doubleSmash.SetFollowUpState(hehe);
        hehe.SetFollowUpState(gimme);
        b.Add(gimme);
        b.Add(doubleSmash);
        b.Add(hehe);
        return b.Build(gimme);
    }

    void GimmeMove(const CreatureList&) const {
        SetHasSpoken(true);
        DamageCmd::Attack(GimmeDamage()).WithHitCount(GimmeRepeat()).FromMonster(*this).Execute(nullptr);
        for (PowerModel power : Creature().GetPowerInstances(PowerId::THIEVERY_POWER)) ThieveryPower(power).Steal();
    }

    void DoubleSmashMove(const CreatureList& targets) const {
        DamageCmd::Attack(DoubleSmashDamage()).WithHitCount(DoubleSmashRepeat()).FromMonster(*this).Execute(nullptr);
        for (PowerModel power : Creature().GetPowerInstances(PowerId::THIEVERY_POWER)) ThieveryPower(power).Steal();
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, targets, Dec(2), Creature(), CardModel{});
    }

    void HeheMove(const CreatureList&) const {
        DamageCmd::Attack(HeheDamage()).FromMonster(*this).Execute(nullptr);
        for (PowerModel power : Creature().GetPowerInstances(PowerId::THIEVERY_POWER)) ThieveryPower(power).Steal();
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
};

}  // namespace sts2
