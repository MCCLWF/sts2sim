using System.Reflection;
using System.Text.Json;
using System.Text.Json.Nodes;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Combat.History;
using MegaCrit.Sts2.Core.Combat.History.Entries;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.GameActions.Multiplayer;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Afflictions;
using MegaCrit.Sts2.Core.Models.Encounters;
using MegaCrit.Sts2.Core.Random;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;

namespace CombatSolver;

// Owned fields from the actual idle player turn. Piles and orb instances are
// supplied by their own components. The history sidecar retains the operands
// of all queries represented by CombatData, not the game's full history log.
internal sealed record EnvLiveCombatContext(JsonElement Component)
{
    internal static EnvLiveCombatContext Capture(CombatState state, Player player,
        IReadOnlyList<Creature> orderedCreatures, CombatManager? manager = null, ActionQueueSet? actions = null)
    {
        EnvModelCatalog.RequireGameVersion();
        manager ??= CombatManager.Instance;
        actions ??= ReferenceEquals(RunManager.Instance.State, state.RunState) ? RunManager.Instance.ActionQueueSet : null;
        if (state.GetType() != typeof(CombatState) || manager.GetType() != typeof(CombatManager)
            || state.Players.Count != 1 || !ReferenceEquals(state.Players[0], player)
            || player.PlayerCombatState is not { } pcs || pcs.GetType() != typeof(PlayerCombatState)
            || !ReferenceEquals(pcs._player, player) || state.Modifiers.Count != 0)
            throw new InvalidOperationException("live_combat_requires_single_player_vanilla_state");
        var turn = manager._turnState;
        if (turn is null || !ReferenceEquals(turn.State, state) || turn.Ct.IsCancellationRequested)
            throw new InvalidOperationException("live_combat_manager_state_missing_or_stale");
        if (!turn.IsInProgress || turn.IsStarting || turn.PendingLoss is not null
            || state.CurrentSide != CombatSide.Player || pcs.Phase != PlayerTurnPhase.Play
            || manager.IsPaused || manager.PlayerActionsDisabled || turn.EndingPlayerTurnPhaseOne
            || turn.EndingPlayerTurnPhaseTwo || turn.IsEnemyTurnStarted || pcs.PlayPile.Cards.Count != 0
            || manager._cardOrPotionEffectDepth.Count != 0 || manager.DebugForcedTopCardOnNextShuffle is not null)
            throw new InvalidOperationException("live_combat_requires_idle_player_play_boundary");
        bool ready, extraTurn;
        using (turn.ReadyLock.EnterScope())
        {
            if (turn.PlayersReadyToEndTurn.Any(p => !ReferenceEquals(p, player))
                || turn.PlayersReadyToBeginEnemyTurn.Count != 0
                || turn.PlayersTakingExtraTurn.Any(p => !ReferenceEquals(p, player))
                || turn.PlayersTakingExtraTurn.Count > 1
                || turn.EndTurnSignalSource?.Task.IsCompleted == true
                || turn.BeginEnemyTurnSignalSource?.Task.IsCompleted == true)
                throw new InvalidOperationException("live_combat_pending_turn_signal_unmapped");
            ready = turn.PlayersReadyToEndTurn.Contains(player);
            extraTurn = turn.PlayersTakingExtraTurn.Contains(player);
        }
        if (ready) throw new InvalidOperationException("live_combat_end_turn_already_queued");
        if (actions is null || actions.GetType() != typeof(ActionQueueSet) || !actions.IsEmpty
            || actions._actionsWaitingForResumption.Count != 0 || actions._actionQueues.Count != 1
            || actions._actionQueues.Any(queue => queue.ownerId != player.NetId || queue.actions.Count != 0 || queue.isPaused))
            throw new InvalidOperationException("live_combat_action_or_choice_pending");
        if (state.RunState.CurrentRoom is not CombatRoom room || room.GetType() != typeof(CombatRoom)
            || !ReferenceEquals(room.CombatState, state) || !room.ShouldCreateCombat
            || room.ParentEventId is not null || state.RunState.CurrentRoomCount != 1)
            throw new InvalidOperationException("live_combat_nested_or_precreated_event_room_unmapped");
        if (!float.IsFinite(room.GoldProportion)) throw new InvalidOperationException("live_combat_gold_proportion_nonfinite");
        if (state.RoundNumber < 1 || pcs.TurnNumber < 1 || state._nextCreatureId > int.MaxValue
            || pcs.OrbQueue.Capacity is < 0 or > 10)
            throw new InvalidOperationException("live_combat_scalar_capacity");
        Dictionary<Creature, int> references = new(ReferenceEqualityComparer.Instance);
        for (int i = 0; i < orderedCreatures.Count; i++)
            if (i >= 23 || !references.TryAdd(orderedCreatures[i], i + 1))
                throw new InvalidOperationException("live_combat_creature_pool_capacity_or_duplicate");
        int Ref(Creature creature) => references.TryGetValue(creature, out int value) ? value
            : throw new InvalidOperationException("live_combat_creature_reference_missing");
        JsonArray Refs(IEnumerable<Creature> values, int capacity)
        {
            var list = values.ToArray();
            if (list.Length > capacity) throw new InvalidOperationException("live_combat_roster_capacity");
            return new(list.Select(creature => JsonValue.Create(Ref(creature))).ToArray());
        }
        EncounterModel encounter = state.Encounter ?? throw new InvalidOperationException("live_combat_encounter_missing");
        int encounterId = EnvModelCatalog.Id(encounter, "encounters");
        if (!encounter.IsMutable || !encounter.HaveMonstersBeenGenerated || encounter._rng is not { } encounterRng
            || encounterRng.GetType() != typeof(Rng) || encounterRng._random.GetType() != typeof(MegaRandom))
            throw new InvalidOperationException("live_combat_encounter_not_initialized");
        for (Type? type = encounter.GetType(); type is not null && type != typeof(EncounterModel); type = type.BaseType)
            foreach (FieldInfo field in type.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly))
                if (type != typeof(GremlinMercNormal) || field.Name != "_goldWasStolen")
                    throw new InvalidOperationException($"live_encounter_subclass_field_unmapped:{encounter.Id.Entry}:{field.Name}");
        JsonArray spawned = [];
        foreach (MonsterModel monster in encounter._spawnedEnemies ?? [])
        {
            if (spawned.Count >= 16 || monster.IsMutable
                || !EnvModelCatalog.Content.GetProperty("monsters").TryGetProperty(monster.Id.Entry, out JsonElement metadata)
                || metadata.GetProperty("type").GetString() != monster.GetType().FullName
                || monster.GetType().Assembly != typeof(MonsterModel).Assembly)
                throw new InvalidOperationException("live_spawned_enemy_binding_or_capacity");
            spawned.Add(metadata.GetProperty("id").GetInt32());
        }
        JsonObject combat = new()
        {
            ["exists"] = true,
            // This synthetic Env flag marks a precombat event layout. The
            // ordinary active CombatRoom checks above prove that it is absent.
            ["precreated_for_event_layout"] = false, ["encounter"] = encounterId,
            ["round_number"] = state.RoundNumber, ["current_side"] = (int)state.CurrentSide,
            ["next_creature_id"] = state._nextCreatureId,
            ["allies"] = Refs(state.Allies, 12), ["enemies"] = Refs(state.Enemies, 12), ["escaped"] = Refs(state.EscapedCreatures, 12),
            ["is_in_progress"] = turn.IsInProgress, ["is_starting"] = turn.IsStarting,
            ["pending_loss"] = turn.PendingLoss is not null, ["player_actions_disabled"] = manager.PlayerActionsDisabled,
            ["ending_player_turn_phase_one"] = turn.EndingPlayerTurnPhaseOne, ["ending_player_turn_phase_two"] = turn.EndingPlayerTurnPhaseTwo,
            ["executing_card_or_potion_effect"] = 0, // verified empty actual depth dictionary above
            ["encounter_rng"] = EnvLiveRunContext.RngValue(encounterRng), ["spawned_enemies"] = spawned,
            ["gremlin_merc_gold_was_stolen"] = encounter is GremlinMercNormal merc && merc._goldWasStolen,
            ["player_ready_to_end_turn"] = ready, ["player_taking_extra_turn"] = extraTurn,
            ["is_enemy_turn_started"] = turn.IsEnemyTurnStarted,
            ["gold_proportion"] = new JsonObject { ["f32_bits"] = BitConverter.SingleToUInt32Bits(room.GoldProportion) },
        };
        JsonArray historySource = CaptureHistory(manager.History, state, player, combat);
        JsonObject playerCombat = new()
        {
            ["exists"] = true, ["turn_number"] = pcs.TurnNumber, ["phase"] = (int)pcs.Phase,
            ["energy"] = pcs.Energy, ["stars"] = pcs.Stars, ["pets"] = Refs(pcs.Pets, 8), ["orb_capacity"] = pcs.OrbQueue.Capacity,
        };
        return new(JsonSerializer.SerializeToElement(new JsonObject
        {
            ["combat_fields"] = combat, ["player_combat_fields"] = playerCombat, ["history_source"] = historySource,
        }));
    }

    private static JsonArray CaptureHistory(CombatHistory history, CombatState state, Player player, JsonObject fields)
    {
        if (history.GetType() != typeof(CombatHistory)) throw new InvalidOperationException("live_combat_history_type_unmapped");
        int finished = 0, attackFinished = 0, attackStarted = 0, attackSkillStarted = 0, damageReceived = 0, exhausted = 0, exhaustedTurn = 0, boundTurn = 0;
        bool skillStarted = false, lostHp = false;
        JsonArray operands = [];
        foreach (CombatHistoryEntry entry in history.Entries)
        {
            if (entry.GetType().Assembly != typeof(CombatHistoryEntry).Assembly || !ReferenceEquals(entry.History, history))
                throw new InvalidOperationException("live_combat_history_entry_unmapped");
            // Other fixed-game entries are no-ops in the current Env history
            // implementation. Do not claim that their original payload exists
            // in CombatData or that future readers can query it.
            if (entry is not (CardPlayStartedEntry or CardPlayFinishedEntry or CardExhaustedEntry or CardAfflictedEntry or DamageReceivedEntry)) continue;
            bool current = entry.HappenedThisTurn(state);
            JsonObject source = new()
            {
                ["kind"] = entry.GetType().Name, ["round"] = entry.RoundNumber, ["side"] = (int)entry.CurrentSide,
                ["actor_combat_id"] = entry.Actor?.CombatId,
                ["player_turns"] = new JsonArray(entry._playerTurnNumbers.Select(pair => (JsonNode)new JsonObject { ["net_id"] = pair.Key, ["turn"] = pair.Value }).ToArray()),
                ["happened_this_turn"] = current,
            };
            switch (entry)
            {
                case CardPlayStartedEntry started:
                    source["card"] = started.CardPlay.Card.Id.Entry;
                    source["card_type"] = (int)started.CardPlay.Card.Type;
                    source["player_net_id"] = started.CardPlay.Player.NetId;
                    if (current && ReferenceEquals(started.CardPlay.Player, player))
                    {
                        skillStarted |= started.CardPlay.Card.Type == CardType.Skill;
                        if (started.CardPlay.Card.Type == CardType.Attack) attackStarted++;
                        if (started.CardPlay.Card.Type is CardType.Attack or CardType.Skill) attackSkillStarted++;
                    }
                    break;
                case CardPlayFinishedEntry played:
                    source["card"] = played.CardPlay.Card.Id.Entry;
                    source["card_type"] = (int)played.CardPlay.Card.Type;
                    source["player_net_id"] = played.CardPlay.Player.NetId;
                    finished++;
                    if (current && ReferenceEquals(played.CardPlay.Player, player) && played.CardPlay.Card.Type == CardType.Attack) attackFinished++;
                    break;
                case CardExhaustedEntry removed:
                    source["card"] = removed.Card.Id.Entry;
                    exhausted++;
                    if (current && ReferenceEquals(removed.Actor, player.Creature)) exhaustedTurn++;
                    break;
                case CardAfflictedEntry afflicted:
                    source["card"] = afflicted.Card.Id.Entry;
                    source["affliction"] = afflicted.Affliction.Id.Entry;
                    if (current && ReferenceEquals(afflicted.Actor, player.Creature) && afflicted.Affliction is Bound) boundTurn++;
                    break;
                case DamageReceivedEntry damage:
                    source["unblocked_damage"] = damage.Result.UnblockedDamage;
                    if (ReferenceEquals(damage.Receiver, player.Creature) && damage.Result.UnblockedDamage > 0)
                    {
                        damageReceived++;
                        lostHp |= current;
                    }
                    break;
            }
            operands.Add(source);
        }
        if (attackFinished > short.MaxValue || damageReceived > short.MaxValue || exhausted > short.MaxValue || exhaustedTurn > short.MaxValue)
            throw new InvalidOperationException("live_combat_history_counter_capacity");
        fields["player_played_skill_this_turn"] = skillStarted;
        fields["cards_played_this_combat"] = finished;
        fields["player_attack_cards_this_turn"] = attackFinished;
        fields["player_attack_cards_started_this_turn"] = attackStarted;
        fields["player_attack_skill_cards_started_this_turn"] = attackSkillStarted;
        fields["player_damage_received_entries_this_combat"] = damageReceived;
        fields["player_lost_hp_this_turn"] = lostHp;
        fields["cards_exhausted_this_combat"] = exhausted;
        fields["player_bound_cards_afflicted_this_turn"] = boundTurn;
        fields["player_cards_exhausted_this_turn"] = exhaustedTurn;
        return operands;
    }
}
