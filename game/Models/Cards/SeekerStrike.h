#pragma once

#include <algorithm>

#include "core/dotnet_sort.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct SeekerStrike : CardModel {
    SIM_MODEL(SeekerStrike, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("SeekerStrike target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);

        CardPile drawPile = Owner().PlayerCombatState().DrawPile();
        CardList candidates = drawPile.Cards();
        dotnet_sort(candidates.span(), [](CardModel a, CardModel b) { return a.CompareTo(b); });
        Owner().RunState().Rng().CombatCardSelection().Shuffle(candidates);

        CardList options;
        int32_t count = std::min(DynamicVars().Cards().IntValue(), static_cast<int32_t>(candidates.size()));
        for (int32_t i = 0; i < count; ++i) options.push_back(candidates[static_cast<size_t>(i)]);
        CardList selected = CardSelectCmd::FromCombatPile(ctx, drawPile, Owner(), CardSelectorPrefs(1), options);
        if (!selected.empty()) CardPileCmd::Add(selected[0], PileType::Hand);
    }
};

}  // namespace sts2
