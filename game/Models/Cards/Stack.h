#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Entities/Cards/CardPile.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Stack : CardModel {
    SIM_MODEL(Stack, CardModel)

    bool GainsBlock() const { return true; }

    void OnPlay(PlayerChoiceContext&, const CardPlay* play) const {
        int32_t discard_count = CardPile::Get(PileType::Discard, Owner()).Count();
        Dec block = DynamicVars().CalculationBase().BaseValue() +
                    DynamicVars().CalculationExtra().BaseValue() * Dec(discard_count);
        CreatureCmd::GainBlock(Owner().Creature(), block, DynamicVars().CalculatedBlock().Props(), play);
    }

};

}  // namespace sts2
