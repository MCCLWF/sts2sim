#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct UpMySleeve : CardModel {
    SIM_MODEL(UpMySleeve, CardModel)

    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        for (int32_t i = 0; i < DynamicVars().Cards().IntValue(); ++i) {
            CardModel shiv = CombatState().CreateCard(CardId::SHIV, Owner());
            CardPileCmd::AddGeneratedCardToCombat(shiv, PileType::Hand, Owner());
        }
        ++extra(0);  // Source's private per-combat play counter (display/state only).
        EnergyCost().AddThisCombat(-1);
    }
};

}  // namespace sts2
