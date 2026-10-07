#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct GeneticAlgorithm : CardModel { SIM_MODEL(GeneticAlgorithm, CardModel)
    bool GainsBlock() const { return true; }
    int32_t IncreasedBlock() const { return extra(0); }
    void SetCurrentBlock(int32_t value) const { DynamicVars().Block().SetBaseValue(Dec(value)); }
    void BuffFromPlay(int32_t amount) const { extra(0) += amount; SetCurrentBlock(1 + extra(0)); }
    void OnPlay(PlayerChoiceContext&, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        int32_t increase = DynamicVars()["Increase"].IntValue();
        BuffFromPlay(increase);
        CardModel deck = DeckVersion();
        if (deck && deck.CardId_() == CardId::GENETIC_ALGORITHM) GeneticAlgorithm(deck).BuffFromPlay(increase);
    }
    void AfterDowngraded() const { SetCurrentBlock(1 + IncreasedBlock()); }
};
}
