#pragma once

#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Tutor : CardModel {
    SIM_MODEL(Tutor, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        Player target = play->Target.Player();
        if (!target) throw GameError("Tutor target has no player");
        CardList selection = CardSelectCmd::FromCombatPile(ctx, CardPile::Get(PileType::Draw, target), target, CardSelectorPrefs(1));
        if (!selection.empty()) CardPileCmd::Add(selection[0], PileType::Hand);
    }
};
}  // namespace sts2
