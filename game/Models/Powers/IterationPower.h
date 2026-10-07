#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct IterationPower : PowerModel { SIM_MODEL(IterationPower, PowerModel)
    void AfterCardDrawn(PlayerChoiceContext& ctx, CardModel card, bool) const {
        if (card.Owner().Creature() != Owner() || card.Type() != CardType::Status) return;
        if (Owner().IsPlayer() && Owner().CombatState().st().player_status_cards_drawn_this_turn <= 1)
            CardPileCmd::Draw(ctx, Dec(Amount()), Owner().Player());
    }
};
}
