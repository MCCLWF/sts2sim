#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Largesse : CardModel {
    SIM_MODEL(Largesse, CardModel)
    void OnPlay(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        Player target = play->Target.Player();
        if (!target) throw GameError("Largesse target has no player");
        CardList generated = CardFactory::GetForCombat(target, CardPoolId::COLORLESS_CARD_POOL, 1,
            Owner().RunState().Rng().CombatCardGeneration(), CardType::None, true);
        if (generated.empty()) return;
        if (CurrentUpgradeLevel() > 0) CardCmd::Upgrade(generated[0]);
        CardPileCmd::AddGeneratedCardToCombat(generated[0], PileType::Hand, Owner());
    }
};
}  // namespace sts2
