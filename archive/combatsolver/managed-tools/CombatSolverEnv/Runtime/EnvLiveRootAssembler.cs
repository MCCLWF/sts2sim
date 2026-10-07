using System.Text.Json;
using System.Text.Json.Nodes;
using Sts2EnvCompare;

namespace CombatSolver;

internal sealed record EnvLiveRootComponents(EnvLiveCards Cards, EnvLiveMonsters Monsters,
    EnvLiveRunContext Run, EnvLiveCreatures Creatures, EnvLiveInventory Inventory,
    EnvLiveCombatContext Combat, EnvLiveRoomContext Room);

// Joins the independently frozen live components into the versioned Env state
// document. The blank template supplies only POD defaults and protocol shape;
// the schema session is consulted only for the current monster registry. Every
// mutable combat/run/pool component is overwritten by live capture before
// ImportSnapshot validates references and hashes.
internal static class EnvLiveRootAssembler
{
    internal static string Compose(EnvLiveRootComponents components, NativeEnv.Session schema, string blankEnvelope)
    {
        if (components.Cards is not { } cards
            || components.Monsters is not { } monsters
            || components.Run is not { } run
            || components.Creatures is not { } creatures
            || components.Inventory is not { } inventory
            || components.Combat is not { } combat
            || components.Room is not { } room)
            throw new InvalidOperationException("live_root_component_missing");
        JsonObject liveProfile = JsonNode.Parse(run.Component.GetProperty("profile").GetRawText())?.AsObject()
            ?? throw new InvalidOperationException("live_root_profile_missing");
        RequireKnownFields(liveProfile, ["all_unlocked"], "profile");
        // The old ProfileConfig is a seeded-new-run option. A live root carries
        // both actual UnlockState sources inside GameState; verify that the
        // summary and owned state agree before allowing snapshot import.
        JsonElement profileSources = run.Component.GetProperty("profile_source");
        bool veteran = true;
        foreach (string name in new[] { "run", "player" })
        {
            JsonElement source = profileSources.GetProperty(name);
            JsonElement stateProfile = run.Component.GetProperty(name == "run" ? "run" : "player").GetProperty("profile");
            bool compatible = source.GetProperty("number_of_runs").GetInt32() > 0
                && source.GetProperty("missing_epochs").GetArrayLength() == 0
                && source.GetProperty("missing_seen_encounters").GetArrayLength() == 0;
            if (source.GetProperty("full_veteran_compatible").GetBoolean() != compatible
                || !JsonElement.DeepEquals(source.GetProperty("state"), stateProfile)
                || stateProfile.GetProperty("full_veteran_compatible").GetBoolean() != compatible
                || stateProfile.GetProperty("number_of_runs").GetInt32() != source.GetProperty("number_of_runs").GetInt32())
                throw new InvalidOperationException("live_root_profile_source_inconsistent:" + name);
            veteran &= compatible;
        }
        if (liveProfile["all_unlocked"]!.GetValue<bool>() != veteran)
            throw new InvalidOperationException("live_root_profile_source_inconsistent:summary");

        JsonObject envelope = JsonNode.Parse(blankEnvelope)?.AsObject()
            ?? throw new InvalidOperationException("live_root_blank_template_json_invalid");
        if (envelope["format"]?.GetValue<string>() != "sts2env.blank_template"
            || envelope["version"]?.GetValue<int>() != 1
            || envelope["snapshot"] is not JsonObject)
            throw new InvalidOperationException("live_root_blank_template_format_mismatch");
        JsonObject document = envelope["snapshot"]!.AsObject();
        if (document["format"]?.GetValue<string>() != "sts2env.logical_state"
            || document["version"]?.GetValue<int>() != 2
            || document["decision"] is not null && document["decision"]!.GetValueKind() != JsonValueKind.Null
            || document["state_hash"] is not null && document["state_hash"]!.GetValueKind() != JsonValueKind.Null
            || document["pending"] is not null && document["pending"]!.GetValueKind() != JsonValueKind.Null
            || document["choice"] is not null && document["choice"]!.GetValueKind() != JsonValueKind.Null
            || document["replay_base"] is not null && document["replay_base"]!.GetValueKind() != JsonValueKind.Null
            || document["answers"] is not JsonArray answers || answers.Count != 0)
            throw new InvalidOperationException("live_root_blank_template_not_blank");
        JsonObject state = document["state"]?.AsObject()
            ?? throw new InvalidOperationException("live_root_blank_template_state_missing");
        RequireKnownFields(state, ["run", "combat", "player_combat", "cards", "powers", "creatures", "relics", "potions", "orbs", "rewards"], "state");
        JsonObject runState = state["run"]?.AsObject()
            ?? throw new InvalidOperationException("live_root_blank_template_run_missing");
        JsonObject playerState = runState["player"]?.AsObject()
            ?? throw new InvalidOperationException("live_root_blank_template_player_missing");
        JsonObject combatState = state["combat"]?.AsObject()
            ?? throw new InvalidOperationException("live_root_blank_template_combat_missing");
        JsonObject playerCombatState = state["player_combat"]?.AsObject()
            ?? throw new InvalidOperationException("live_root_blank_template_player_combat_missing");
        FieldCoverage runCoverage = new(runState, "run");
        FieldCoverage playerCoverage = new(playerState, "run.player");
        FieldCoverage combatCoverage = new(combatState, "combat");
        FieldCoverage playerCombatCoverage = new(playerCombatState, "player_combat");

        JsonElement liveRun = run.Component.GetProperty("run");
        JsonElement livePlayer = run.Component.GetProperty("player");
        JsonElement liveRoom = room.Component.GetProperty("run_fields");
        runCoverage.Merge(liveRun);
        // The room component independently proves these already-owned values.
        // It may validate them, but may not overwrite the run component.
        runCoverage.Merge(liveRoom, ["rooms", "current_act_index", "act_floor", "total_floor"]);
        playerCoverage.Merge(livePlayer);

        JsonElement liveCombat = combat.Component.GetProperty("combat_fields");
        JsonElement livePlayerCombat = combat.Component.GetProperty("player_combat_fields");
        combatCoverage.Merge(liveCombat);
        playerCombatCoverage.Merge(livePlayerCombat);

        JsonElement liveCards = cards.Component.GetProperty("cards");
        state["cards"] = JsonNode.Parse(liveCards.GetRawText());
        playerCoverage.Assign("deck", cards.Component.GetProperty("deck"));
        playerCombatCoverage.Assign("piles", cards.Component.GetProperty("piles"));

        JsonElement liveInventory = inventory.Component;
        state["relics"] = JsonNode.Parse(liveInventory.GetProperty("relics").GetRawText());
        state["potions"] = JsonNode.Parse(liveInventory.GetProperty("potions").GetRawText());
        state["orbs"] = JsonNode.Parse(liveInventory.GetProperty("orbs").GetRawText());
        playerCoverage.Merge(liveInventory.GetProperty("player_fields"));
        playerCombatCoverage.Merge(liveInventory.GetProperty("player_combat_fields"), ["orb_capacity"]);

        using JsonDocument monsterLayoutDocument = JsonDocument.Parse(schema.ExportMonsterMachinesJson());
        JsonElement monsterLayout = monsterLayoutDocument.RootElement;
        Dictionary<int, JsonElement> boundMonsters = monsters.Bind(monsterLayout);
        if (envelope["monster_default"] is null)
            throw new InvalidOperationException("live_root_player_monster_default_missing");
        using JsonDocument monsterDefaultDocument = JsonDocument.Parse(envelope["monster_default"]!.ToJsonString());
        JsonElement seedMonster = monsterDefaultDocument.RootElement;
        if (seedMonster.ValueKind is JsonValueKind.Undefined or JsonValueKind.Null
            || seedMonster.GetProperty("id").GetInt32() != 0)
            throw new InvalidOperationException("live_root_player_monster_default_missing");
        state["creatures"] = JsonNode.Parse(creatures.Bind(boundMonsters, seedMonster).GetRawText());
        state["powers"] = JsonNode.Parse(creatures.Component.GetProperty("powers").GetRawText());
        playerCoverage.Merge(creatures.Component.GetProperty("player_fields"));
        playerCombatCoverage.Merge(creatures.Component.GetProperty("player_combat_fields"), ["pets"]);
        combatCoverage.Merge(creatures.Component.GetProperty("combat_fields"),
            ["allies", "enemies", "escaped", "next_creature_id"]);

        // The ordinary-combat room contract explicitly freezes an empty reward
        // surface and the synchronizer. Keep these fields from the schema only
        // after asserting the captured contract says they are empty.
        JsonElement liveRewards = room.Component.GetProperty("rewards");
        if (liveRewards.GetProperty("active").GetBoolean()
            || liveRewards.GetProperty("reward_refs").GetArrayLength() != 0
            || room.Component.GetProperty("reward_pool").GetArrayLength() != 0)
            throw new InvalidOperationException("live_root_rewards_not_empty");
        if (room.Component.GetProperty("event_subscriptions").GetProperty("player_relic_obtained").GetBoolean())
            throw new InvalidOperationException("live_root_relic_subscription_pending");
        // These fields are deliberately not supplied by the live components.
        // The ordinary-room contract proves that no reward/event/rest/treasure
        // continuation is pending, so the only valid values here are the
        // seed-free POD defaults from the blank template.
        RequireBlankPool(state, "rewards");
        RequireBlankRewards(runState);
        RequireBlankEvents(runState, seedMonster.GetProperty("rng"), liveRoom.GetProperty("room_event").GetInt32());
        JsonElement finishedEvent = room.Component.GetProperty("synchronizer").GetProperty("events").GetProperty("event");
        if (finishedEvent.ValueKind != JsonValueKind.Null)
        {
            JsonObject value = JsonNode.Parse(finishedEvent.GetRawText())!.AsObject();
            RequireKnownFields(value, ["id", "began", "is_finished", "cleanup_called", "rng", "description", "options", "vars", "extra",
                "slippery_skipped_removals", "custom_done_page", "healed_amount"], "live.finished_event");
            runState["events"]![0] = value;
        }
        RequireBlankRoomStates(runState);
        // This is the complete allowance list. All other fields in these four
        // schema objects must have a unique live writer. Checking destination
        // keys also rejects a newly exported C++ field until it is mapped.
        foreach (string field in new[] { "rewards", "events", "rest_site", "treasure", "merchant" })
            runCoverage.AcceptValidatedDefault(field);
        runCoverage.Assign("combat_extra_rewards", new JsonArray());
        playerCoverage.RequireComplete();
        runCoverage.AcceptValidatedContainer("player"); // fully checked child container
        runCoverage.RequireComplete();
        combatCoverage.RequireComplete();
        playerCombatCoverage.RequireComplete();
        document["decision"] = 1; // DecisionKind.Combat

        // Import computes the canonical hash after this document is assembled.
        document["state_hash"] = null;
        document["pending"] = null;
        document["answers"] = new JsonArray();
        document["choice"] = null;
        document["replay_base"] = null;
        return document.ToJsonString(new JsonSerializerOptions { WriteIndented = false });
    }

    private sealed class FieldCoverage(JsonObject destination, string path)
    {
        private readonly HashSet<string> assigned = new(StringComparer.Ordinal);

        internal void Merge(JsonElement source, string[]? validateOnly = null)
        {
            if (source.ValueKind != JsonValueKind.Object)
                throw new InvalidOperationException("live_root_component_object_required:" + path);
            HashSet<string> validationFields = new(validateOnly ?? [], StringComparer.Ordinal);
            HashSet<string> validated = new(StringComparer.Ordinal);
            foreach (JsonProperty property in source.EnumerateObject())
            {
                if (!validationFields.Contains(property.Name))
                {
                    Assign(property.Name, property.Value);
                    continue;
                }
                if (!validated.Add(property.Name))
                    throw new InvalidOperationException("live_root_component_field_duplicate:" + path + "." + property.Name);
                if (!assigned.Contains(property.Name))
                    throw new InvalidOperationException("live_root_component_field_missing:" + path + "." + property.Name);
                if (!JsonNode.DeepEquals(destination[property.Name], JsonNode.Parse(property.Value.GetRawText())))
                    throw new InvalidOperationException("live_root_component_field_inconsistent:" + path + "." + property.Name);
            }
            foreach (string field in validationFields)
                if (!validated.Contains(field))
                    throw new InvalidOperationException("live_root_component_validation_field_missing:" + path + "." + field);
        }

        internal void Assign(string field, JsonElement value) => Assign(field, JsonNode.Parse(value.GetRawText()));

        internal void Assign(string field, JsonNode? value)
        {
            Mark(field);
            destination[field] = value;
        }

        internal void AcceptValidatedDefault(string field) => Mark(field);
        internal void AcceptValidatedContainer(string field) => Mark(field);

        private void Mark(string field)
        {
            if (!destination.ContainsKey(field))
                throw new InvalidOperationException("live_root_component_field_unmapped:" + path + "." + field);
            if (!assigned.Add(field))
                throw new InvalidOperationException("live_root_component_field_duplicate:" + path + "." + field);
        }

        internal void RequireComplete()
        {
            foreach (string field in destination.Select(pair => pair.Key))
                if (!assigned.Contains(field))
                    throw new InvalidOperationException("live_root_component_field_missing:" + path + "." + field);
        }
    }

    private static void RequireBlankPool(JsonObject state, string name)
    {
        JsonObject pool = state[name]?.AsObject()
            ?? throw new InvalidOperationException("live_root_blank_pool_missing:" + name);
        RequireKnownFields(pool, ["capacity", "next", "count", "slots"], "state." + name);
        if (pool["capacity"]?.GetValue<int>() != 15 || pool["next"]?.GetValue<int>() != 1
            || pool["count"]?.GetValue<int>() != 0 || pool["slots"] is not JsonArray slots || slots.Count != 0)
            throw new InvalidOperationException("live_root_blank_pool_not_empty:" + name);
    }

    private static void RequireBlankRewards(JsonObject run)
    {
        JsonObject rewards = run["rewards"]?.AsObject()
            ?? throw new InvalidOperationException("live_root_blank_rewards_missing");
        RequireKnownFields(rewards, ["active", "room", "allow_empty", "disallow_skipping", "generated", "rewards"], "run.rewards");
        if (rewards["active"]?.GetValue<bool>() != false
            || rewards["room"]?.GetValue<int>() != 0
            || rewards["allow_empty"]?.GetValue<bool>() != false
            || rewards["disallow_skipping"]?.GetValue<bool>() != false
            || rewards["generated"]?.GetValue<bool>() != false
            || rewards["rewards"] is not JsonArray refs || refs.Count != 0)
            throw new InvalidOperationException("live_root_blank_rewards_not_empty");
    }

    private static void RequireBlankEvents(JsonObject run, JsonElement defaultRng, int canonicalEvent)
    {
        if (run["room_event"]?.GetValue<int>() != canonicalEvent
            || run["events"] is not JsonArray events || events.Count != 2)
            throw new InvalidOperationException("live_root_blank_events_not_empty");
        foreach (JsonNode? node in events)
        {
            if (node is not JsonObject item) throw new InvalidOperationException("live_root_blank_events_not_empty");
            RequireKnownFields(item, ["id", "began", "is_finished", "cleanup_called", "rng", "description", "options", "vars", "extra",
                "slippery_skipped_removals", "custom_done_page", "healed_amount"], "run.events");
            if (item["id"]?.GetValue<int>() != 0 || item["began"]?.GetValue<bool>() != false
                || item["is_finished"]?.GetValue<bool>() != false || item["cleanup_called"]?.GetValue<bool>() != false
                || !JsonNode.DeepEquals(item["rng"], JsonNode.Parse(defaultRng.GetRawText()))
                || item["description"]?.GetValue<uint>() != 0
                || item["options"] is not JsonArray options || options.Count != 0
                || item["vars"] is not JsonArray vars || vars.Count != 12 || vars.Any(value => !IsZeroDecimal(value))
                || item["extra"] is not JsonArray extra || extra.Count != 8 || extra.Any(value => value?.GetValue<int>() != 0)
                || item["slippery_skipped_removals"] is not JsonArray skipped || skipped.Count != 0
                || item["custom_done_page"]?.GetValue<uint>() != 0 || item["healed_amount"]?.GetValue<int>() != 0)
                throw new InvalidOperationException("live_root_blank_events_not_empty");
        }
    }

    private static void RequireBlankRoomStates(JsonObject run)
    {
        JsonObject rest = run["rest_site"]?.AsObject()
            ?? throw new InvalidOperationException("live_root_blank_rest_site_missing");
        JsonObject treasure = run["treasure"]?.AsObject()
            ?? throw new InvalidOperationException("live_root_blank_treasure_missing");
        JsonObject merchant = run["merchant"]?.AsObject()
            ?? throw new InvalidOperationException("live_root_blank_merchant_missing");
        RequireKnownFields(rest, ["options", "completed", "last_chosen", "proceed_enabled"], "run.rest_site");
        RequireKnownFields(treasure, ["picking", "current_relics", "singleplayer_skipped"], "run.treasure");
        RequireKnownFields(merchant, ["exists", "character_cards", "colorless_cards", "relics", "potions", "has_card_removal", "card_removal_used", "card_removal_cost"], "run.merchant");
        if (rest["options"] is not JsonArray options || options.Count != 0
            || rest["completed"]?.GetValue<bool>() != false
            || rest["last_chosen"]?.GetValue<int>() != -1
            || rest["proceed_enabled"]?.GetValue<bool>() != false
            || treasure["picking"]?.GetValue<bool>() != false
            || treasure["current_relics"] is not JsonArray relics || relics.Count != 0
            || treasure["singleplayer_skipped"]?.GetValue<bool>() != false
            || merchant["exists"]?.GetValue<bool>() != false
            || merchant["character_cards"] is not JsonArray characterCards || characterCards.Count != 0
            || merchant["colorless_cards"] is not JsonArray colorlessCards || colorlessCards.Count != 0
            || merchant["relics"] is not JsonArray merchantRelics || merchantRelics.Count != 0
            || merchant["potions"] is not JsonArray merchantPotions || merchantPotions.Count != 0
            || merchant["has_card_removal"]?.GetValue<bool>() != false
            || merchant["card_removal_used"]?.GetValue<bool>() != false
            || merchant["card_removal_cost"]?.GetValue<int>() != 0)
            throw new InvalidOperationException("live_root_blank_room_state_not_empty");
    }

    private static bool IsZeroDecimal(JsonNode? value)
        => value is JsonObject item && item.Count == 1 && item["decimal_bits"] is JsonArray bits
            && bits.Count == 4 && bits.All(bit => bit?.GetValue<uint>() == 0);

    private static void RequireKnownFields(JsonObject value, string[] fields, string path)
    {
        HashSet<string> known = new(fields, StringComparer.Ordinal);
        foreach (string field in value.Select(pair => pair.Key))
            if (!known.Contains(field))
                throw new InvalidOperationException("live_root_default_field_unmapped:" + path + "." + field);
        foreach (string field in known)
            if (!value.ContainsKey(field))
                throw new InvalidOperationException("live_root_default_field_missing:" + path + "." + field);
    }
}
