#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct PhantomBladesPower : PowerModel {
    SIM_MODEL(PhantomBladesPower, PowerModel)

    void AfterCardEnteredCombat(CardModel card) const {
        if (!card.HasTag(CardTag::Shiv) || card.Owner() != Owner().Player()) return;
        CardCmd::ApplyKeyword(card, {CardKeyword::Retain});
    }

    void AfterApplied(Creature, CardModel) const {
        for (CardModel card : Owner().Player().PlayerCombatState().AllCards()) {
            if (card.HasTag(CardTag::Shiv)) CardCmd::ApplyKeyword(card, {CardKeyword::Retain});
        }
    }

    Dec ModifyDamageAdditive(Creature, Dec, ValueProp props, Creature dealer, CardModel card,
                             const CardPlay*) const {
        if (!IsPoweredAttack(props) || !card || !card.HasTag(CardTag::Shiv) || dealer != Owner()) return Dec(0);
        return Owner().CombatState().st().player_shiv_cards_finished_this_turn == 0 ? Dec(Amount()) : Dec(0);
    }
};

}  // namespace sts2
