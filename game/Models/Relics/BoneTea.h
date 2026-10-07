// Models/Relics/BoneTea.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Entities/Cards/CardPile.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct BoneTea : RelicModel {
    SIM_MODEL(BoneTea, RelicModel)
    bool IsUsedUp() const { return extra(0) >= 1; }
    bool ShowCounter() const { return false; }
    int32_t DisplayAmount() const { return IsUsedUp() ? 0 : 1; }
    void AfterPlayerTurnStart(PlayerChoiceContext&, Player player) const {
        if (IsUsedUp() || player != Owner() || Owner().PlayerCombatState().TurnNumber() > 1) return;
        for (CardModel card : CardPile::Get(PileType::Hand, Owner()).Cards()) CardCmd::Upgrade(card);
        extra(0) = 1;
        DynamicVars()["Combats"].SetBaseValue(Dec(0));
        SetStatus(RelicStatus::Disabled);
    }
};

}  // namespace sts2
