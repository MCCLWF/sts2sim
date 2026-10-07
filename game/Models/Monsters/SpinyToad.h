// Models/Monsters/SpinyToad.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct SpinyToad : MonsterModel {
    SIM_MODEL(SpinyToad, MonsterModel)

    bool IsSpiny() const { return extra(0) != 0; }
    void SetIsSpiny(bool value) const { extra(0) = value; }
    int32_t LashDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 19, 17); }
    int32_t ExplosionDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 25, 23); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState spikes = b.Move("PROTRUDING_SPIKES_MOVE", SIM_MOVE(SpinyToad, SpikesMove), BuffIntent());
        MoveState explosion = b.Move("SPIKE_EXPLOSION_MOVE", SIM_MOVE(SpinyToad, ExplosionMove), SingleAttackIntent(ExplosionDamage()));
        MoveState lash = b.Move("TONGUE_LASH_MOVE", SIM_MOVE(SpinyToad, LashMove), SingleAttackIntent(LashDamage()));
        spikes.SetFollowUpState(explosion);
        explosion.SetFollowUpState(lash);
        lash.SetFollowUpState(spikes);
        b.Add(spikes);
        b.Add(explosion);
        b.Add(lash);
        return b.Build(spikes);
    }

    void SpikesMove(const CreatureList&) const {
        SetIsSpiny(true);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::THORNS_POWER, ctx, Creature(), Dec(5), Creature(), CardModel{});
    }

    void ExplosionMove(const CreatureList&) const {
        SetIsSpiny(false);
        DamageCmd::Attack(ExplosionDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::THORNS_POWER, ctx, Creature(), Dec(-5), Creature(), CardModel{});
    }

    void LashMove(const CreatureList&) const {
        DamageCmd::Attack(LashDamage()).FromMonster(*this).Execute(nullptr);
    }
};

}  // namespace sts2
