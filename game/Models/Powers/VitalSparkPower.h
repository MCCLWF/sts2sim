// Models/Powers/VitalSparkPower.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct VitalSparkPower : PowerModel {
    SIM_MODEL(VitalSparkPower, PowerModel)

    void AfflictSkills() const {
        for (Player player : Owner().CombatState().Players()) {
            for (CardModel card : player.PlayerCombatState().AllCards()) {
                if (card.Type() == CardType::Skill) CardCmd::Afflict(AfflictionId::TAINTED, card, Dec(Amount()));
            }
        }
    }

    void BeforeCombatStart() const { AfflictSkills(); }

    void AfterCardEnteredCombat(CardModel card) const {
        if (card.Affliction().IsNull() && card.Type() == CardType::Skill) {
            CardCmd::Afflict(AfflictionId::TAINTED, card, Dec(Amount()));
        }
    }

    void AfterCardPlayed(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || play->Card.Affliction().IsNull() || play->Card.Affliction().AfflictionId_() != AfflictionId::TAINTED) return;
        PowerCmd::Apply(PowerId::TAINTED_POWER, ctx, play->Card.Owner().Creature(), Dec(Amount()), sts2::Creature{}, CardModel{});
    }

    void AfterRemoved(Creature oldOwner) const {
        sts2::CombatState combat = oldOwner.CombatState();
        if (!combat) return;
        for (Player player : combat.Players()) {
            for (CardModel card : player.PlayerCombatState().AllCards()) {
                if (!card.Affliction().IsNull() && card.Affliction().AfflictionId_() == AfflictionId::TAINTED) CardCmd::ClearAffliction(card);
            }
        }
    }

    void AfterPowerAmountChanged(PlayerChoiceContext&, PowerModel power, Dec, Creature, CardModel) const {
        if (power != *this) return;
        for (Player player : Owner().CombatState().Players()) {
            for (CardModel card : player.PlayerCombatState().AllCards()) {
                if (!card.Affliction().IsNull() && card.Affliction().AfflictionId_() == AfflictionId::TAINTED) {
                    card.Affliction().SetAmount(Amount());
                }
            }
        }
    }
};

}  // namespace sts2
