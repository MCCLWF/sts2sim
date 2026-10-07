// Models/Relics/SereTalon.cs (v0.111.0).
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct SereTalon : RelicModel {
    SIM_MODEL(SereTalon, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        PlayerChoiceContext context = PlayerChoiceContext::Throwing();
        CreatureCmd::LoseMaxHp(context, Owner().Creature(), Dec(9), false);
        for (int32_t i = 0; i < 3; ++i)
            CardPileCmd::Add(Owner().RunState().CreateCard(CardId::WISH, Owner()), PileType::Deck);
    }
};

}  // namespace sts2
