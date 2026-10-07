// Models/Enchantments/Imbued.cs (v0.111.0).
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/EnchantmentModel.h"
#include "game/Models/Events/AllowedHelpers.h"

namespace sts2 {

struct Imbued : EnchantmentModel {
    SIM_MODEL(Imbued, EnchantmentModel)

    static bool CanEnchant(CardModel card) {
        return event_allowed::BaseCanEnchant(card) && card.Type() == CardType::Skill;
    }
    bool ShouldStartAtBottomOfDrawPile() const { return true; }
    bool ShowAmount() const { return false; }

    void AfterAutoPrePlayPhaseEntered(PlayerChoiceContext& context, Player player) const {
        CardModel card = Card();
        if (card && player == card.Owner() && player.PlayerCombatState().TurnNumber() <= 1)
            CardCmd::AutoPlay(context, card, Creature{});
    }
};

}  // namespace sts2
