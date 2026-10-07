// Models/Relics/PenNib.cs
#pragma once

#include "game/Models/RelicModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

// Pen Nib doubles every tenth powered attack.  The pending card reference is
// kept in the relic's POD extra slots, matching the saved _attackToDouble
// reference without adding a pointer to simulation state.
struct PenNib : RelicModel {
    SIM_MODEL(PenNib, RelicModel)

    bool ShowCounter() const { return true; }
    // In NonInteractiveMode the source DoActivateVisuals wait completes
    // inline. AttackToDouble is a separate damage reference, not IsActivating.
    int32_t DisplayAmount() const { return extra(0) % 10; }

    Dec ModifyDamageMultiplicative(Creature, Dec, ValueProp props, Creature dealer,
                                   CardModel cardSource, const CardPlay*) const {
        if (!IsPoweredAttack(props) || !cardSource) return Dec(1);
        if (dealer != Owner().Creature()
            && (!dealer.IsMonster() || dealer.Monster().MonsterId_() != MonsterId::OSTY || dealer.PetOwner() != Owner()))
            return Dec(1);
        if (extra(1)) return cardSource.Index() == static_cast<uint32_t>(extra(1)) ? Dec(2) : Dec(1);
        CardPile pile = cardSource.Pile();
        if (extra(0) == 9 && (!pile || pile.Type() != PileType::Play)) return Dec(2);
        return Dec(1);
    }

    void BeforeCardPlayed(const CardPlay* play) const {
        if (!play || play->Card.Type() != CardType::Attack || play->Card.Owner() != Owner()) return;
        extra(0) = (extra(0) + 1) % 10;
        // AttacksPlayed setter -> UpdateDisplay.
        SetStatus(extra(0) == 9 ? RelicStatus::Active : RelicStatus::Normal);
        if (extra(0) == 0) extra(1) = play->Card.Index();
    }

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (extra(1) && play && play->Card.Index() == static_cast<uint32_t>(extra(1))) extra(1) = 0;
    }
};

}  // namespace sts2
