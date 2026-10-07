// Models/Monsters/GasBomb.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct GasBomb : MonsterModel {
    SIM_MODEL(GasBomb, MonsterModel)

    int32_t ExplodeDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 9, 8); }
    bool HasExploded() const { return extra(0) != 0; }
    void SetHasExploded(bool value) const { extra(0) = value; }
    bool ShouldFadeAfterDeath() const { return false; }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::MINION_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
    }

    static Dec ExplodeDamageIntent(MonsterModel self) { return Dec(GasBomb(self).ExplodeDamage()); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState explode = b.Move("EXPLODE_MOVE", SIM_MOVE(GasBomb, ExplodeMove), DeathBlowIntent(SIM_INTENT_DAMAGE(GasBomb, self.ExplodeDamage())));
        b.Add(explode);
        return b.Build(explode);
    }

    void ExplodeMove(const CreatureList&) const {
        SetHasExploded(true);
        DamageCmd::Attack(ExplodeDamage()).FromMonster(*this).Execute(nullptr);
        CreatureCmd::Kill(Creature());
    }
};

}  // namespace sts2
