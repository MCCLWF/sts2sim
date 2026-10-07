#pragma once
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Scrape : CardModel { SIM_MODEL(Scrape, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        CardList drawn = CardPileCmd::Draw(ctx, DynamicVars().Cards().IntValue(), Owner());
        CardList discard;
        for (CardModel card : drawn)
            if (card.EnergyCost().CostsX() || card.EnergyCost().GetWithModifiers(CostModifiers::All) != 0) discard.push_back(card);
        CardCmd::Discard(ctx, discard);
    }
};
}
