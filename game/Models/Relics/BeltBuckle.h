// Models/Relics/BeltBuckle.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct BeltBuckle : RelicModel {
    SIM_MODEL(BeltBuckle, RelicModel)
    bool DexterityApplied() const { return extra(0) != 0; }
    void RefreshStatus() const {
        SetStatus(CombatManager(sim_).IsInProgress() && Owner().Potions().empty()
                      ? RelicStatus::Active : RelicStatus::Normal);
    }
    void ApplyDexterity() const {
        if (DexterityApplied()) return;
        extra(0) = 1;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, ctx, Owner().Creature(), DynamicVars().Dexterity().BaseValue(),
                        Creature{}, CardModel{});
    }
    void RemoveDexterity() const {
        if (!DexterityApplied()) return;
        extra(0) = 0;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, ctx, Owner().Creature(), -DynamicVars().Dexterity().BaseValue(),
                        Creature{}, CardModel{});
    }
    void AfterObtained() const {
        if (CombatManager(sim_).IsInProgress() && Owner().Potions().empty()) ApplyDexterity();
    }
    void BeforeCombatStart() const {
        extra(0) = 0;
        RefreshStatus();
        if (Owner().Potions().empty()) ApplyDexterity();
    }
    void AfterCombatEnd(CombatRoom) const { RefreshStatus(); }
    void AfterPotionProcured(PotionModel) const {
        RefreshStatus();
        if (CombatManager(sim_).IsInProgress() && !Owner().Potions().empty()) RemoveDexterity();
    }
    void AfterPotionDiscarded(PotionModel) const {
        RefreshStatus();
        if (CombatManager(sim_).IsInProgress() && Owner().Potions().empty()) ApplyDexterity();
    }
    void AfterPotionUsed(PotionModel, Creature) const {
        RefreshStatus();
        if (CombatManager(sim_).IsInProgress() && Owner().Potions().empty()) ApplyDexterity();
    }
    void AfterCombatVictory(CombatRoom) const {
        extra(0) = 0;
        RefreshStatus();
    }
};

}  // namespace sts2
