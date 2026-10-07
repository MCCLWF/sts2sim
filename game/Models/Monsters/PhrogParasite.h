// Models/Monsters/PhrogParasite.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/Models/Powers/InfestedPower.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct PhrogParasite : MonsterModel {
    SIM_MODEL(PhrogParasite, MonsterModel)

    int32_t LashDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 5, 4); }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::INFESTED_POWER, ctx, Creature(), Dec(4), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState infect = b.Move("INFECT_MOVE", SIM_MOVE(PhrogParasite, InfectMove), StatusIntent(3));
        MoveState lash = b.Move("LASH_MOVE", SIM_MOVE(PhrogParasite, LashMove), MultiAttackIntent(LashDamage(), 4));
        RandomBranchState random = b.RandomBranch("RAND");
        infect.SetFollowUpState(lash);
        lash.SetFollowUpState(infect);
        random.AddBranch(infect, MoveRepeatType::CannotRepeat);
        random.AddBranch(lash, MoveRepeatType::CannotRepeat);
        b.Add(infect); b.Add(lash); b.Add(random);
        return b.Build(infect);
    }

    void LashMove(const CreatureList&) const {
        DamageCmd::Attack(LashDamage()).WithHitCount(4).FromMonster(*this).Execute(nullptr);
    }

    void InfectMove(const CreatureList& targets) const {
        CardPileCmd::AddToCombatAndPreview(CardId::INFECTION, targets, PileType::Discard, 3, Player{});
    }
};

}  // namespace sts2
