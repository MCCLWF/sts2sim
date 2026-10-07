// Models/Cards/Uproar.cs
#pragma once
#include "core/dotnet_sort.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Uproar : CardModel { SIM_MODEL(Uproar, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(2)
            .FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        CardList candidates;
        for (CardModel card : CardPile::Get(PileType::Draw, Owner()).Cards())
            if (card.Type() == CardType::Attack && !card.Keywords().Contains(CardKeyword::Unplayable))
                candidates.push_back(card);
        if (candidates.empty()) {
            for (CardModel card : CardPile::Get(PileType::Draw, Owner()).Cards())
                if (card.Type() == CardType::Attack) candidates.push_back(card);
        }
        if (candidates.empty()) return;
        dotnet_sort(candidates.span(), [](CardModel a, CardModel b) { return a.CompareTo(b); });
        Owner().RunState().Rng().Shuffle().Shuffle(candidates);
        CardCmd::AutoPlay(ctx, candidates[0], Creature{});
    }
};
}
