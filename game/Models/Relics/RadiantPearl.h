// Models/Relics/RadiantPearl.cs (v0.111.0).
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct RadiantPearl : RelicModel {
    SIM_MODEL(RadiantPearl, RelicModel)

    void BeforeHandDraw(Player player, PlayerChoiceContext&, CombatState) const {
        if (player != Owner() || Owner().PlayerCombatState().TurnNumber() != 1) return;
        CardModel luminesce = Owner().Creature().CombatState().CreateCard(CardId::LUMINESCE, Owner());
        CardPileCmd::AddGeneratedCardToCombat(luminesce, PileType::Hand, Owner());
    }
};

}  // namespace sts2
