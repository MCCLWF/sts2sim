// Models/Monsters/Wriggler.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Wriggler : MonsterModel {
    SIM_MODEL(Wriggler, MonsterModel)

    int32_t BiteDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 7, 6); }
    bool StartStunned() const { return extra(0) != 0; }
    void SetStartStunned(bool value) const { extra(0) = value; }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState bite = b.Move("NASTY_BITE_MOVE", SIM_MOVE(Wriggler, BiteMove), SingleAttackIntent(BiteDamage()));
        MoveState wriggle = b.Move("WRIGGLE_MOVE", SIM_MOVE(Wriggler, WriggleMove), BuffIntent(), StatusIntent(1));
        MoveState spawned = b.Move("SPAWNED_MOVE", SIM_MOVE(Wriggler, SpawnedMove), StunIntent());
        ConditionalBranchState init = b.ConditionalBranch("INIT_MOVE");
        init.AddState(bite, SIM_COND(Wriggler, self.Creature().SlotName() == "wriggler1"));
        init.AddState(wriggle, SIM_COND(Wriggler, self.Creature().SlotName() == "wriggler2"));
        init.AddState(bite, SIM_COND(Wriggler, self.Creature().SlotName() == "wriggler3"));
        init.AddState(wriggle, SIM_COND(Wriggler, self.Creature().SlotName() == "wriggler4"));
        spawned.SetFollowUpState(init);
        bite.SetFollowUpState(wriggle);
        wriggle.SetFollowUpState(bite);
        b.Add(spawned); b.Add(bite); b.Add(wriggle); b.Add(init);
        if (StartStunned()) return b.Build(spawned);
        return b.Build(init);
    }

    void SpawnedMove(const CreatureList&) const {}

    void BiteMove(const CreatureList&) const {
        DamageCmd::Attack(BiteDamage()).FromMonster(*this).Execute(nullptr);
    }

    void WriggleMove(const CreatureList& targets) const {
        CardPileCmd::AddToCombatAndPreview(CardId::INFECTION, targets, PileType::Discard, 1, Player{});
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
};

}  // namespace sts2
