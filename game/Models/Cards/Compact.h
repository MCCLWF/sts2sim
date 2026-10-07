#pragma once
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Compact : CardModel { SIM_MODEL(Compact, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext&, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        std::vector<CardTransformation> transforms;
        for (CardModel card : Owner().PlayerCombatState().Hand().Cards()) {
            if (!card.IsTransformable() || card.Type() != CardType::Status) continue;
            CardModel fuel = CombatState().CreateCard(CardId::FUEL, Owner());
            if (CurrentUpgradeLevel() > 0) CardCmd::Upgrade(fuel);
            transforms.emplace_back(card, fuel);
        }
        CardCmd::Transform(transforms, nullptr);
    }
};
}
