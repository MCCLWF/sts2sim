#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct AllForOne : CardModel { SIM_MODEL(AllForOne, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        CardList discarded = Owner().PlayerCombatState().DiscardPile().Cards();
        for (CardModel card : discarded) {
            CardType type = card.Type();
            bool attackSkillPower = type == CardType::Attack || type == CardType::Skill || type == CardType::Power;
            if (attackSkillPower && !card.EnergyCost().CostsX() && card.EnergyCost().GetWithModifiers(CostModifiers::All) == 0)
                CardPileCmd::Add(card, PileType::Hand);
        }
    }
};
}
