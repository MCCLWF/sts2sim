#pragma once
#include <algorithm>
#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"
namespace sts2 {
struct PowerCell : RelicModel { SIM_MODEL(PowerCell, RelicModel)
    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants, sts2::CombatState) const {
        if (!participants.contains(Owner().Creature()) || Owner().PlayerCombatState().TurnNumber() > 1) return;
        CardList eligible;
        for (CardModel card : Owner().PlayerCombatState().DrawPile().Cards())
            if (!card.EnergyCost().CostsX() && card.EnergyCost().GetWithModifiers(CostModifiers::Local) == 0) eligible.push_back(card);
        CardPileCmd::StableShuffle(eligible, Owner().RunState().Rng().CombatCardSelection());
        CardList selected;
        int32_t count = std::min<int32_t>(DynamicVars().Cards().IntValue(), static_cast<int32_t>(eligible.size()));
        for (int32_t i = 0; i < count; ++i) selected.push_back(eligible[static_cast<size_t>(i)]);
        CardPileCmd::Add(selected, PileType::Hand);
    }
};
}
