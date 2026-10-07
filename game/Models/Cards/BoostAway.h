#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct BoostAway : CardModel { SIM_MODEL(BoostAway, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext&, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        CardModel dazed = CombatState().CreateCard(CardId::DAZED, Owner());
        CardPileCmd::AddGeneratedCardToCombat(dazed, PileType::Discard, Owner());
    }
};
}
