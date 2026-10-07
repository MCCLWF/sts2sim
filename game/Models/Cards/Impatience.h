// Models/Cards/Impatience.cs
#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Impatience : CardModel {
    SIM_MODEL(Impatience, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        for (CardModel card : Owner().PlayerCombatState().Hand().Cards())
            if (card.Type() == CardType::Attack) return;
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
    }
};
}
