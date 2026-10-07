// Models/Relics/PaelsLegion.cs (v0.111.0).
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Entities/Cards/CardPlay.h"
#include "game/Models/MonsterModel.h"
#include "game/Models/RelicModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct PaelsLegionRelic : RelicModel {
    SIM_MODEL(PaelsLegionRelic, RelicModel)

    // The original stores a CardPlay reference until AfterCardPlayed. Within
    // a resumed action the card index and series index identify that play.
    int32_t Cooldown() const { return extra(0); }
    void SetCooldown(int32_t value) const { extra(0) = value; }
    bool AddsPet() const { return true; }
    bool ShowCounter() const { return DisplayAmount() > 0; }
    int32_t DisplayAmount() const {
        return CombatManager(sim_).IsInProgress() && Cooldown() > 0 ? Cooldown() : -1;
    }

    void SummonPet() const {
        CombatState combat = Owner().Creature().CombatState();
        MonsterModel monster = MonsterModel::ToMutable(sim_, MonsterId::PAELS_LEGION);
        Creature pet = combat.CreateCreature(monster, CombatSide::Player, -1);
        PlayerCmd::AddPet(pet, Owner());
    }
    void AfterObtained() const {
        // Skin uses a separate presentation RNG seeded from owner/id, and has
        // no effect on the run RNG, decisions or creature state.
        if (CombatManager(sim_).IsInProgress()) SummonPet();
    }
    void BeforeCombatStart() const { SummonPet(); }

    Dec ModifyBlockMultiplicative(Creature, Dec, ValueProp props, CardModel cardSource,
                                   const CardPlay*) const {
        return IsCardOrMonsterMove(props) && cardSource && cardSource.Owner() == Owner() && Cooldown() <= 0
            ? Dec(2) : Dec(1);
    }
    void AfterModifyingBlockAmount(Dec amount, CardModel, const CardPlay* play) const {
        if (amount <= Dec(0) || !play) return;
        if (extra(1) != 0 &&
            (extra(1) != play->Card.Index() || extra(2) != play->PlayIndex + 1)) return;
        extra(1) = play->Card.Index();
        extra(2) = play->PlayIndex + 1;
    }
    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || extra(1) == 0 || extra(1) != play->Card.Index() ||
            extra(2) != play->PlayIndex + 1) return;
        Flash();
        extra(1) = 0;
        extra(2) = 0;
        SetCooldown(DynamicVars()["Turns"].IntValue());
        SetStatus(RelicStatus::Normal);
        extra(3) = 1;  // TriggeredBlockLastTurn; animation is presentation only.
    }
    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (!participants.contains(Owner().Creature())) return;
        SetCooldown(Cooldown() - 1);
        if (Cooldown() <= 0) SetStatus(RelicStatus::Active);
        extra(3) = 0;
    }
    void AfterCombatEnd(CombatRoom) const {
        SetStatus(RelicStatus::Normal);
        SetCooldown(0);
        extra(1) = extra(2) = extra(3) = 0;
    }
};

}  // namespace sts2
