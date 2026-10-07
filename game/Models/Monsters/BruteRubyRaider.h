// Models/Monsters/BruteRubyRaider.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct BruteRubyRaider : MonsterModel {
    SIM_MODEL(BruteRubyRaider, MonsterModel)

    int32_t BeatDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 8, 7); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState beat = b.Move("BEAT_MOVE", SIM_MOVE(BruteRubyRaider, BeatMove), SingleAttackIntent(BeatDamage()));
        MoveState roar = b.Move("ROAR_MOVE", SIM_MOVE(BruteRubyRaider, RoarMove), BuffIntent());
        beat.SetFollowUpState(roar);
        roar.SetFollowUpState(beat);
        b.Add(beat);
        b.Add(roar);
        return b.Build(beat);
    }

    void BeatMove(const CreatureList& targets) const { DamageCmd::Attack(BeatDamage()).FromMonster(*this).Execute(nullptr); }
    void RoarMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(3), Creature(), CardModel{});
    }
};

}  // namespace sts2
