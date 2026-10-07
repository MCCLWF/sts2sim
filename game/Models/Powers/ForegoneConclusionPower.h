#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
struct ForegoneConclusionPower : PowerModel {
    SIM_MODEL(ForegoneConclusionPower, PowerModel)
    void BeforeHandDraw(Player player, PlayerChoiceContext& ctx, sts2::CombatState) const {
        Player owner = Owner().Player();
        if (!owner || player != owner) return;
        CardPileCmd::ShuffleIfNecessary(ctx, owner);
        CardList chosen = CardSelectCmd::FromCombatPile(ctx, CardPile::Get(PileType::Draw, owner), owner, CardSelectorPrefs(Amount()));
        CardPileCmd::Add(chosen, PileType::Hand);
        PowerCmd::Remove(*this);
    }
};
}  // namespace sts2
