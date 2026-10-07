// Commands/ForgeCmd.cs. Visual smithing effects are omitted; card creation,
// damage accumulation, and AfterForge hooks retain the original order.
#pragma once

#include "game/All.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Combat/CombatManager.h"
#include "game/Hooks/Hook.h"
#include "game/Models/Cards/SovereignBlade.h"

namespace sts2::ForgeCmd {

inline CardList GetSovereignBlades(Player player, bool includeExhausted) {
    CardList blades;
    for (CardModel card : player.PlayerCombatState().AllCards()) {
        if (!card.IsDupe() && card.CardId_() == CardId::SOVEREIGN_BLADE &&
            (includeExhausted || card.Pile().Type() != PileType::Exhaust)) {
            blades.push_back(card);
        }
    }
    return blades;
}

// Forge first ensures a non-exhausted blade is available, then adds damage to
// every blade including exhausted ones, and finally dispatches AfterForge.
inline CardList Forge(Dec amount, Player player, AbstractModel source = {}) {
    CombatState combatState = player.Creature().CombatState();
    if (!combatState || CombatManager(&player.sim()).IsOverOrEnding()) return {};

    CardList available = GetSovereignBlades(player, false);
    if (available.empty()) {
        CardModel blade = combatState.CreateCard(CardId::SOVEREIGN_BLADE, player);
        CardPileCmd::AddGeneratedCardToCombat(blade, PileType::Hand, player);
        available.push_back(blade);
    }

    for (CardModel blade : GetSovereignBlades(player, true)) SovereignBlade(blade).AddDamage(amount);
    Hook::AfterForge(combatState, amount, player, source);
    return available;
}

}  // namespace sts2::ForgeCmd
