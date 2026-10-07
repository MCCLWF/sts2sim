// Models/Monsters/TwoTailedRat.cs
#pragma once

#include <algorithm>

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/EncounterModel.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct TwoTailedRat : MonsterModel {
    SIM_MODEL(TwoTailedRat, MonsterModel)

    int32_t StarterMoveIndex() const { return extra(0) - 1; }
    void SetStarterMoveIndex(int32_t value) const { extra(0) = value + 1; }
    int32_t TurnsUntilSummonable() const { return extra(3) == 0 ? 2 : extra(1); }
    void SetTurnsUntilSummonable(int32_t value) const { extra(3) = 1; extra(1) = value; }
    int32_t CallForBackupCount() const { return extra(2); }
    void SetCallForBackupCount(int32_t value) const { extra(2) = value; }

    int32_t ScratchDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 9, 8); }
    int32_t DiseaseBiteDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 7, 6); }

    void AfterAddedToRoom() const {}

    bool CanSummon() const {
        if (TurnsUntilSummonable() > 0 || CallForBackupCount() >= 3) return false;
        EncounterModel encounter(sim_);
        if (encounter.GetNextSlot(CombatState()).empty()) return false;
        for (sts2::Creature teammate : CombatState().GetTeammatesOf(Creature())) {
            if (teammate != Creature() && teammate.Monster().NextMove().Id() == "CALL_FOR_BACKUP_MOVE") return false;
        }
        return true;
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState scratch = b.Move("SCRATCH_MOVE", SIM_MOVE(TwoTailedRat, ScratchMove), SingleAttackIntent(ScratchDamage()));
        MoveState disease = b.Move("DISEASE_BITE_MOVE", SIM_MOVE(TwoTailedRat, DiseaseBiteMove), SingleAttackIntent(DiseaseBiteDamage()));
        MoveState screech = b.Move("SCREECH_MOVE", SIM_MOVE(TwoTailedRat, ScreechMove), DebuffIntent());
        MoveState call = b.Move("CALL_FOR_BACKUP_MOVE", SIM_MOVE(TwoTailedRat, CallForBackup), SummonIntent());
        RandomBranchState random = b.RandomBranch("RAND");
        random.AddBranch(scratch, MoveRepeatType::CannotRepeat,
                         SIM_WEIGHT(TwoTailedRat, self.CanSummon() ? (1.0f / 12.0f) : 1.0f));
        random.AddBranch(disease, MoveRepeatType::CannotRepeat,
                         SIM_WEIGHT(TwoTailedRat, self.CanSummon() ? (1.0f / 12.0f) : 1.0f));
        random.AddBranch(screech, 3, MoveRepeatType::CannotRepeat,
                         SIM_WEIGHT(TwoTailedRat, self.CanSummon() ? (1.0f / 12.0f) : 1.0f));
        random.AddBranch(call, MoveRepeatType::UseOnlyOnce,
                         SIM_WEIGHT(TwoTailedRat, self.CanSummon() ? 0.75f : 0.0f));
        scratch.SetFollowUpState(random);
        disease.SetFollowUpState(random);
        screech.SetFollowUpState(random);
        call.SetFollowUpState(random);
        b.Add(random); b.Add(scratch); b.Add(disease); b.Add(screech); b.Add(call);
        if (StarterMoveIndex() == -1) return b.Build(random);
        switch (StarterMoveIndex() % 3) {
        case 0: return b.Build(scratch);
        case 1: return b.Build(disease);
        default: return b.Build(screech);
        }
    }

    void ScratchMove(const CreatureList&) const {
        SetTurnsUntilSummonable(TurnsUntilSummonable() - 1);
        DamageCmd::Attack(ScratchDamage()).FromMonster(*this).Execute(nullptr);
    }

    void DiseaseBiteMove(const CreatureList&) const {
        SetTurnsUntilSummonable(TurnsUntilSummonable() - 1);
        DamageCmd::Attack(DiseaseBiteDamage()).FromMonster(*this).Execute(nullptr);
    }

    void ScreechMove(const CreatureList& targets) const {
        SetTurnsUntilSummonable(TurnsUntilSummonable() - 1);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, targets, Dec(1), Creature(), CardModel{});
    }

    void CallForBackup(const CreatureList&) const {
        sts2::CombatState combat = CombatState();
        std::string_view nextSlot;
        EncounterModel encounter(sim_);
        for (std::string_view slot : encounter.Slots()) {
            bool free = true;
            for (sts2::Creature enemy : combat.Enemies()) free &= enemy.SlotName() != slot;
            if (free) nextSlot = slot;
        }
        if (!nextSlot.empty()) {
            CreatureCmd::Add(MonsterModel::ToMutable(sim_, MonsterId::TWO_TAILED_RAT), combat,
                             CombatSide::Enemy, nextSlot);
        }
        int32_t maxCount = 0;
        for (sts2::Creature enemy : combat.Enemies()) {
            if (enemy.Monster().MonsterId_() == MonsterId::TWO_TAILED_RAT) {
                maxCount = std::max(maxCount, TwoTailedRat(enemy.Monster()).CallForBackupCount() + 1);
            }
        }
        for (sts2::Creature enemy : combat.Enemies()) {
            if (enemy.Monster().MonsterId_() == MonsterId::TWO_TAILED_RAT) {
                TwoTailedRat(enemy.Monster()).SetCallForBackupCount(maxCount);
            }
        }
    }
};

}  // namespace sts2
