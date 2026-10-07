#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct ManifestAuthority : CardModel {
    SIM_MODEL(ManifestAuthority, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext&, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        CardList generated = CardFactory::GetForCombat(Owner(), CardPoolId::COLORLESS_CARD_POOL, 1,
            Owner().RunState().Rng().CombatCardGeneration(), CardType::None, true);
        if (generated.empty()) return;
        if (CurrentUpgradeLevel() > 0) CardCmd::Upgrade(generated[0]);
        CardPileCmd::AddGeneratedCardToCombat(generated[0], PileType::Hand, Owner());
    }
};
}  // namespace sts2
