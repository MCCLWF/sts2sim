#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Hologram : CardModel { SIM_MODEL(Hologram, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        CardList selected = CardSelectCmd::FromCombatPile(ctx, Owner().PlayerCombatState().DiscardPile(), Owner(), CardSelectorPrefs(1));
        if (!selected.empty()) CardPileCmd::Add(selected[0], PileType::Hand);
    }
};
}
