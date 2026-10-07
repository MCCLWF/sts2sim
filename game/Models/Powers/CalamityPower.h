#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct CalamityPower : PowerModel {
    SIM_MODEL(CalamityPower, PowerModel)

    void BeforeCardPlayed(const CardPlay* play) const {
        if (!play || play->Card.Owner().Creature() != Owner() || play->Card.Type() != CardType::Attack) return;
        play->Card.st().calamity_pending_amount = Amount();
    }

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play) return;
        int32_t amount = play->Card.st().calamity_pending_amount;
        if (amount < 0) return;
        play->Card.st().calamity_pending_amount = -1;
        CardList cards = CardFactory::GetForCombat(Owner().Player(), sts2::data(Owner().Player().Character()).card_pool,
                                                   amount, Owner().Player().RunState().Rng().CombatCardGeneration(),
                                                   CardType::Attack);
        for (CardModel card : cards) CardPileCmd::AddGeneratedCardToCombat(card, PileType::Hand, Owner().Player());
    }
};

}  // namespace sts2
