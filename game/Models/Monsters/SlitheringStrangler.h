// Models/Monsters/SlitheringStrangler.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct SlitheringStrangler : MonsterModel {
    SIM_MODEL(SlitheringStrangler, MonsterModel)

    int32_t ThwackDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 8, 7); }
    int32_t LashDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 13, 12); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        RandomBranchState random = b.RandomBranch("rand");
        MoveState constrict = b.Move("CONSTRICT", SIM_MOVE(SlitheringStrangler, ConstrictMove), DebuffIntent());
        MoveState thwack = b.Move("THWACK", SIM_MOVE(SlitheringStrangler, ThwackMove), SingleAttackIntent(ThwackDamage()), DefendIntent());
        MoveState lash = b.Move("LASH", SIM_MOVE(SlitheringStrangler, LashMove), SingleAttackIntent(LashDamage()));
        constrict.SetFollowUpState(random);
        thwack.SetFollowUpState(constrict);
        lash.SetFollowUpState(constrict);
        random.AddBranch(thwack, MoveRepeatType::CanRepeatForever);
        random.AddBranch(lash, MoveRepeatType::CanRepeatForever);
        b.Add(random);
        b.Add(thwack);
        b.Add(constrict);
        b.Add(lash);
        return b.Build(constrict);
    }

    void ConstrictMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::CONSTRICT_POWER, ctx, targets, Dec(3), Creature(), CardModel{});
    }
    void ThwackMove(const CreatureList& targets) const {
        DamageCmd::Attack(ThwackDamage()).FromMonster(*this).Execute(nullptr);
        CreatureCmd::GainBlock(Creature(), Dec(5), ValueProp::Move, nullptr);
    }
    void LashMove(const CreatureList& targets) const { DamageCmd::Attack(LashDamage()).FromMonster(*this).Execute(nullptr); }
};

}  // namespace sts2
