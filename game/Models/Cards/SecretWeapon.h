#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct SecretWeapon : CardModel {
    SIM_MODEL(SecretWeapon, CardModel)

    static bool IsAttack(CardModel card) { return card.Type() == CardType::Attack; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardList cards = CardSelectCmd::FromCombatPile(ctx, Owner().PlayerCombatState().DrawPile(), Owner(),
                                                        CardSelectorPrefs(1), IsAttack);
        if (!cards.empty()) CardPileCmd::Add(cards.front(), PileType::Hand);
    }
};

}  // namespace sts2
