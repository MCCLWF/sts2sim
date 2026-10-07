// Models/Cards/NeowsFury.cs
#pragma once
#include "game/Models/CardModel.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
namespace sts2 {
struct NeowsFury : CardModel {
    SIM_MODEL(NeowsFury, CardModel)
    bool CanBeGeneratedInCombat() const { return false; }
    void OnPlay(PlayerChoiceContext& context, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(context);
        int32_t n = std::min(DynamicVars().Cards().IntValue(), CardPile::MaxCardsInHand - Owner().PlayerCombatState().Hand().Count());
        if (n > 0) CardPileCmd::Add(CardSelectCmd::FromCombatPile(context, Owner().PlayerCombatState().DiscardPile(), Owner(), CardSelectorPrefs(0, n)), PileType::Hand);
    }
};
}
