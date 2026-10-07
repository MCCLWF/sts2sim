// Combat/CombatManager.cs
//
// The C# CombatManager is a singleton that runs the turn loop as a
// long-lived async task and waits on end-turn signals. Here the loop is cut
// at its waits: each player action (play a card, use a potion, end the turn)
// is executed by one synchronous call that returns at the next point where
// the game waits for the player (docs/design-engine.md §5). Its state lives
// in CombatData (engine/sim/state.h).
#pragma once

#include <cstdint>

#include "game/All.h"

namespace sts2 {

class CombatHistory;

class CombatManager {
public:
    explicit CombatManager(Sim* sim) : sim_(sim) {}

    // --- status (CombatManager / CombatTurnState properties)
    bool IsInProgress() const { return st().is_in_progress; }
    bool IsStarting() const { return st().is_starting; }
    bool IsAboutToLose() const { return st().pending_loss; }
    bool IsEnding() const;
    bool IsOverOrEnding() const { return IsEnding() || !IsInProgress(); }
    bool EndingPlayerTurnPhaseOne() const { return st().ending_player_turn_phase_one; }
    bool EndingPlayerTurnPhaseTwo() const { return st().ending_player_turn_phase_two; }
    bool PlayerActionsDisabled() const { return st().player_actions_disabled; }
    sts2::CombatHistory History() const;

    // --- card and potion effects
    bool IsExecutingCardOrPotionEffect(Player player) const;
    void BeginCardOrPotionEffect(Player player);
    void EndCardOrPotionEffect(Player player);
    void CheckForEmptyHand(PlayerChoiceContext& choiceContext, Player player);

    // --- setup and creatures
    void SetUpCombat(CombatState state);
    void AddCreature(Creature creature);
    void AfterCreatureAdded(Creature creature);
    void RemoveCreature(Creature creature);
    void LoseCombat();
    // Reset(graceful): tears the combat down when its room is left.
    void Reset(bool graceful);
    bool CheckWinCondition();

    // --- the turn loop, cut at the player's waits
    // StartCombatInternal up to the first wait for the player.
    void StartCombat();
    // EndPlayerTurnAction: phase one, phase two, the enemy turn and the next
    // player turn start, up to the next wait for the player (or combat end).
    void EndPlayerTurn();
    // After a player-driven action other than ending the turn: the
    // ActionExecutor's CheckWinCondition, then, if the action made the player
    // ready to end the turn (PlayerCmd.EndTurn), the rest of the turn loop.
    void AfterPlayerAction();

private:
    CombatData& st() const { return sim_->g->combat; }
    bool IsCombatEnding() const;
    void CheckpointExecutionBoundary(ExecutionBoundaryKind kind);
    void StartTurn();
    void RunAutoPrePlayPhase(PlayerChoiceContext& choiceContext, Player player);
    void SetupPlayerTurn(Player player, PlayerChoiceContext& choiceContext);
    void EndCombatInternal();
    void ExecuteEnemyTurn();
    void EndEnemyTurn();
    void EndEnemyTurnInternal();
    void RunAfterEndTurnSignal();
    void EndPlayerTurnPhaseOneInternal();
    void DoTurnEnd(Player player, PlayerChoiceContext& choiceContext);
    void DoTurnEndCards(const CardList& turnEndCards, PlayerChoiceContext& choiceContext);
    void AfterAllPlayersReadyToBeginEnemyTurn();
    void EndPlayerTurnPhaseTwoInternal();
    void FlushPlayerHand(Player player, PlayerChoiceContext& choiceContext);
    void SwitchFromPlayerToEnemySide();
    void SwitchSides();

    Sim* sim_;
};

}  // namespace sts2
