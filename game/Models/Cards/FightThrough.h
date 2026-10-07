#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct FightThrough : CardModel { SIM_MODEL(FightThrough, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext&, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        for (int32_t i = 0; i < 2; ++i)
            CardPileCmd::AddGeneratedCardToCombat(CombatState().CreateCard(CardId::WOUND, Owner()), PileType::Discard, Owner());
    }
};
}
