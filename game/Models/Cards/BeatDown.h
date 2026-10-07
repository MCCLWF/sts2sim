#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Combat/CombatManager.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct BeatDown : CardModel {
    SIM_MODEL(BeatDown, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardList attacks;
        for (CardModel card : Owner().PlayerCombatState().DiscardPile().Cards()) {
            if (card.Type() == CardType::Attack && !card.Keywords().Contains(CardKeyword::Unplayable)) attacks.push_back(card);
        }
        CardPileCmd::StableShuffle(attacks, Owner().RunState().Rng().Shuffle());
        int32_t count = std::min(DynamicVars().Cards().IntValue(), static_cast<int32_t>(attacks.size()));
        sts2::CombatState combat = Owner().Creature().CombatState();
        for (int32_t i = 0; i < count && !CombatManager(&sim()).IsOverOrEnding(); ++i) {
            CardModel card = attacks[static_cast<size_t>(i)];
            Creature target;
            if (card.TargetType() == TargetType::AnyEnemy)
                target = Owner().RunState().Rng().CombatTargets().NextItem(combat.HittableEnemies());
            CardCmd::AutoPlay(ctx, card, target);
        }
    }
};

}  // namespace sts2
