#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct NightmarePower : PowerModel {
    SIM_MODEL(NightmarePower, PowerModel)

    CardModel SelectedCard() const { return CardModel(sim_, static_cast<CardRef>(extra(0))); }

    void SetSelectedCard(CardModel selected) const {
        if (!selected || selected.Owner() != Owner().Player()) throw GameError("NightmarePower: invalid selected card");
        CardModel copy = Owner().CombatState().CloneCard(selected);
        copy.st().clone_of = selected.Index();
        CardCmd::ClearAffliction(copy);
        extra(0) = copy.Index();
    }

    void BeforeHandDraw(Player player, PlayerChoiceContext&, sts2::CombatState combatState) const {
        if (player != Owner().Player()) return;
        CardModel selected = SelectedCard();
        if (!selected) throw GameError("NightmarePower: selected card is missing");
        for (int32_t i = 0; i < Amount(); ++i) {
            CardModel copy = combatState.CloneCard(selected);
            copy.st().clone_of = selected.Index();
            CardPileCmd::AddGeneratedCardToCombat(copy, PileType::Hand, player);
        }
        PowerCmd::Remove(*this);
    }
};

}  // namespace sts2
