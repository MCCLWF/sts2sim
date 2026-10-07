// Models/Monsters/CorpseSlug.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct CorpseSlug : MonsterModel {
    SIM_MODEL(CorpseSlug, MonsterModel)

    int32_t WhipSlapDamage() const { return 3; }
    int32_t WhipSlapRepeat() const { return 2; }
    int32_t GlompDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 9, 8); }
    int32_t GoopFrailAmt() const { return 2; }
    int32_t RavenousStr() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 5, 4); }

    bool IsRavenous() const { return extra(0) != 0; }  // _isRavenous
    void SetIsRavenous(bool value) const { extra(0) = value; }
    int32_t StarterMoveIdx() const { return extra(1); }  // _starterMoveIdx
    void SetStarterMoveIdx(int32_t value) const { extra(1) = value; }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::RAVENOUS_POWER, ctx, Creature(), Dec(RavenousStr()), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState moveState = b.Move("WHIP_SLAP_MOVE", SIM_MOVE(CorpseSlug, WhipSlapMove), MultiAttackIntent(WhipSlapDamage(), WhipSlapRepeat()));
        MoveState moveState2 = b.Move("GLOMP_MOVE", SIM_MOVE(CorpseSlug, GlompMove), SingleAttackIntent(GlompDamage()));
        MoveState moveState3 = b.Move("GOOP_MOVE", SIM_MOVE(CorpseSlug, GoopMove), DebuffIntent());
        moveState.SetFollowUpState(moveState2);
        moveState2.SetFollowUpState(moveState3);
        moveState3.SetFollowUpState(moveState);
        b.Add(moveState);
        b.Add(moveState2);
        b.Add(moveState3);
        switch (StarterMoveIdx() % 3) {
        case 0: return b.Build(moveState);
        case 1: return b.Build(moveState2);
        default: return b.Build(moveState3);
        }
    }

    void WhipSlapMove(const CreatureList& targets) const {
        DamageCmd::Attack(WhipSlapDamage()).WithHitCount(WhipSlapRepeat()).FromMonster(*this).Execute(nullptr);
    }
    void GlompMove(const CreatureList& targets) const { DamageCmd::Attack(GlompDamage()).FromMonster(*this).Execute(nullptr); }
    void GoopMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, targets, Dec(GoopFrailAmt()), Creature(), CardModel{});
    }

    // EnsureCorpseSlugsStartWithDifferentMoves(monsters.OfType<CorpseSlug>(), rng)
    template <class List>
    static void EnsureCorpseSlugsStartWithDifferentMoves(const List& monsters, sts2::Rng& rng) {
        int32_t num = rng.NextInt(3);
        for (MonsterModel m : monsters) {
            if (m.MonsterId_() != MonsterId::CORPSE_SLUG) continue;
            CorpseSlug(m).SetStarterMoveIdx(num % 3);
            ++num;
        }
    }
};

}  // namespace sts2
