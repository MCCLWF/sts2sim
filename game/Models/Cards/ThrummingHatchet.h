#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct ThrummingHatchet : CardModel {
    SIM_MODEL(ThrummingHatchet, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("Thrumming Hatchet target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
    }

    void BeforeHandDraw(Player player, PlayerChoiceContext&, sts2::CombatState) const {
        if (player != Owner() || !player.PlayerCombatState() ||
            st().last_played_turn != player.PlayerCombatState().TurnNumber() - 1) return;
        CardPile pile = Pile();
        if (!pile || pile.Type() != PileType::Hand) CardPileCmd::Add(*this, PileType::Hand);
    }
};

}  // namespace sts2
