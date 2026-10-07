// Shared combat behavior from Models/Monsters/DecimillipedeSegment.cs.
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/Models/Powers/ReattachPower.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct DecimillipedeSegmentLogic : MonsterModel {
    SIM_MODEL(DecimillipedeSegmentLogic, MonsterModel)

    int32_t StarterMoveIdx() const { return extra(0); }
    void SetStarterMoveIdx(int32_t value) const { extra(0) = value; }
    int32_t WritheDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 6, 5); }
    int32_t BulkDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 7, 6); }
    int32_t ConstrictDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 9, 8); }

    void AfterAddedToRoom() const {
        int32_t maxHp = Creature().MaxHp();
        if (maxHp % 2 == 1) ++maxHp;
        const auto teammates = CombatState().GetTeammatesOf(Creature());
        for (;;) {
            bool duplicate = false;
            for (auto teammate : teammates) {
                if (teammate != Creature() && teammate.MaxHp() == maxHp) { duplicate = true; break; }
            }
            if (!duplicate) break;
            maxHp += 2;
            if (maxHp > MaxInitialHp()) maxHp = MinInitialHp();
        }
        CreatureCmd::SetMaxAndCurrentHp(Creature(), Dec(maxHp));
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::REATTACH_POWER, ctx, Creature(), Dec(25), Creature(), CardModel{});
    }
    void WritheMove(const CreatureList&) const {
        DamageCmd::Attack(WritheDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr);
    }
    void BulkMove(const CreatureList&) const {
        DamageCmd::Attack(BulkDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
    void ConstrictMove(const CreatureList& targets) const {
        DamageCmd::Attack(ConstrictDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, targets, Dec(1), Creature(), CardModel{});
    }
    void DeadMove(const CreatureList&) const {}
    void ReattachMove(const CreatureList&) const {
        ReattachPower(Creature().GetPower(PowerId::REATTACH_POWER)).DoReattach();
    }
    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState writhe = b.Move("WRITHE_MOVE", SIM_MOVE(DecimillipedeSegmentLogic, WritheMove), MultiAttackIntent(WritheDamage(), 2));
        MoveState bulk = b.Move("BULK_MOVE", SIM_MOVE(DecimillipedeSegmentLogic, BulkMove), SingleAttackIntent(BulkDamage()), BuffIntent());
        MoveState constrict = b.Move("CONSTRICT_MOVE", SIM_MOVE(DecimillipedeSegmentLogic, ConstrictMove), SingleAttackIntent(ConstrictDamage()), DebuffIntent());
        MoveState dead = b.Move("DEAD_MOVE", SIM_MOVE(DecimillipedeSegmentLogic, DeadMove));
        MoveState reattach = b.Move("REATTACH_MOVE", SIM_MOVE(DecimillipedeSegmentLogic, ReattachMove), HealIntent());
        reattach.SetMustPerformOnceBeforeTransitioning(true);
        RandomBranchState random = b.RandomBranch("RAND");
        constrict.SetFollowUpState(bulk);
        bulk.SetFollowUpState(writhe);
        writhe.SetFollowUpState(constrict);
        dead.SetFollowUpState(reattach);
        reattach.SetFollowUpState(random);
        random.AddBranch(writhe, MoveRepeatType::CannotRepeat);
        random.AddBranch(bulk, MoveRepeatType::CannotRepeat);
        random.AddBranch(constrict, MoveRepeatType::CannotRepeat);
        b.Add(writhe);
        b.Add(bulk);
        b.Add(constrict);
        b.Add(dead);
        b.Add(reattach);
        b.Add(random);
        switch (StarterMoveIdx() % 3) {
        case 0: return b.Build(writhe);
        case 1: return b.Build(bulk);
        default: return b.Build(constrict);
        }
    }
};

}  // namespace sts2
