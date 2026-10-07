#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct BowlbugRock : MonsterModel {
    SIM_MODEL(BowlbugRock, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(GenerateBestiaryMoveList);
    SIM_NOT_PORTED(SetupSkins);
    SIM_NOT_PORTED(get_AttackSfx);
    SIM_NOT_PORTED(get_DeathSfx);
    SIM_NOT_PORTED(get_MinInitialHp);
    SIM_NOT_PORTED(get_MaxInitialHp);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    bool IsOffBalance() const { return extra(0) != 0; }
    void SetIsOffBalance(bool value) const { extra(0) = value; }
    int32_t HeadbuttDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 16, 15); }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::IMBALANCED_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState headbutt = b.Move("HEADBUTT_MOVE", SIM_MOVE(BowlbugRock, HeadbuttMove), SingleAttackIntent(HeadbuttDamage()));
        MoveState dizzy = b.Move("DIZZY_MOVE", SIM_MOVE(BowlbugRock, DizzyMove), StunIntent());
        ConditionalBranchState postHeadbutt = b.ConditionalBranch("POST_HEADBUTT");
        postHeadbutt.AddState(dizzy, SIM_COND(BowlbugRock, self.IsOffBalance()));
        postHeadbutt.AddState(headbutt, SIM_COND(BowlbugRock, !self.IsOffBalance()));
        headbutt.SetFollowUpState(postHeadbutt);
        dizzy.SetFollowUpState(headbutt);
        b.Add(dizzy);
        b.Add(postHeadbutt);
        b.Add(headbutt);
        return b.Build(headbutt);
    }

    void HeadbuttMove(const CreatureList&) const {
        DamageCmd::Attack(HeadbuttDamage()).FromMonster(*this).Execute(nullptr);
        if (IsOffBalance()) CreatureCmd::Stun(Creature(), SIM_MOVE(BowlbugRock, DizzyMove));
    }
    void DizzyMove(const CreatureList&) const { SetIsOffBalance(false); }
};

}  // namespace sts2
