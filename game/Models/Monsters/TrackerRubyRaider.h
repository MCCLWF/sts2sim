// Models/Monsters/TrackerRubyRaider.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct TrackerRubyRaider : MonsterModel {
    SIM_MODEL(TrackerRubyRaider, MonsterModel)

    int32_t HoundsDamage() const { return 1; }
    int32_t HoundsRepeat() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 9, 8); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState track = b.Move("TRACK_MOVE", SIM_MOVE(TrackerRubyRaider, TrackMove), DebuffIntent());
        MoveState hounds = b.Move("HOUNDS_MOVE", SIM_MOVE(TrackerRubyRaider, HoundsMove), MultiAttackIntent(HoundsDamage(), HoundsRepeat()));
        track.SetFollowUpState(hounds);
        hounds.SetFollowUpState(hounds);
        b.Add(track);
        b.Add(hounds);
        return b.Build(track);
    }

    void TrackMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, targets, Dec(2), Creature(), CardModel{});
    }
    void HoundsMove(const CreatureList& targets) const {
        DamageCmd::Attack(HoundsDamage()).WithHitCount(HoundsRepeat()).FromMonster(*this).Execute(nullptr);
    }
};

}  // namespace sts2
