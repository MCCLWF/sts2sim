#pragma once
#include "game/Models/PowerModel.h"
namespace sts2 {
struct EchoFormPower : PowerModel { SIM_MODEL(EchoFormPower, PowerModel)
    // Echo Form's remaining VFX toggles are presentation-only.
    void AfterApplied(Creature, CardModel) const {}
    void AfterRemoved(Creature) const {}
    void AfterModifyingCardPlayCount(CardModel) const {}
    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay*) const {}
    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList&, sts2::CombatState) const {}
    int32_t ModifyCardPlayCount(CardModel card, Creature, int32_t playCount) const {
        if (card.Owner().Creature() != Owner()) return playCount;
        if (Owner().IsPlayer() && Owner().CombatState().st().player_cards_started_this_turn < Amount()) return playCount + 1;
        return playCount;
    }
};
}
