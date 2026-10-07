using System.Collections;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using System.Text.Json;
using System.Text.Json.Nodes;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Events;
using MegaCrit.Sts2.Core.Rewards;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;

namespace CombatSolver;

// The room boundary is deliberately narrow. It proves the ordinary, single
// combat room contract and transfers only fields whose future reads are known.
// A pending reward, nested room, or synchronizer message rejects Env before a
// branch can use this component.
internal sealed record EnvLiveRoomContext(JsonElement Component)
{
    internal static EnvLiveRoomContext Capture(RunState run, Player player, CombatState combat)
    {
        EnvModelCatalog.RequireGameVersion();
        if (run.GetType() != typeof(RunState) || run.Players.Count != 1 || !ReferenceEquals(run.Players[0], player))
            throw new InvalidOperationException("live_room_require_single_player_run");
        if (run.CurrentRoomCount != 1 || run.CurrentRoom is not CombatRoom room || !ReferenceEquals(room.CombatState, combat)
            || !ReferenceEquals(run.BaseRoom, room))
            throw new InvalidOperationException("live_room_requires_single_idle_combat_room");
        if (room.RoomType is not (RoomType.Monster or RoomType.Elite or RoomType.Boss))
            throw new InvalidOperationException("live_room_type_unmapped");
        // A combat nested under an EventRoom can have the same RoomType, but
        // its post-combat continuation and extra reward closures are not in
        // the ordinary RunData contract.  Keep this boundary unambiguous.
        if (room.ParentEventId is not null || !room.ShouldCreateCombat)
            throw new InvalidOperationException("live_room_nested_event_combat_unmapped");
        if (room.IsPreFinished || run.IsGameOver)
            throw new InvalidOperationException("live_room_not_active_combat");
        if (room.ExtraRewards.Any(pair => pair.Value.Count != 0))
            throw new InvalidOperationException("live_combat_extra_rewards_pending");
        if (room.ExtraRewards.Keys.Any(owner => !ReferenceEquals(owner, player)))
            throw new InvalidOperationException("live_combat_extra_rewards_foreign_player");

        JsonObject relicSubscriptions = CaptureRelicSubscriptions(player);

        JsonObject synchronizer = CaptureSynchronizers(player);
        JsonObject component = new()
        {
            ["run_fields"] = new JsonObject
            {
                // Env RunPhase.Combat is the third enum value (None, Map,
                // Combat); the game has no corresponding RunPhase property.
                ["phase"] = 2,
                ["rooms"] = new JsonArray((int)room.RoomType),
                ["room_event"] = synchronizer["events"]!["state"]!["canonical_event"]!.DeepClone(),
                ["event_synchronizer"] = synchronizer["events"]!["state"]!.DeepClone(),
                ["current_act_index"] = run.CurrentActIndex,
                ["act_floor"] = run.ActFloor,
                ["total_floor"] = run.TotalFloor,
            },
            ["room"] = new JsonObject
            {
                ["current_room_count"] = run.CurrentRoomCount,
                ["is_game_over"] = run.IsGameOver,
                ["room_type"] = (int)room.RoomType,
                ["encounter"] = EnvModelCatalog.Id(room.Encounter, "encounters"),
                ["gold_proportion"] = new JsonObject { ["f32_bits"] = BitConverter.SingleToUInt32Bits(room.GoldProportion) },
                ["is_pre_finished"] = room.IsPreFinished,
                ["should_create_combat"] = room.ShouldCreateCombat,
                ["should_resume_parent_event"] = room.ShouldResumeParentEventAfterCombat,
                ["parent_event"] = room.ParentEventId is { } parent ? parent.Entry : null,
                ["extra_rewards"] = new JsonArray(),
            },
            ["rewards"] = new JsonObject
            {
                ["active"] = false, ["room"] = 0, ["allow_empty"] = false,
                ["disallow_skipping"] = false, ["generated"] = false, ["reward_refs"] = new JsonArray(),
            },
            ["reward_pool"] = new JsonArray(),
            ["synchronizer"] = synchronizer,
            ["event_subscriptions"] = relicSubscriptions,
            ["source"] = "live_ordinary_combat_room_idle_boundary",
            ["full_root"] = false,
        };
        return new(JsonSerializer.SerializeToElement(component));
    }

    private static JsonObject CaptureRelicSubscriptions(Player player)
    {
        FieldInfo? relicEvent = player.GetType().GetFields(BindingFlags.Instance | BindingFlags.NonPublic)
            .FirstOrDefault(field => typeof(Delegate).IsAssignableFrom(field.FieldType)
                && field.Name.Contains("RelicObtained", StringComparison.Ordinal));
        if (relicEvent?.GetValue(player) is not Delegate pending)
        {
            return new JsonObject { ["player_relic_obtained"] = false, ["subscriber_count"] = 0,
                ["subscriber_kinds"] = new JsonArray() };
        }

        JsonArray kinds = new();
        int cardRewardCount = 0;
        int unknownCount = 0;
        foreach (Delegate subscriber in pending.GetInvocationList())
        {
            Type? targetType = subscriber.Target?.GetType();
            string typeName = targetType?.FullName ?? "<static>";
            string kind;
            if (targetType is not null && typeof(CardReward).IsAssignableFrom(targetType))
            {
                kind = "card_reward";
                cardRewardCount++;
            }
            else if (IsKnownPresentationSubscriber(typeName))
            {
                kind = "presentation";
            }
            else
            {
                kind = "unknown";
                unknownCount++;
            }
            kinds.Add(new JsonObject { ["kind"] = kind, ["target"] = typeName,
                ["method"] = subscriber.Method.Name });
        }

        // CardReward subscribes before it is displayed and removes the
        // subscription only when selected or skipped.  Unknown subscribers
        // may carry gameplay effects too, so neither category may be dropped.
        if (cardRewardCount != 0)
            throw new InvalidOperationException($"live_reward_card_subscription_pending:{cardRewardCount}");
        if (unknownCount != 0)
            throw new InvalidOperationException($"live_reward_relic_subscription_unknown:{unknownCount}");

        return new JsonObject { ["player_relic_obtained"] = false,
            ["subscriber_count"] = kinds.Count, ["subscriber_kinds"] = kinds };
    }

    private static bool IsKnownPresentationSubscriber(string typeName)
        => typeName is "MegaCrit.Sts2.Core.Nodes.Multiplayer.NMultiplayerPlayerState"
            or "MegaCrit.Sts2.Core.Nodes.Relics.NRelicInventory"
            or "MegaCrit.Sts2.Core.Nodes.CommonUi.NTopBar"
            or "MegaCrit.Sts2.Core.Nodes.Potions.NPotionContainer";

    private static JsonObject CaptureSynchronizers(Player player)
    {
        RunManager manager;
        try
        {
            manager = MegaCrit.Sts2.Core.Runs.RunManager.Instance
                ?? throw new InvalidOperationException("live_synchronizers_missing");
        }
        catch (NullReferenceException)
        {
            throw new InvalidOperationException("live_synchronizers_missing");
        }
        JsonObject rewards = CaptureRewardsSynchronizer(manager.RewardsSetSynchronizer, player);
        JsonObject rewardMessages = CaptureRewardSynchronizer(manager.RewardSynchronizer);
        JsonObject events = CaptureEventSynchronizer(manager.EventSynchronizer, player);
        JsonObject restSite = CaptureRestSiteSynchronizer(manager.RestSiteSynchronizer);
        JsonObject treasure = CaptureTreasureSynchronizer(manager.TreasureRoomRelicSynchronizer);
        return new JsonObject { ["rewards"] = rewards, ["reward_messages"] = rewardMessages,
            ["events"] = events, ["rest_site"] = restSite, ["treasure"] = treasure };
    }

    private static JsonObject CaptureRewardsSynchronizer(object sync, Player player)
    {
        Type stateType = sync.GetType();
        FieldInfo statesField = stateType.GetField("_rewardStates", BindingFlags.Instance | BindingFlags.NonPublic)
            ?? throw new InvalidOperationException("live_rewards_synchronizer_layout_unmapped");
        if (statesField.GetValue(sync) is not System.Collections.IList states || states.Count != 1)
            throw new InvalidOperationException("live_rewards_synchronizer_player_count");
        object state = states[0]!;
        Type itemType = state.GetType();
        object Value(string name) => itemType.GetField(name, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic)?.GetValue(state)
            ?? throw new InvalidOperationException("live_rewards_synchronizer_field_unmapped:" + name);
        if (Value("rewardsStack") is not System.Collections.ICollection stack || stack.Count != 0)
            throw new InvalidOperationException("live_rewards_stack_pending");
        if (Value("bufferedMessages") is not System.Collections.ICollection buffered || buffered.Count != 0)
            throw new InvalidOperationException("live_rewards_buffered_messages_pending");
        if (Value("completedRewards") is not System.Collections.IDictionary completed || completed.Count != 0)
            throw new InvalidOperationException("live_rewards_completed_bookkeeping_pending");
        int nextId = Value("nextId") is int id ? id : throw new InvalidOperationException("live_rewards_next_id_unmapped");
        if (nextId != 0) throw new InvalidOperationException("live_rewards_next_id_nonzero");
        return new() { ["player_net_id"] = player.NetId, ["stack_count"] = 0, ["buffered_count"] = 0,
            ["completed_count"] = 0, ["next_id"] = nextId };
    }

    private static JsonObject CaptureRewardSynchronizer(object sync)
    {
        object? buffered = Field(sync, "_bufferedMessages");
        int count = CollectionCount(buffered, "live_reward_synchronizer_buffer_layout_unmapped");
        if (count != 0) throw new InvalidOperationException("live_reward_messages_pending:" + count);
        return new JsonObject { ["buffered_count"] = count };
    }

    private static JsonObject CaptureEventSynchronizer(object sync, Player player)
    {
        int unfinished = 0;
        object events = RequiredField(sync, "_events", "live_event_synchronizer_layout_unmapped");
        int eventCount = CollectionCount(events, "live_event_synchronizer_events_unmapped");
        if (eventCount > 1) throw new InvalidOperationException("live_event_bookkeeping_pending:" + eventCount);
        JsonObject? liveEvent = null;
        if (events is IEnumerable enumerable)
        {
            foreach (object? item in enumerable)
            {
                if (item is null)
                    throw new InvalidOperationException("live_event_bookkeeping_pending:" + eventCount);
                if (item is not Neow neow || neow.GetType() != typeof(Neow))
                    throw new InvalidOperationException("live_finished_event_model_unmapped");
                if (!neow.IsFinished) unfinished++;
                else liveEvent = CaptureFinishedNeow(neow, player);
            }
        }
        int pendingTasks = CollectionCount(RequiredField(sync, "_pendingOptionTasks",
            "live_event_synchronizer_tasks_unmapped"), "live_event_synchronizer_tasks_unmapped");
        if (unfinished != 0) throw new InvalidOperationException("live_event_pending:" + unfinished);
        if (pendingTasks != 0) throw new InvalidOperationException("live_event_option_tasks_pending:" + pendingTasks);
        object votes = RequiredField(sync, "_playerVotes", "live_event_synchronizer_votes_unmapped");
        int voteCount = CollectionCount(votes, "live_event_synchronizer_votes_unmapped");
        object page = RequiredField(sync, "_pageIndex", "live_event_synchronizer_page_unmapped");
        if (page is not uint pageIndex) throw new InvalidOperationException("live_event_synchronizer_page_unmapped");
        if (voteCount > 1 || pageIndex != 0
            || eventCount == 0 && voteCount != 0)
            throw new InvalidOperationException("live_event_bookkeeping_pending");
        int activeVotes = 0;
        if (votes is IEnumerable voteValues)
            foreach (object? vote in voteValues) if (vote is not null) activeVotes++;
        if (activeVotes != 0) throw new InvalidOperationException("live_event_votes_pending:" + activeVotes);
        object? canonical = Field(sync, "_canonicalEvent");
        int canonicalId = 0;
        if (eventCount == 1)
        {
            if (canonical is not Neow neow || neow.IsMutable || voteCount != 1
                || !ReferenceEquals(neow, ModelDb.Event<Neow>()))
                throw new InvalidOperationException("live_event_canonical_or_vote_binding_unmapped");
            canonicalId = EnvModelCatalog.Id(neow, "events");
        }
        else if (canonical is not null) throw new InvalidOperationException("live_event_canonical_without_instance");

        object combatSync = RequiredField(sync, "_combatSynchronizer", "live_event_combat_sync_unmapped");
        object states = RequiredField(combatSync, "_states", "live_event_combat_states_unmapped");
        int readyStates = 0;
        if (states is IEnumerable stateValues)
            foreach (object? state in stateValues) if (state is not null) readyStates++;
        if (readyStates != 0 || Field(combatSync, "_canonicalEvent") is not null
            || PropertyValue(combatSync, "MutableEncounterForLayout") is not null
            || PropertyValue(combatSync, "CombatStateForLayout") is not null)
            throw new InvalidOperationException("live_event_combat_pending");

        return new JsonObject { ["event_count"] = eventCount, ["unfinished_count"] = unfinished,
            ["pending_tasks"] = pendingTasks, ["vote_count"] = voteCount, ["active_votes"] = activeVotes,
            ["event_combat_ready_states"] = readyStates, ["event"] = liveEvent,
            ["state"] = new JsonObject { ["event_count"] = eventCount, ["canonical_event"] = canonicalId,
                ["has_player_vote_slot"] = voteCount == 1, ["has_player_vote"] = false,
                ["player_vote"] = 0, ["page_index"] = pageIndex } };
    }

    private static JsonObject CaptureFinishedNeow(Neow neow, Player player)
    {
        object? Read(Type type, string name) => type.GetField(name,
            BindingFlags.Instance | BindingFlags.NonPublic)?.GetValue(neow)
            ?? throw new InvalidOperationException("live_finished_neow_field_unmapped:" + name);
        if (!neow.IsMutable || !ReferenceEquals(neow.Owner, player)
            || Read(typeof(EventModel), "_cleanupCalled") is not true
            || neow.CurrentOptions.Count != 0 || neow.DynamicVars.Count != 0
            || neow.CanonicalEncounter is not null)
            throw new InvalidOperationException("live_finished_neow_state_unmapped");
        object? NullableRead(Type type, string name)
        {
            FieldInfo field = type.GetField(name, BindingFlags.Instance | BindingFlags.NonPublic)
                ?? throw new InvalidOperationException("live_finished_neow_field_unmapped:" + name);
            return field.GetValue(neow);
        }
        if (NullableRead(typeof(Neow), "_modifierOptions") is ICollection modifiers && modifiers.Count != 0
            || NullableRead(typeof(AncientEventModel), "_debugOption") is not null)
            throw new InvalidOperationException("live_finished_neow_modifiers_or_debug_unmapped");
        if (neow.Rng is null) throw new InvalidOperationException("live_finished_neow_rng_missing");
        JsonObject rng = EnvLiveRunContext.RngValue(neow.Rng);
        uint Hash(string? key)
        {
            if (key is null) return 0;
            uint hash = 2166136261;
            foreach (byte value in Encoding.UTF8.GetBytes(key)) hash = unchecked((hash ^ value) * 16777619);
            return hash == 0 ? 1 : hash;
        }
        return new JsonObject { ["id"] = EnvModelCatalog.Id(neow, "events"), ["began"] = true,
            ["is_finished"] = true, ["cleanup_called"] = true, ["rng"] = rng,
            ["description"] = Hash(neow.Description?.LocEntryKey), ["options"] = new JsonArray(),
            ["vars"] = new JsonArray(Enumerable.Range(0, 12).Select(_ => (JsonNode)new JsonObject {
                ["decimal_bits"] = new JsonArray(0, 0, 0, 0) }).ToArray()),
            ["extra"] = new JsonArray(Enumerable.Range(0, 8).Select(_ => (JsonNode)JsonValue.Create(0)!).ToArray()),
            ["slippery_skipped_removals"] = new JsonArray(),
            ["custom_done_page"] = Hash((string?)NullableRead(typeof(AncientEventModel), "_customDonePage")),
            ["healed_amount"] = neow.HealedAmount };
    }

    private static JsonObject CaptureRestSiteSynchronizer(object sync)
    {
        object sites = RequiredField(sync, "_restSites", "live_rest_site_layout_unmapped");
        int siteCount = CollectionCount(sites, "live_rest_site_layout_unmapped");
        if (siteCount != 0) throw new InvalidOperationException("live_rest_site_bookkeeping_pending:" + siteCount);
        int optionCount = 0;
        int incomplete = 0;
        if (sites is IEnumerable siteValues)
        {
            foreach (object? site in siteValues)
            {
                if (site is null) { incomplete++; continue; }
                optionCount += CollectionCount(RequiredField(site, "options", "live_rest_site_options_unmapped"),
                    "live_rest_site_options_unmapped");
                object completion = RequiredField(site, "completionTaskSource", "live_rest_site_completion_unmapped");
                if (PropertyValue(completion, "Task") is not Task completionTask || !completionTask.IsCompleted) incomplete++;
            }
        }
        object? hovered = Field(sync, "_hoveredMessage");
        object? hoverTask = Field(sync, "_hoverMessageTask");
        if (optionCount != 0) throw new InvalidOperationException("live_rest_site_options_pending:" + optionCount);
        if (incomplete != 0) throw new InvalidOperationException("live_rest_site_completion_pending:" + incomplete);
        if (hovered is not null) throw new InvalidOperationException("live_rest_site_hover_pending");
        if (hoverTask is Task hoverCompletionTask && !hoverCompletionTask.IsCompleted) throw new InvalidOperationException("live_rest_site_hover_task_pending");
        return new JsonObject { ["site_count"] = siteCount, ["option_count"] = optionCount,
            ["incomplete_count"] = incomplete, ["hover_task"] = hoverTask is not null };
    }

    private static JsonObject CaptureTreasureSynchronizer(object sync)
    {
        object? current = Field(sync, "_currentRelics");
        object? predicted = Field(sync, "_predictedVote");
        object? skipped = Field(sync, "_singleplayerSkipped");
        if (current is not null) throw new InvalidOperationException("live_treasure_relic_picking_pending");
        if (predicted is not null) throw new InvalidOperationException("live_treasure_vote_pending");
        if (skipped is true) throw new InvalidOperationException("live_treasure_skip_pending");
        int votes = CollectionCount(RequiredField(sync, "_votes", "live_treasure_votes_unmapped"),
            "live_treasure_votes_unmapped");
        if (votes != 0) throw new InvalidOperationException("live_treasure_bookkeeping_pending:" + votes);
        return new JsonObject { ["picking"] = false, ["current_relic_count"] = 0,
            ["vote_count"] = votes, ["singleplayer_skipped"] = false };
    }

    private static object RequiredField(object owner, string name, string error)
        => Field(owner, name) ?? throw new InvalidOperationException(error);

    private static object? Field(object owner, string name)
        => owner.GetType().GetField(name, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic)?.GetValue(owner);

    private static object? PropertyValue(object owner, string name)
        => owner.GetType().GetProperty(name, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic)?.GetValue(owner);

    private static bool PropertyBool(object owner, string name, string error)
        => PropertyValue(owner, name) is bool value ? value : throw new InvalidOperationException(error);

    private static int CollectionCount(object? value, string error)
        => value is ICollection collection ? collection.Count : throw new InvalidOperationException(error);
}
