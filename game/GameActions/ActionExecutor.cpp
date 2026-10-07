// GameActions/ActionExecutor.cs, PlayCardAction.cs, EndPlayerTurnAction.cs
#include "game/GameActions/ActionExecutor.h"

#include "game/Combat/CombatManager.h"
#include "game/Entities/Cards/CardPlay.h"

namespace sts2::ActionExecutor {

namespace {

// PlayCardAction.ExecuteAction
bool PlayCardExecute(CardModel card, Creature target) {
    CardPile pile = card.Pile();
    if (!pile || pile.Type() != PileType::Hand) return false;
    UnplayableReason reason{};
    AbstractModel preventer;
    if (!card.CanPlay(reason, preventer) || !card.IsValidTarget(target)) return false;  // Cancel()
    CardModel::SpentResources spent = card.SpendResources();
    ResourceInfo resources;
    resources.EnergySpent = spent.energy;
    resources.EnergyValue = spent.energy;
    resources.StarsSpent = spent.stars;
    resources.StarValue = spent.stars;
    PlayerChoiceContext choiceContext;  // GameActionPlayerChoiceContext
    card.OnPlayWrapper(choiceContext, target, /*isAutoPlay=*/false, resources);
    return true;
}

}  // namespace

bool PlayCard(Sim* sim, CardModel card, Creature target) {
    CombatManager cm(sim);
    bool played = PlayCardExecute(card, target);
    cm.AfterPlayerAction();
    return played;
}

void EndTurn(Sim* sim, int32_t turnNumber) {
    Player player(sim, 1);
    if (player.PlayerCombatState().TurnNumber() != turnNumber) return;  // stale action
    CombatManager(sim).EndPlayerTurn();  // PlayerCmd.EndTurn(player, canBackOut: true) and the turn loop
}

}  // namespace sts2::ActionExecutor
