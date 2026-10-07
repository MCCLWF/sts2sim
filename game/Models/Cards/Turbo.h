#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Turbo : CardModel { SIM_MODEL(Turbo, CardModel)
    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
        CardPileCmd::AddGeneratedCardToCombat(CombatState().CreateCard(CardId::VOID, Owner()), PileType::Discard, Owner());
    }
};
}
