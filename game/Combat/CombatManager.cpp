// Combat/CombatManager.cs
//
// The C# turn loop (StartCombatInternal / AwaitTurnEndAndSwitchSides) is cut
// at its two waits for the player: StartCombat() runs it up to the first
// play phase, EndPlayerTurn() from the end-turn signal to the next play phase.
// Everything in between runs in the C# order. Multiplayer readiness, UI,
// pauses and cancellation are gone (single player, no timers).
#include "game/Combat/CombatManager.h"
#include "game/Commands/OrbCmd.h"
#include "game/Models/EncounterModel.h"

#include "game/Combat/History/CombatHistory.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Hooks/Hook.h"

namespace sts2 {

namespace {
constexpr uint8_t kPlayer = 1;  // the only player

void SetPhaseForAllPlayers(Sim* sim, PlayerTurnPhase phase) {
    for (Player p : CombatState(sim).Players()) {
        if (p.PlayerCombatState()) p.PlayerCombatState().SetPhase(phase);
    }
}
}  // namespace

CombatHistory CombatManager::History() const { return CombatHistory(sim_); }

// ------------------------------------------------------------------ status

bool CombatManager::IsCombatEnding() const {
    if (!st().is_in_progress) return false;
    if (st().pending_loss) return true;
    CombatState state(sim_);
    for (Creature e : state.Enemies()) {
        if (e && e.IsAlive() && e.IsPrimaryEnemy()) return false;
    }
    if (Hook::ShouldStopCombatFromEnding(state)) return false;
    return true;
}

bool CombatManager::IsEnding() const { return st().exists && IsCombatEnding(); }

bool CombatManager::IsExecutingCardOrPotionEffect(Player player) const {
    return st().executing_card_or_potion_effect > 0;
}

void CombatManager::BeginCardOrPotionEffect(Player player) { ++st().executing_card_or_potion_effect; }

void CombatManager::EndCardOrPotionEffect(Player player) {
    int num = st().executing_card_or_potion_effect - 1;
    if (num <= 0) {
        st().executing_card_or_potion_effect = 0;
        if (player.Creature().IsDead()) {
            // RemoveDeadPlayerCardsFromCombat: only while some player is alive.
            // Single player: the only player is dead, so nothing to do.
        }
    } else {
        st().executing_card_or_potion_effect = static_cast<int8_t>(num);
    }
}

void CombatManager::CheckForEmptyHand(PlayerChoiceContext& choiceContext, Player player) {
    if (st().is_in_progress && !IsExecutingCardOrPotionEffect(player) &&
        player.PlayerCombatState().Hand().IsEmpty()) {
        Hook::AfterHandEmptied(CombatState(sim_), choiceContext, player);
    }
}

// -------------------------------------------------------- setup, creatures

// SetUpCombat. The combat's CombatTurnState starts with IsStarting = true.
void CombatManager::SetUpCombat(CombatState state) {
    st().is_starting = true;
    st().is_in_progress = false;
    for (Player p : state.Players()) p.ResetCombatState();
    for (Player p : state.Players()) p.PopulateCombatState(p.RunState().Rng().Shuffle(), state);
    for (Creature c : state.Creatures()) AddCreature(c);
}

void CombatManager::AddCreature(Creature creature) {
    CombatState state(sim_);
    if (!state.ContainsCreature(creature)) throw GameError("CombatState must already contain creature.");
    if (MonsterModel m = creature.Monster()) m.SetUpForCombat();
    if (creature.HasSlotName()) state.SortEnemiesBySlotName();
}

void CombatManager::AfterCreatureAdded(Creature creature) {
    CombatState state(sim_);
    creature.AfterAddedToRoom();
    if (creature.IsEnemy() && state.CurrentSide() == CombatSide::Player) {
        CreatureList targets;
        for (Player p : state.Players()) targets.push_back(p.Creature());
        creature.Monster().RollMove(targets);
    }
}

void CombatManager::RemoveCreature(Creature creature) {
    if (creature.IsMonster()) {
        creature.Monster().BeforeRemovedFromRoom();
        creature.Monster().ResetStateMachine();
    }
}

void CombatManager::Reset(bool graceful) {
    if (graceful && st().exists) {
        CombatState state(sim_);
        SetPhaseForAllPlayers(sim_, PlayerTurnPhase::None);
        for (Creature item : state.Creatures()) {  // Creatures.ToList()
            item.Reset();
            RemoveCreature(item);
            state.RemoveCreature(item);
        }
    }
    st() = CombatData{};  // _turnState = null; History.Clear(); _cardOrPotionEffectDepth.Clear()
}

void CombatManager::LoseCombat() {
    if (st().exists && !st().pending_loss) st().pending_loss = true;
}

// CheckWinCondition: ProcessPendingLoss / EndCombatInternal.
bool CombatManager::CheckWinCondition() {
    if (st().pending_loss) {
        CheckpointExecutionBoundary(ExecutionBoundaryKind::SafeOutcome);
        st().pending_loss = false;
        st().is_in_progress = false;  // ProcessPendingLoss; CombatEnded ends the run
        return true;
    }
    if (IsCombatEnding()) {
        CheckpointExecutionBoundary(ExecutionBoundaryKind::SafeOutcome);
        EndCombatInternal();
        return true;
    }
    return false;
}

void CombatManager::CheckpointExecutionBoundary(ExecutionBoundaryKind kind) {
    // Current AttackContext users are card effects inside OnPlayWrapper, whose
    // effect-depth scope outlives the context. Only expose the boundary after
    // those wrappers (and any nested choice replay prefix) have completed.
    if (st().executing_card_or_potion_effect != 0) return;
    sim_->CheckpointExecutionBoundary(kind);
}

// ---------------------------------------------------------------- turn loop

// StartCombatInternal up to the first wait (AwaitTurnEndAndSwitchSides).
void CombatManager::StartCombat() {
    CombatState state(sim_);
    st().player_ethereal_cards_played_this_combat = 0;
    st().player_cards_drawn_this_turn = 0;
    st().player_applied_doom_this_turn = false;
    sim_->g->creatures.visit_snapshot([](auto, auto) {}, [&](auto ref, const auto&) {
        sim_->g->creatures[ref].attacks_this_turn = 0;
    });
    for (Creature c : state.Creatures()) AfterCreatureAdded(c);
    st().is_in_progress = true;
    st().is_starting = false;
    Hook::BeforeCombatStart(state.RunState(), state);
    StartTurn();
    if (st().is_in_progress && st().player_ready_to_end_turn) RunAfterEndTurnSignal();
}

void CombatManager::StartTurn() {
    if (!st().is_in_progress) return;
    CombatState state(sim_);
    sim_->g->creatures.visit_snapshot([](auto, auto) {}, [&](auto ref, const auto&) {
        sim_->g->creatures[ref].attacks_this_turn = 0;
    });
    sim_->g->creatures.visit_snapshot([](auto, auto) {}, [&](auto ref, const auto&) {
        sim_->g->creatures[ref].powered_damage_this_turn.clear();
    });
    if (state.CurrentSide() == CombatSide::Player) {
        st().player_lost_hp_previous_turn = st().player_lost_hp_this_turn;
        st().player_nonfully_blocked_damage_previous_turn = st().player_nonfully_blocked_damage_this_turn;
        st().player_played_skill_this_turn = false;
        st().player_attack_cards_this_turn = 0;
        st().player_lost_hp_this_turn = false;
        st().player_nonfully_blocked_damage_this_turn = false;
        st().player_cards_exhausted_this_turn = 0;
        st().player_bound_cards_afflicted_this_turn = 0;
        st().player_cards_started_this_turn = 0;
        st().player_all_cards_started_this_turn = 0;
        st().player_cards_finished_this_turn = 0;
        st().player_skill_cards_finished_this_turn = 0;
        st().player_shiv_cards_finished_this_turn = 0;
        st().player_cards_discarded_this_turn = 0;
        // Keep room-entry star gains in the first player's turn history. The
        // source game records Divine Right before combat starts, then formulas
        // like Radiate query that entry during turn one.
        if (!state.Players().empty() && state.Players().front().PlayerCombatState().TurnNumber() > 1)
            st().player_stars_gained_this_turn = 0;
        st().player_energy_spent_this_turn = 0;
        st().player_status_cards_drawn_this_turn = 0;
        st().player_cards_drawn_this_turn = 0;
        st().player_applied_doom_this_turn = false;
        st().player_zero_cost_attacks_started_this_turn = 0;
    }
    SetPhaseForAllPlayers(sim_, PlayerTurnPhase::None);
    bool isExtraPlayerTurn = st().player_taking_extra_turn;
    CreatureList creaturesStartingTurn;
    StaticVec<Player, 4> playersStartingTurn;
    if (state.CurrentSide() == CombatSide::Player && isExtraPlayerTurn) {
        creaturesStartingTurn.push_back(Player(sim_, kPlayer).Creature());
        playersStartingTurn.push_back(Player(sim_, kPlayer));
    } else {
        creaturesStartingTurn = state.CreaturesOnCurrentSide();
        if (state.CurrentSide() == CombatSide::Player) playersStartingTurn = state.Players();
    }
    for (Creature c : creaturesStartingTurn) c.BeforeTurnStart(state.CurrentSide());
    Hook::BeforeSideTurnStart(state, state.CurrentSide(), creaturesStartingTurn);
    if (state.CurrentSide() == CombatSide::Player) {
        SetPhaseForAllPlayers(sim_, PlayerTurnPhase::Start);
        st().player_actions_disabled = false;
        st().player_ready_to_end_turn = false;
        if (!isExtraPlayerTurn) {
            for (Creature enemy : state.Enemies()) enemy.PrepareForNextTurn(state.PlayerCreatures());
        }
    }
    for (Creature c : creaturesStartingTurn) c.AfterTurnStart(state.CurrentSide());
    for (Creature c : creaturesStartingTurn) Hook::AfterBlockCleared(state, c);
    // SetupPlayerTurn runs for each player with its own choice context. A
    // choice inside it is answered from the prefix, so the whole setup runs
    // before AfterSideTurnStart (see docs/audits/pitfalls.md, turn-start choices).
    PlayerChoiceContext setupContext;
    for (Player p : playersStartingTurn) SetupPlayerTurn(p, setupContext);
    Hook::AfterSideTurnStart(state, state.CurrentSide(), creaturesStartingTurn);
    if (state.CurrentSide() == CombatSide::Player) {
        for (Player p : playersStartingTurn) OrbCmd::AfterTurnStart(setupContext, p);
        for (Player p : state.Players()) {
            bool starting = false;
            for (Player s : playersStartingTurn) starting |= s == p;
            if (p.Creature().IsDead() || !starting) {
                st().player_ready_to_end_turn = true;  // SetReadyToEndTurn
                return;                                // AllPlayersReadyToEndTurn (single player)
            }
        }
        for (Player p : playersStartingTurn) {
            if (!p.Creature().IsDead()) RunAutoPrePlayPhase(setupContext, p);
        }
        CheckWinCondition();
        if (st().is_in_progress) st().is_enemy_turn_started = false;
    } else {
        st().is_enemy_turn_started = true;
        CheckWinCondition();
        if (st().is_in_progress) ExecuteEnemyTurn();
    }
}

void CombatManager::RunAutoPrePlayPhase(PlayerChoiceContext& choiceContext, Player player) {
    player.PlayerCombatState().SetPhase(PlayerTurnPhase::AutoPrePlay);
    CheckForEmptyHand(choiceContext, player);
    Hook::AfterAutoPrePlayPhaseEntered(choiceContext, CombatState(sim_), player);
    player.PlayerCombatState().SetPhase(PlayerTurnPhase::Play);
}

void CombatManager::SetupPlayerTurn(Player player, PlayerChoiceContext& choiceContext) {
    if (player.Creature().IsDead()) return;
    PlayerCombatState pcs = player.PlayerCombatState();
    if (!pcs) return;
    CombatState state(sim_);
    if (Hook::ShouldPlayerResetEnergy(state, player)) {
        pcs.ResetEnergy();
    } else {
        pcs.AddMaxEnergyToCurrent();
    }
    Hook::AfterEnergyReset(state, player);
    Hook::BeforeHandDraw(state, player, choiceContext);
    ModelList modifiers;
    Dec handDraw = Hook::ModifyHandDraw(state, player, Dec(5), modifiers);
    Hook::AfterModifyingHandDraw(state, modifiers);
    if (pcs.TurnNumber() == 1) {
        CardPile pile = pcs.DrawPile();
        CardList list;
        for (CardModel c : pile.Cards()) {
            EnchantmentModel e = c.Enchantment();
            if (e && e.ShouldStartAtBottomOfDrawPile()) list.push_back(c);
        }
        for (CardModel c : list) pile.MoveToBottomInternal(c);
        CardList list2;
        for (CardModel c : pile.Cards()) {
            if (!c.Keywords().Contains(CardKeyword::Innate)) continue;
            bool inList = false;  // .Except(list)
            for (CardModel b : list) inList |= b == c;
            bool dup = false;     // Except is a set operation
            for (CardModel b : list2) dup |= b == c;
            if (!inList && !dup) list2.push_back(c);
        }
        for (CardModel c : list2) pile.MoveToTopInternal(c);
        handDraw = dec_max(handDraw, Dec(static_cast<int64_t>(list2.size())));
        handDraw = dec_min(handDraw, Dec(CardPile::MaxCardsInHand));
    }
    CardPileCmd::Draw(choiceContext, handDraw, player, /*fromHandDraw=*/true);
    Hook::AfterPlayerTurnStart(state, choiceContext, player);
}

// EndPlayerTurnAction -> PlayerCmd.EndTurn -> SetReadyToEndTurn, then the rest
// of AwaitTurnEndAndSwitchSides and the turn loop up to the next wait.
void CombatManager::EndPlayerTurn() {
    CombatState state(sim_);
    if (!st().is_in_progress || state.CurrentSide() != CombatSide::Player) return;
    if (st().player_ready_to_end_turn) return;  // PlayerCmd.EndTurn: already ready
    st().player_actions_disabled = true;       // OnEndedTurnLocally
    st().player_ready_to_end_turn = true;
    RunAfterEndTurnSignal();
}

void CombatManager::AfterPlayerAction() {
    if (st().is_in_progress) CheckWinCondition();
    if (st().is_in_progress && st().player_ready_to_end_turn && CombatState(sim_).CurrentSide() == CombatSide::Player) {
        RunAfterEndTurnSignal();
    }
}

// AwaitTurnEndAndSwitchSides after the end-turn signal, then
// `while (turnState.IsLive)`: enemy turns run through; a player turn waits for
// the player, unless its start already made the player ready to end the turn
// (a dead player), in which case the signal is already set and the loop goes on.
void CombatManager::RunAfterEndTurnSignal() {
    CombatState state(sim_);
    for (;;) {
        // AfterAllPlayersReadyToEndTurn
        st().ending_player_turn_phase_one = true;
        EndPlayerTurnPhaseOneInternal();
        st().ending_player_turn_phase_one = false;
        // ReadyToBeginEnemyTurnAction -> AfterAllPlayersReadyToBeginEnemyTurn
        if (!st().is_in_progress) return;
        AfterAllPlayersReadyToBeginEnemyTurn();
        while (st().is_in_progress && state.CurrentSide() != CombatSide::Player) StartTurn();
        if (!st().is_in_progress) return;
        StartTurn();
        if (!st().is_in_progress || !st().player_ready_to_end_turn) return;
    }
}

void CombatManager::EndPlayerTurnPhaseOneInternal() {
    CombatState state(sim_);
    if (state.CurrentSide() != CombatSide::Player) {
        throw GameError("EndPlayerTurn called while the current side is not the player");
    }
    StaticVec<Player, 4> playersEndingTurn = st().player_taking_extra_turn
                                                 ? StaticVec<Player, 4>{Player(sim_, kPlayer)}
                                                 : state.Players();
    for (Player p : playersEndingTurn) {
        p.PlayerCombatState().SetPhase(PlayerTurnPhase::AutoPostPlay);
        PlayerChoiceContext ctx;
        Hook::AfterAutoPostPlayPhaseEntered(ctx, state, p);
    }
    for (Player p : playersEndingTurn) p.PlayerCombatState().SetPhase(PlayerTurnPhase::End);
    CreatureList participants;
    for (Player p : playersEndingTurn) participants.push_back(p.Creature());
    Hook::BeforeSideTurnEnd(state, state.CurrentSide(), participants);
    if (CheckWinCondition()) return;
    for (Player p : playersEndingTurn) {
        PlayerChoiceContext ctx;
        DoTurnEnd(p, ctx);
    }
    if (CheckWinCondition()) return;
    for (Player p : playersEndingTurn) Hook::BeforeFlush(state, p);
    CheckWinCondition();
}

void CombatManager::DoTurnEnd(Player player, PlayerChoiceContext& choiceContext) {
    OrbCmd::BeforeTurnEnd(choiceContext, player);
    if (!st().is_in_progress || IsCombatEnding()) return;
    CombatState state(sim_);
    CardPile pile = player.PlayerCombatState().Hand();
    CardList turnEndCards;
    CardList list;
    for (CardModel card : pile.Cards()) {
        if (card.HasTurnEndInHandEffect()) {
            turnEndCards.push_back(card);
        } else if (card.Keywords().Contains(CardKeyword::Ethereal) && Hook::ShouldEtherealTrigger(state, card)) {
            list.push_back(card);
        }
    }
    for (CardModel card : list) CardCmd::Exhaust(choiceContext, card, /*causedByEthereal=*/true);
    DoTurnEndCards(turnEndCards, choiceContext);
}

// DoTurnEndCards chains the cards: each card's effects wait for the previous
// card's effects and its own move to the play pile, so the order is card by
// card: move to Play, OnTurnEndInHand, then to Discard (or Exhaust if ethereal).
void CombatManager::DoTurnEndCards(const CardList& turnEndCards, PlayerChoiceContext& choiceContext) {
    for (CardModel card : turnEndCards) {
        CardPileCmd::Add(card, PileType::Play);
        card.OnTurnEndInHandWrapper(choiceContext);
        if (!card.Keywords().Contains(CardKeyword::Ethereal)) {
            CardPileCmd::Add(card, CardPile::Get(PileType::Discard, card.Owner()), CardPilePosition::Bottom,
                             AbstractModel{}, /*skipVisuals=*/true);
        } else {
            CardCmd::Exhaust(choiceContext, card, /*causedByEthereal=*/true, /*skipVisuals=*/true);
        }
    }
}

void CombatManager::AfterAllPlayersReadyToBeginEnemyTurn() {
    CombatState state(sim_);
    st().ending_player_turn_phase_two = true;
    if (st().is_in_progress && state.CurrentSide() == CombatSide::Player) {
        EndPlayerTurnPhaseTwoInternal();
        SwitchFromPlayerToEnemySide();
    }
    st().ending_player_turn_phase_two = false;
}

void CombatManager::EndPlayerTurnPhaseTwoInternal() {
    CombatState state(sim_);
    StaticVec<Player, 4> playersEndingTurn = st().player_taking_extra_turn
                                                 ? StaticVec<Player, 4>{Player(sim_, kPlayer)}
                                                 : state.Players();
    for (Player p : playersEndingTurn) {
        PlayerChoiceContext ctx;
        FlushPlayerHand(p, ctx);
    }
    CreatureList participants;
    for (Player p : playersEndingTurn) participants.push_back(p.Creature());
    Hook::AfterSideTurnEnd(state, state.CurrentSide(), participants);
}

void CombatManager::FlushPlayerHand(Player player, PlayerChoiceContext& choiceContext) {
    if (player.Creature().IsDead()) return;
    PlayerCombatState pcs = player.PlayerCombatState();
    if (!pcs) return;
    CombatState state(sim_);
    CardList cardsToFlush;
    CardList cardsToRetain;
    bool flag = Hook::ShouldFlush(state, player);
    for (CardModel card : pcs.Hand().Cards()) {
        if (!flag || card.ShouldRetainThisTurn()) {
            cardsToRetain.push_back(card);
        } else {
            cardsToFlush.push_back(card);
        }
    }
    if (!cardsToFlush.empty()) CardPileCmd::Add(cardsToFlush, PileType::Discard);
    Hook::AfterFlush(state, player, choiceContext, cardsToFlush, cardsToRetain);
    pcs.EndOfTurnCleanup();
}

void CombatManager::SwitchFromPlayerToEnemySide() {
    CombatState state(sim_);
    st().player_taking_extra_turn = false;
    bool extra = false;
    for (Player p : state.Players()) {
        if (Hook::ShouldTakeExtraTurn(state, p)) {
            st().player_taking_extra_turn = true;
            extra = true;
        }
    }
    SwitchSides();
    if (extra) Hook::AfterTakingExtraTurn(state, Player(sim_, kPlayer));
}

void CombatManager::SwitchSides() {
    CombatState state(sim_);
    bool flag = st().player_taking_extra_turn;
    if (state.CurrentSide() == CombatSide::Player && !flag) {
        state.SetCurrentSide(CombatSide::Enemy);
    } else {
        state.SetCurrentSide(CombatSide::Player);
        if (!flag) state.SetRoundNumber(state.RoundNumber() + 1);
        for (Player p : state.Players()) p.PlayerCombatState().IncrementTurnNumber();
    }
    // Native CombatHistoryEntry.HappenedThisTurn compares CurrentSide and
    // every player's TurnNumber as well as RoundNumber. A side switch always
    // invalidates the previous started-card count, including an extra player
    // turn that keeps the same side and round but increments TurnNumber.
    st().player_attack_cards_started_this_turn = 0;
    st().player_attack_skill_cards_started_this_turn = 0;
    // Iteration's CardDrawnEntry.HappenedThisTurn also distinguishes sides.
    // GremlinHorn can draw a Status during the enemy turn and must see it as
    // the first such entry even if Status cards were drawn on the player side.
    st().player_status_cards_drawn_this_turn = 0;
    for (Creature c : state.Creatures()) c.OnSideSwitch();
}

void CombatManager::ExecuteEnemyTurn() {
    if (!st().is_in_progress) return;
    CombatState state(sim_);
    for (Creature enemy : state.Enemies()) {  // Enemies.ToList()
        if (!state.ContainsCreature(enemy)) continue;
        enemy.TakeTurn();
        CheckWinCondition();
        if (!st().is_in_progress) return;
    }
    EndEnemyTurn();
}

void CombatManager::EndEnemyTurn() {
    if (!st().is_in_progress) return;
    CombatState state(sim_);
    if (state.CurrentSide() != CombatSide::Enemy) throw GameError("EndEnemyTurn called while the current side is not the enemy");
    EndEnemyTurnInternal();
    CheckWinCondition();
    CheckpointExecutionBoundary(ExecutionBoundaryKind::EnemyPhaseDone);
    if (!IsCombatEnding()) SwitchSides();
}

void CombatManager::EndEnemyTurnInternal() {
    CombatState state(sim_);
    CreatureList enemies = state.CreaturesOnCurrentSide();
    Hook::BeforeSideTurnEnd(state, state.CurrentSide(), enemies);
    for (Player p : state.Players()) p.PlayerCombatState().EndOfTurnCleanup();
    Hook::AfterSideTurnEnd(state, state.CurrentSide(), enemies);
}

// EndCombatInternal: the combat-level part. Room rewards, saving and the
// map screen belong to the run flow (M1-i).
void CombatManager::EndCombatInternal() {
    CombatState state(sim_);
    st().is_in_progress = false;
    st().player_taking_extra_turn = false;
    SetPhaseForAllPlayers(sim_, PlayerTurnPhase::None);
    st().player_actions_disabled = false;
    for (Player p : state.Players()) p.ReviveBeforeCombatEnd();
    CombatRoom room(sim_, data(state.Encounter()).room_type);
    Hook::AfterCombatEnd(state.RunState(), state, room);
    // History.Clear(); room.OnCombatEnded(): GoldProportion = Encounter.CalculateGoldProportion(CombatState).
    st().gold_proportion = EncounterModel(sim_).CalculateGoldProportion(state);
    for (Player p : state.Players()) p.AfterCombatEnd();
    Hook::AfterCombatVictory(state.RunState(), state, room);
}

}  // namespace sts2
