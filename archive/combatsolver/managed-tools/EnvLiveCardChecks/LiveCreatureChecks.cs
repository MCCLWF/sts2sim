using System.Reflection;
using System.Text.Json;
using System.Text.Json.Nodes;
using CombatSolver;
using CombatSolverEnv.Api;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Powers;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Unlocks;
using MegaCrit.Sts2.Core.ValueProps;
using Sts2EnvCompare;

internal static class LiveCreatureChecks
{
    internal static void Run()
    {
        Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all, 1);
        RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(acts => acts[0].ToMutable()).ToList(),
            [], GameMode.Standard, 0, "DUAL001");
        EncounterModel encounter = ModelDb.GetById<EncounterModel>(new("ENCOUNTER", "CULTISTS_NORMAL")).ToMutable();
        CombatState combat = new(encounter, run);
        combat.AddPlayer(player);
        player.PlayerCombatState = new(player);
        foreach (string entry in new[] { "CALCIFIED_CULTIST", "DAMP_CULTIST" })
        {
            MonsterModel monster = ModelDb.GetById<MonsterModel>(new("MONSTER", entry)).ToMutable();
            Creature creature = combat.CreateCreature(monster, CombatSide.Enemy, null);
            combat.AddCreature(creature);
            monster.SetUpForCombat();
            monster.RollMove(combat.PlayerCreatures);
        }
        Creature enemy = combat.Enemies[0];
        enemy.SetMaxHpInternal(55m);
        enemy.SetCurrentHpInternal(40m);
        enemy.GainBlockInternal(3m);
        player.Creature.SetCurrentHpInternal(63m);
        player.Creature.GainBlockInternal(7m);
        StrengthPower strength = (StrengthPower)ModelDb.Power<StrengthPower>().ToMutable();
        strength.ApplyInternal(player.Creature, 4m, silent: true);
        WeakPower weak = (WeakPower)ModelDb.Power<WeakPower>().ToMutable();
        weak.ApplyInternal(player.Creature, 2m, silent: true);
        weak.AmountOnTurnStart = 7;
        weak.SkipNextDurationTick = true;
        weak.Applier = enemy;
        weak.Target = combat.Enemies[1];
        weak.DynamicVars["DamageDecrease"].BaseValue = 0.62500m;
        RitualPower ritual = (RitualPower)ModelDb.Power<RitualPower>().ToMutable();
        ritual.ApplyInternal(enemy, 3m, silent: true);
        ritual._wasJustAppliedByEnemy = true;
        AutomationPower automation = (AutomationPower)ModelDb.Power<AutomationPower>().ToMutable();
        automation.ApplyInternal(player.Creature, 1m, silent: true);
        automation._internalData!.GetType().GetField("cardsLeft", BindingFlags.Instance | BindingFlags.Public)!
            .SetValue(automation._internalData, 8);
        HardenedShellPower shell = (HardenedShellPower)ModelDb.Power<HardenedShellPower>().ToMutable();
        shell.ApplyInternal(combat.Enemies[1], 20m, silent: true);
        FieldInfo shellCounter = shell._internalData!.GetType().GetField("damageReceivedThisTurn", BindingFlags.Instance | BindingFlags.Public)!;
        shellCounter.SetValue(shell._internalData, 7.000m);
        HellraiserPower hellraiser = (HellraiserPower)ModelDb.Power<HellraiserPower>().ToMutable();
        hellraiser.ApplyInternal(player.Creature, 1m, silent: true);
        FieldInfo infiniteCount = hellraiser._internalData!.GetType().GetField("infiniteAutoPlaysThisTurn")!;
        infiniteCount.SetValue(hellraiser._internalData, 11);
        Creature[] creatures = combat.Creatures.ToArray();
        EnvLiveCreatures.Warmup(combat);
        EnvLiveCreatures captured = EnvLiveCreatures.Capture(combat, player, creatures);
        string frozen = captured.Component.GetRawText();
        JsonElement livePowers = captured.Component.GetProperty("powers");
        JsonElement frozenWeak = Power(livePowers, EnvModelCatalog.Content.GetProperty("powers").GetProperty("WEAK_POWER").GetProperty("id").GetInt32());
        Require(frozenWeak.GetProperty("applier").GetInt32() == 2 && frozenWeak.GetProperty("target").GetInt32() == 3,
            "power applier/target identity changed");
        Require(frozenWeak.GetProperty("amount_on_turn_start").GetInt32() == 7 && frozenWeak.GetProperty("skip_next_duration_tick").GetBoolean(),
            "power duration state lost");
        Require(frozenWeak.GetProperty("vars")[0].GetProperty("decimal_bits")[3].GetUInt32() == 5U << 16,
            "power decimal scale lost");
        Require(Power(livePowers, EnvModelCatalog.Content.GetProperty("powers").GetProperty("RITUAL_POWER").GetProperty("id").GetInt32())
            .GetProperty("extra")[0].GetInt32() == 1, "Ritual subclass state lost");
        Require(Power(livePowers, EnvModelCatalog.Content.GetProperty("powers").GetProperty("AUTOMATION_POWER").GetProperty("id").GetInt32())
            .GetProperty("extra")[0].GetInt32() == 8, "Automation private counter lost");
        Require(Power(livePowers, EnvModelCatalog.Content.GetProperty("powers").GetProperty("HARDENED_SHELL_POWER").GetProperty("id").GetInt32())
            .GetProperty("vars")[0].GetProperty("decimal_bits")[3].GetUInt32() == 3U << 16, "HardenedShell private decimal scale lost");
        Require(Power(livePowers, EnvModelCatalog.Content.GetProperty("powers").GetProperty("HELLRAISER_POWER").GetProperty("id").GetInt32())
            .GetProperty("extra")[0].GetInt32() == 11, "Hellraiser private count lost");

        // This restores the live CREATURE and POWER components into a seeded
        // fixture. It does not certify other fields or a complete live root.
        using NativeEnv.Session seeded = NativeEnv.OpenCombat("DUAL001", "CULTISTS_NORMAL");
        using JsonDocument layout = JsonDocument.Parse(seeded.ExportMonsterMachinesJson());
        JsonObject document = JsonNode.Parse(seeded.ExportSnapshotJson())!.AsObject();
        JsonObject state = document["state"]!.AsObject();
        JsonElement emptyMonster = JsonSerializer.SerializeToElement(state["creatures"]!["slots"]![0]!["value"]!["monster"]);
        Dictionary<int, JsonElement> boundMonsters = EnvLiveMonsters.Capture(creatures).Bind(layout.RootElement);
        JsonElement creaturesPool = captured.Bind(boundMonsters, emptyMonster);
        var wrongModels = new Dictionary<int, JsonElement>(boundMonsters);
        int[] monsterIds = wrongModels.Keys.ToArray();
        (wrongModels[monsterIds[0]], wrongModels[monsterIds[1]]) = (wrongModels[monsterIds[1]], wrongModels[monsterIds[0]]);
        try { captured.Bind(wrongModels, emptyMonster); throw new Exception("bound monster model swap accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("bound_monster_model_mismatch")) { }
        state["creatures"] = JsonNode.Parse(creaturesPool.GetRawText());
        state["powers"] = JsonNode.Parse(livePowers.GetRawText());
        foreach (JsonProperty field in captured.Component.GetProperty("combat_fields").EnumerateObject())
            state["combat"]![field.Name] = JsonNode.Parse(field.Value.GetRawText());
        foreach (JsonProperty field in captured.Component.GetProperty("player_fields").EnumerateObject())
            state["run"]!["player"]![field.Name] = JsonNode.Parse(field.Value.GetRawText());
        foreach (JsonProperty field in captured.Component.GetProperty("player_combat_fields").EnumerateObject())
            state["player_combat"]![field.Name] = JsonNode.Parse(field.Value.GetRawText());
        document["state_hash"] = null;
        using NativeEnv.Session imported = NativeEnv.ImportCombatSnapshot(document.ToJsonString());
        using JsonDocument restored = JsonDocument.Parse(imported.ExportSnapshotJson());
        JsonElement restoredState = restored.RootElement.GetProperty("state");
        Require(JsonElement.DeepEquals(creaturesPool, restoredState.GetProperty("creatures")), "C++ creature restore changed live fields");
        Require(JsonElement.DeepEquals(livePowers, restoredState.GetProperty("powers")), "C++ power restore changed live fields");

        int strikeId = EnvModelCatalog.Content.GetProperty("cards").GetProperty("STRIKE_IRONCLAD").GetProperty("id").GetInt32();
        JsonElement hand = restoredState.GetProperty("player_combat").GetProperty("piles")[0];
        JsonElement cardPool = restoredState.GetProperty("cards");
        int handIndex = Enumerable.Range(0, hand.GetArrayLength()).First(i => Slot(cardPool, hand[i].GetInt32()).GetProperty("id").GetInt32() == strikeId);
        CardModel nativeStrike = player.Deck.Cards.First(card => card is StrikeIronclad);
        decimal nativeDamage = nativeStrike.DynamicVars["Damage"].BaseValue;
        nativeDamage += strength.ModifyDamageAdditive(enemy, nativeDamage, ValueProp.Move, player.Creature, nativeStrike, null);
        nativeDamage *= weak.ModifyDamageMultiplicative(enemy, nativeDamage, ValueProp.Move, player.Creature, nativeStrike, null);
        nativeDamage = decimal.Floor(nativeDamage);
        decimal blocked = enemy.DamageBlockInternal(nativeDamage, ValueProp.Move);
        enemy.LoseHpInternal(nativeDamage - blocked, ValueProp.Move);
        int expectedHp = enemy.CurrentHp, expectedBlock = enemy.Block;
        ulong sourceHash = imported.StateHash;
        using NativeEnv.Session prediction = imported.Predict(new EnvAction(2, handIndex, 1));
        using JsonDocument predicted = JsonDocument.Parse(prediction.ExportSnapshotJson());
        JsonElement predictedEnemy = Slot(predicted.RootElement.GetProperty("state").GetProperty("creatures"), 2);
        Require(predictedEnemy.GetProperty("current_hp").GetInt32() == expectedHp && predictedEnemy.GetProperty("block").GetInt32() == expectedBlock,
            "live Strength/Weak decimal and creature block did not drive native-equivalent Strike damage");
        Require(imported.StateHash == sourceHash, "creature prediction mutated source");

        weak.DynamicVars["DamageDecrease"].BaseValue = 0.99m;
        ritual._wasJustAppliedByEnemy = false;
        automation._internalData!.GetType().GetField("cardsLeft", BindingFlags.Instance | BindingFlags.Public)!.SetValue(automation._internalData, 1);
        Require(captured.Component.GetRawText() == frozen, "live creature/power mutation changed owned component");
        enemy.HpDisplay = HpDisplay.InfiniteWithNumbers;
        Require(Slot(EnvLiveCreatures.Capture(combat, player, creatures).Component.GetProperty("creatures"), 2)
            .GetProperty("hp_display").GetInt32() == 1, "HpDisplay gameplay state lost");
        enemy.HpDisplay = (HpDisplay)255;
        try { EnvLiveCreatures.Capture(combat, player, creatures); throw new Exception("invalid HpDisplay accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("hp_display_invalid")) { }
        enemy.HpDisplay = HpDisplay.Normal;
        try { EnvLiveCreatures.Capture(combat, player, creatures.Reverse().ToArray()); throw new Exception("reordered creature references accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("roster_order")) { }
        weak.Applier = Player.CreateForNewRun<Ironclad>(UnlockState.all, 1).Creature;
        try { EnvLiveCreatures.Capture(combat, player, creatures); throw new Exception("external power applier accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("reference_outside_roster")) { }
        weak.Applier = enemy;
        foreach (decimal counter in new[] { 7.000m, 7.125m, new decimal(0, 0, 0, true, 3), 21m })
        {
            shellCounter.SetValue(shell._internalData, counter);
            JsonElement bits = Power(EnvLiveCreatures.Capture(combat, player, creatures).Component.GetProperty("powers"),
                EnvModelCatalog.Content.GetProperty("powers").GetProperty("HARDENED_SHELL_POWER").GetProperty("id").GetInt32())
                .GetProperty("vars")[0].GetProperty("decimal_bits");
            Require(bits.EnumerateArray().Select(v => v.GetUInt32()).SequenceEqual(decimal.GetBits(counter).Select(v => unchecked((uint)v))),
                "HardenedShell private decimal bits changed");
        }
        shellCounter.SetValue(shell._internalData, 7m);
        var autoPlaying = (HashSet<CardModel>)hellraiser._internalData!.GetType().GetField("autoPlayingCards")!.GetValue(hellraiser._internalData)!;
        autoPlaying.Add(nativeStrike);
        try { EnvLiveCreatures.Capture(combat, player, creatures); throw new Exception("in-progress Hellraiser accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("hellraiser_autoplay_in_progress")) { }
        autoPlaying.Clear();
        object automationData = automation._internalData!;
        automation._internalData = null;
        try { EnvLiveCreatures.Capture(combat, player, creatures); throw new Exception("missing Automation private state accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("internal_data_missing:AUTOMATION_POWER")) { }
        automation._internalData = automationData;
        DampenPower unsupported = (DampenPower)ModelDb.Power<DampenPower>().ToMutable();
        unsupported.ApplyInternal(player.Creature, 1m, silent: true);
        EnvLiveCreatures.Warmup(combat);
        try { EnvLiveCreatures.Capture(combat, player, creatures); throw new Exception("unmapped private power history accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("internal_data_unmapped:DAMPEN_POWER")) { }
        unsupported.RemoveInternal();
        CoveredPower stringVar = (CoveredPower)ModelDb.Power<CoveredPower>().ToMutable();
        stringVar.ApplyInternal(player.Creature, 1m, silent: true);
        EnvLiveCreatures.Warmup(combat);
        try { EnvLiveCreatures.Capture(combat, player, creatures); throw new Exception("StringVar accepted as decimal-only state"); }
        catch (InvalidOperationException error) when (error.Message.Contains("variable_type_unmapped:COVERED_POWER")) { }
        Console.WriteLine("live creature/power component: exact roster, HP/block, refs, decimal vars, private counters, owned C++ restore and native-derived Strike prediction passed; full_root=false");
        HpDisplayChecks.Run();
    }

    private static JsonElement Slot(JsonElement pool, int index)
        => pool.GetProperty("slots").EnumerateArray().Single(slot => slot.GetProperty("index").GetInt32() == index).GetProperty("value");
    private static JsonElement Power(JsonElement pool, int id)
        => pool.GetProperty("slots").EnumerateArray().Select(slot => slot.GetProperty("value")).Single(power => power.GetProperty("id").GetInt32() == id);
    private static void Require(bool condition, string reason)
    {
        if (!condition) throw new Exception(reason);
    }
}
