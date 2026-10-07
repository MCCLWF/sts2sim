using System.Text.Json;
using System.Text.Json.Nodes;
using CombatSolver;
using CombatSolverEnv.Api;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Orbs;
using MegaCrit.Sts2.Core.Models.Potions;
using MegaCrit.Sts2.Core.Models.Relics;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Unlocks;
using Sts2EnvCompare;

internal static class LiveInventoryChecks
{
    internal static void Run()
    {
        Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all, 1);
        RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(acts => acts[0].ToMutable()).ToList(),
            [], GameMode.Standard, 0, "DUAL001");
        CombatState combat = new(runState: run);
        combat.AddPlayer(player);
        player.PlayerCombatState = new(player);
        RelicModel vajra = ModelDb.Relic<Vajra>().ToMutable();
        player.AddRelicInternal(vajra, silent: true);
        vajra.IsWax = true;
        vajra.IsMelted = true;
        vajra.Status = MegaCrit.Sts2.Core.Entities.Relics.RelicStatus.Disabled;
        vajra.FloorAddedToDeck = 19;
        vajra.DynamicVars["StrengthPower"].BaseValue = 1.2500m;
        PotionModel fire = ModelDb.Potion<FirePotion>().ToMutable();
        PotionModel strength = ModelDb.Potion<StrengthPotion>().ToMutable();
        player.AddPotionInternal(fire, 2, silent: true);
        player.AddPotionInternal(strength, 0, silent: true);
        var queue = player.PlayerCombatState.OrbQueue;
        queue.AddCapacity(3);
        DarkOrb dark = (DarkOrb)ModelDb.Orb<DarkOrb>().ToMutable();
        GlassOrb glass = (GlassOrb)ModelDb.Orb<GlassOrb>().ToMutable();
        dark.Owner = player; glass.Owner = player;
        dark._evokeVal = 19.2500m; glass._passiveVal = 2.50m;
        queue.Insert(0, dark); queue.Insert(1, glass);
        EnvLiveInventory.Warmup(player);
        EnvLiveInventory captured = EnvLiveInventory.Capture(player);
        string frozen = captured.Component.GetRawText();
        JsonElement fields = captured.Component.GetProperty("player_fields");
        Require(fields.GetProperty("potions")[1].GetInt32() == 0, "empty potion slot was compressed");
        using NativeEnv.Session seeded = NativeEnv.OpenCombat("DUAL001", "CULTISTS_NORMAL");
        JsonObject document = JsonNode.Parse(seeded.ExportSnapshotJson())!.AsObject();
        JsonObject state = document["state"]!.AsObject();
        // Explicit partial-component fixture, all combat/card/creature context
        // remains seeded. Restoring active inventory does not certify a root.
        foreach (string name in new[] { "relics", "potions", "orbs" })
            state[name] = JsonNode.Parse(captured.Component.GetProperty(name).GetRawText());
        foreach (JsonProperty field in fields.EnumerateObject())
            state["run"]!["player"]![field.Name] = JsonNode.Parse(field.Value.GetRawText());
        foreach (JsonProperty field in captured.Component.GetProperty("player_combat_fields").EnumerateObject())
            state["player_combat"]![field.Name] = JsonNode.Parse(field.Value.GetRawText());
        document["state_hash"] = null;
        using NativeEnv.Session restored = NativeEnv.ImportCombatSnapshot(document.ToJsonString());
        using JsonDocument output = JsonDocument.Parse(restored.ExportSnapshotJson());
        foreach (string name in new[] { "relics", "potions", "orbs" })
            Require(JsonElement.DeepEquals(captured.Component.GetProperty(name), output.RootElement.GetProperty("state").GetProperty(name)), "inventory restore changed:" + name);
        JsonElement restoredPcs = output.RootElement.GetProperty("state").GetProperty("player_combat");
        Require(restoredPcs.GetProperty("orb_capacity").GetInt32() == queue.Capacity && restoredPcs.GetProperty("orbs").GetArrayLength() == 2,
            "orb queue state lost");
        JsonElement enemy = output.RootElement.GetProperty("state").GetProperty("creatures").GetProperty("slots")[1].GetProperty("value");
        int expectedHp = enemy.GetProperty("current_hp").GetInt32() - decimal.ToInt32(fire.DynamicVars["Damage"].BaseValue);
        ulong before = restored.StateHash;
        using NativeEnv.Session used = restored.Predict(new EnvAction(10, 2, 1));
        using JsonDocument usedOutput = JsonDocument.Parse(used.ExportSnapshotJson());
        var usedState = usedOutput.RootElement.GetProperty("state");
        Require(usedState.GetProperty("creatures").GetProperty("slots")[1].GetProperty("value").GetProperty("current_hp").GetInt32() == expectedHp,
            "restored potion slot did not drive FirePotion damage");
        Require(usedState.GetProperty("run").GetProperty("player").GetProperty("potions")[2].GetInt32() == 0 && restored.StateHash == before,
            "potion prediction lost ownership or mutated source");
        dark._evokeVal = 99; glass._passiveVal = 0;
        vajra.DynamicVars["StrengthPower"].BaseValue = 9;
        Require(captured.Component.GetRawText() == frozen, "live inventory changed owned component");
        fire.DynamicVars["Damage"].BaseValue = 21;
        try { EnvLiveInventory.Capture(player); throw new Exception("modified potion table variable accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("potion_variable_modified")) { }
        fire.DynamicVars["Damage"].BaseValue = 20;
        player.AddRelicInternal(ModelDb.Relic<OrnamentalFan>().ToMutable(), silent: true);
        EnvLiveInventory.Warmup(player);
        try { EnvLiveInventory.Capture(player); throw new Exception("unknown relic instance counters accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("relic_subclass_fields_unmapped")) { }
        Console.WriteLine("live inventory component: exact relic/orb values, ordered potion holes, owned C++ restore, pure potion prediction and explicit variable/counter rejection passed; full_root=false");
    }
    private static void Require(bool value, string reason) { if (!value) throw new InvalidOperationException(reason); }
}
