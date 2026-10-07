#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/Cards/Wither.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct WitheringPresencePower : PowerModel {
    SIM_MODEL(WitheringPresencePower, PowerModel)

    void InitializeCounter() const { extra(0) = 6; }

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* cardPlay) const {
        if (!cardPlay || cardPlay->Card.Owner() != Target().Player()) return;
        --extra(0);
        if (extra(0) > 0) return;
        CreatureList targets{Target()};
        CardPileCmd::AddToCombatAndPreview(CardId::WITHER, targets, PileType::Hand, 1, Player{});
        extra(0) = 6;
    }
};

}  // namespace sts2
