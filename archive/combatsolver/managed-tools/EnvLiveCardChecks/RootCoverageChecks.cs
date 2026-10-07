using System.Text.Json;
using System.Text.Json.Nodes;
using CombatSolver;
using Sts2EnvCompare;

// Mutations of an actually captured production root. Each refusal prevents a
// concrete blank fallback, conflicting owner, or unsupported schema field.
internal static class RootCoverageChecks
{
    internal static void Run(EnvLiveRootComponents components, NativeEnv.Session schema, string blank)
    {
        void Refuses(EnvLiveRootComponents changed, string reason, string? changedBlank = null)
        {
            try
            {
                _ = EnvLiveRootAssembler.Compose(changed, schema, changedBlank ?? blank);
                throw new Exception("root coverage accepted missing or ambiguous state:" + reason);
            }
            catch (InvalidOperationException error) when (error.Message == reason) { }
        }

        Refuses(components with { Run = new(Change(components.Run.Component, root => root["run"]!.AsObject().Remove("odds"))) },
            "live_root_component_field_missing:run.odds");
        Refuses(components with { Run = new(Change(components.Run.Component, root => root["player"]!.AsObject().Remove("gold"))) },
            "live_root_component_field_missing:run.player.gold");
        Refuses(components with { Combat = new(Change(components.Combat.Component, root => root["combat_fields"]!.AsObject().Remove("player_lost_hp_this_turn"))) },
            "live_root_component_field_missing:combat.player_lost_hp_this_turn");
        Refuses(components with { Combat = new(Change(components.Combat.Component, root => root["player_combat_fields"]!.AsObject().Remove("stars"))) },
            "live_root_component_field_missing:player_combat.stars");
        Refuses(components with { Inventory = new(Change(components.Inventory.Component,
            root => root["player_fields"]!["gold"] = components.Run.Component.GetProperty("player").GetProperty("gold").GetInt32())) },
            "live_root_component_field_duplicate:run.player.gold");
        Refuses(components with { Room = new(Change(components.Room.Component,
            root => root["run_fields"]!["current_act_index"] = components.Run.Component.GetProperty("run").GetProperty("current_act_index").GetInt32() + 1)) },
            "live_root_component_field_inconsistent:run.current_act_index");
        JsonObject newSchemaField = JsonNode.Parse(blank)!.AsObject();
        newSchemaField["snapshot"]!["state"]!["combat"]!["future_mutable_field"] = 0;
        Refuses(components, "live_root_component_field_missing:combat.future_mutable_field", newSchemaField.ToJsonString());
        JsonObject newRewardField = JsonNode.Parse(blank)!.AsObject();
        newRewardField["snapshot"]!["state"]!["run"]!["rewards"]!["future_field"] = 0;
        Refuses(components, "live_root_default_field_unmapped:run.rewards.future_field", newRewardField.ToJsonString());
        JsonObject newStateField = JsonNode.Parse(blank)!.AsObject();
        newStateField["snapshot"]!["state"]!["future_field"] = 0;
        Refuses(components, "live_root_default_field_unmapped:state.future_field", newStateField.ToJsonString());
        Refuses(components with { Run = new(Change(components.Run.Component, root => root["profile"]!["future_field"] = 0)) },
            "live_root_default_field_unmapped:profile.future_field");
        Refuses(components with { Run = new(Change(components.Run.Component,
            root => root["profile_source"]!["run"]!["full_veteran_compatible"] = false)) },
            "live_root_profile_source_inconsistent:run");
        Refuses(components with { Run = new(Change(components.Run.Component,
            root => root["run"]!["profile"]!["unlocked_epoch_bits"] = 0UL)) },
            "live_root_profile_source_inconsistent:run");
        JsonObject pendingRoomState = JsonNode.Parse(blank)!.AsObject();
        pendingRoomState["snapshot"]!["state"]!["run"]!["treasure"]!["singleplayer_skipped"] = true;
        Refuses(components, "live_root_blank_room_state_not_empty", pendingRoomState.ToJsonString());
        Console.WriteLine("live root coverage: four required-field omissions, duplicate writer, conflicting component, new combat/reward/state/profile fields, inconsistent profile proof/bitset and nonblank room default rejected");
    }

    private static JsonElement Change(JsonElement component, Action<JsonObject> change)
    {
        JsonObject root = JsonNode.Parse(component.GetRawText())!.AsObject();
        change(root);
        return JsonSerializer.SerializeToElement(root);
    }
}
