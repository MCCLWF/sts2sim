using System.Runtime.CompilerServices;
using System.Text.Json;
using System.Text.Json.Nodes;
using CombatSolver;
using CombatSolverEnv.Api;
using Sts2EnvCompare;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Unlocks;

internal static class Program
{
    private static void Main()
    {
        GameBootstrap.InstallResolver(GameBootstrap.DefaultGameData);
        Check();
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static void Check()
    {
        GameBootstrap.Init(testMode: false);
        Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all, 1);
        RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(acts => acts[0].ToMutable()).ToList(),
            [], GameMode.Standard, 0, "DUAL001");
        CombatState combat = new(runState: run);
        combat.AddPlayer(player);
        player.PlayerCombatState = new(player);
        var pcs = player.PlayerCombatState;
        CardModel original = player.Deck.Cards.First(card => card is StrikeIronclad);
        CardModel first = combat.CreateCard<StrikeIronclad>(player);
        first.DeckVersion = original;
        first.EnergyCost.SetThisTurnOrUntilPlayed(0);
        first.DynamicVars["Damage"].BaseValue = 11.25m;
        CardModel second = combat.CreateCard<StrikeIronclad>(player);
        second.UpgradeInternal();
        second._cloneOf = first;
        second._isDupe = true;
        second.EnergyCost.AddUntilPlayed(-1);
        CardModel removedOriginal = ModelDb.Card<StrikeIronclad>().ToMutable();
        removedOriginal.Owner = player;
        removedOriginal.HasBeenRemovedFromState = true;
        second.DeckVersion = removedOriginal;
        pcs.Hand.AddInternal(first, silent: true);
        pcs.Hand.AddInternal(second, silent: true);
        pcs.DrawPile.AddInternal(combat.CreateCard<DefendIronclad>(player), silent: true);
        EnvLiveCards.Warmup(combat);
        EnvLiveCards component = EnvLiveCards.Capture(combat, player, combat.Creatures.ToArray(),
            new Dictionary<CardModel, int>(ReferenceEqualityComparer.Instance) { [first] = 7 });
        int nativeLocalCost = first.EnergyCost.GetWithModifiers(CostModifiers.Local);
        JsonElement frozen = component.Component;
        JsonElement firstValue = Slot(frozen, frozen.GetProperty("piles")[0][0].GetInt32());
        JsonElement secondValue = Slot(frozen, frozen.GetProperty("piles")[0][1].GetInt32());
        Require(firstValue.GetProperty("last_played_turn").GetInt32() == 7, "card history turn lost");
        Require(secondValue.GetProperty("clone_of").GetInt32() == frozen.GetProperty("piles")[0][0].GetInt32(), "clone identity lost");
        Require(firstValue.GetProperty("deck_version").GetInt32() == frozen.GetProperty("deck")[0].GetInt32(), "deck original identity lost");
        Require(Slot(frozen, secondValue.GetProperty("deck_version").GetInt32()).GetProperty("removed").GetBoolean(),
            "reachable removed original was not captured");
        Require(firstValue.GetProperty("cost_modifiers")[0].GetProperty("expiration").GetInt32() == 6, "combined cost expiry lost");
        Require(secondValue.GetProperty("upgrade").GetInt32() == 1, "same-name upgraded instance lost");
        Require(firstValue.GetProperty("var_overrides").GetArrayLength() == 1, "modified dynamic variable lost");
        Require(firstValue.GetProperty("base_star_cost").GetInt32() == first.CanonicalStarCost && !first._starCostSet,
            "lazy star cost was read as zero or initialized during capture");
        string before = frozen.GetRawText();
        first.EnergyCost.SetThisCombat(2);
        first.DynamicVars["Damage"].BaseValue = 99;
        pcs.Hand.RemoveInternal(second, silent: true);
        Require(component.Component.GetRawText() == before, "live mutation changed frozen component");

        // Only the CARD component is live here. The rest is an explicit seeded
        // fixture, not evidence for full live-root import or backend completion.
        NativeEnv.Configure();
        using NativeEnv.Session seeded = NativeEnv.OpenCombat("DUAL001", "CULTISTS_NORMAL");
        JsonObject document = JsonNode.Parse(seeded.ExportSnapshotJson())!.AsObject();
        Require(document["model_id_hash"]!.GetValue<uint>() == frozen.GetProperty("model_id_hash").GetUInt32(), "catalog ABI mismatch");
        JsonObject state = document["state"]!.AsObject();
        state["cards"] = JsonNode.Parse(frozen.GetProperty("cards").GetRawText());
        state["run"]!["player"]!["deck"] = JsonNode.Parse(frozen.GetProperty("deck").GetRawText());
        state["player_combat"]!["piles"] = JsonNode.Parse(frozen.GetProperty("piles").GetRawText());
        document["state_hash"] = null;
        using NativeEnv.Session imported = NativeEnv.ImportCombatSnapshot(document.ToJsonString());
        using JsonDocument roundtrip = JsonDocument.Parse(imported.ExportSnapshotJson());
        Require(JsonElement.DeepEquals(roundtrip.RootElement.GetProperty("state").GetProperty("cards"), frozen.GetProperty("cards")), "C++ did not restore actual card values");
        ulong hash = imported.StateHash;
        int initialEnergy = state["player_combat"]!["energy"]!.GetValue<int>();
        using NativeEnv.Session played = imported.Predict(new EnvAction(2, 0, 1));
        using JsonDocument playedJson = JsonDocument.Parse(played.ExportSnapshotJson());
        Require(imported.StateHash == hash && played.StateHash != hash, "card prediction mutated source or failed");
        JsonElement playedPiles = playedJson.RootElement.GetProperty("state").GetProperty("player_combat").GetProperty("piles");
        Require(playedJson.RootElement.GetProperty("state").GetProperty("player_combat").GetProperty("energy").GetInt32()
            == initialEnergy - nativeLocalCost, "actual native cost did not drive Env resource spending");
        Require(playedPiles[0].GetArrayLength() == 1 && playedPiles[2].GetArrayLength() == 1, "actual hand did not drive Env action");
        Require(Slot(playedJson.RootElement.GetProperty("state"), frozen.GetProperty("piles")[0][0].GetInt32()).GetProperty("cost_modifiers").GetArrayLength() == 0,
            "until-played cost did not expire");

        // Attachment state and subclass counters require dedicated mappings.
        first.Enchantment = ModelDb.Enchantment<MegaCrit.Sts2.Core.Models.Enchantments.Steady>().ToMutable();
        try { EnvLiveCards.Capture(combat, player, combat.Creatures.ToArray(), new Dictionary<CardModel, int>()); throw new Exception("unmapped attachment accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("attachment_unmapped")) { }
        first.Enchantment = null;
        CardModel unsupported = combat.CreateCard<Claw>(player);
        pcs.Hand.AddInternal(unsupported, silent: true);
        EnvLiveCards.Warmup(combat);
        try { EnvLiveCards.Capture(combat, player, combat.Creatures.ToArray(), new Dictionary<CardModel, int>()); throw new Exception("unmapped subclass accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("subclass_fields_unmapped:CLAW")) { }
        combat.RemoveCard(unsupported);
        pcs.Hand.RemoveInternal(unsupported, silent: true);
        CardModel hidden = combat.CreateCard<SpoilsMap>(player);
        pcs.Hand.AddInternal(hidden, silent: true);
        EnvLiveCards.Warmup(combat);
        try { EnvLiveCards.Capture(combat, player, combat.Creatures.ToArray(), new Dictionary<CardModel, int>()); throw new Exception("unexported auto property accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("hidden_subclass_field_unmapped:SPOILS_MAP")) { }
        combat.RemoveCard(hidden);
        pcs.Hand.RemoveInternal(hidden, silent: true);
        ((MegaCrit.Sts2.Core.Localization.DynamicVars.DamageVar)first.DynamicVars["Damage"]).Props = MegaCrit.Sts2.Core.ValueProps.ValueProp.Unpowered;
        try { EnvLiveCards.Capture(combat, player, combat.Creatures.ToArray(), new Dictionary<CardModel, int>()); throw new Exception("modified damage properties accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("variable_props_unmapped")) { }
        Console.WriteLine("live card component: native instances, exact values, identity, immutable ownership, C++ restore/predict and explicit rejection passed; full_root=false");
        CheckMonsters();
        CheckToadpoles();
        CheckRandomBranches();
        CheckMonsterCoordinateRng();
        CheckRunContext();
        LiveCreatureChecks.Run();
        LiveInventoryChecks.Run();
        LiveCombatContextChecks.Run();
        RoomContextChecks.Run();
        LiveRootChecks.Run();
        LiveRootChecks.Run("rampage");
        LiveRootChecks.Run("thrash");
        LiveRootChecks.Run("thrash_body_slam");
        LiveRootChecks.Run("body_slam");
        LiveRootChecks.Run("barrage");
    }

    private static void CheckMonsters()
    {
        Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all, 1);
        RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(acts => acts[0].ToMutable()).ToList(),
            [], GameMode.Standard, 0, "DUAL001");
        CombatState combat = new(runState: run);
        combat.AddPlayer(player);
        player.PlayerCombatState = new(player);
        foreach (string entry in new[] { "CALCIFIED_CULTIST", "DAMP_CULTIST" })
        {
            MonsterModel monster = ModelDb.GetById<MonsterModel>(new("MONSTER", entry)).ToMutable();
            var creature = combat.CreateCreature(monster, CombatSide.Enemy, null);
            combat.AddCreature(creature);
            monster.SetUpForCombat();
            monster.RollMove(combat.PlayerCreatures);
        }
        using NativeEnv.Session root = NativeEnv.OpenCombat("DUAL001", "CULTISTS_NORMAL");
        EnvLiveMonsters initial = EnvLiveMonsters.Capture(combat.Creatures.ToArray());
        initial.RequireMatch(root);
        string frozen = initial.Component.GetRawText();
        using NativeEnv.Session turn2 = root.Predict(new EnvAction(3));
        // Exercise the game's FSM transition APIs; combat effects are outside
        // this fixture. Expected current/log/flags come from the native machine.
        foreach (var creature in combat.Enemies)
        {
            var monster = creature.Monster!;
            monster.MoveStateMachine!.OnMovePerformed(monster.NextMove);
            monster.RollMove(combat.PlayerCreatures);
            monster.OnSideSwitch();
        }
        EnvLiveMonsters second = EnvLiveMonsters.Capture(combat.Creatures.ToArray());
        second.RequireMatch(turn2);
        Require(initial.Component.GetRawText() == frozen, "native FSM mutation changed frozen monster component");

        var first = combat.Enemies[0].Monster!;
        var move = (MegaCrit.Sts2.Core.MonsterMoves.MonsterMoveStateMachine.MoveState)first.MoveStateMachine!.States["DARK_STRIKE_MOVE"];
        move.FollowUpState = first.MoveStateMachine.States["INCANTATION_MOVE"];
        move._performedAtLeastOnce = true;
        first.Rng.NextInt(100);
        EnvLiveMonsters edited = EnvLiveMonsters.Capture(combat.Creatures.ToArray());
        using JsonDocument layout = JsonDocument.Parse(turn2.ExportMonsterMachinesJson());
        Dictionary<int, JsonElement> mapped = edited.Bind(layout.RootElement);
        JsonObject document = JsonNode.Parse(turn2.ExportSnapshotJson())!.AsObject();
        foreach (var slot in document["state"]!["creatures"]!["slots"]!.AsArray())
        {
            var value = slot!["value"]!.AsObject();
            if (mapped.TryGetValue(value["combat_id"]!.GetValue<int>(), out JsonElement monster)) value["monster"] = JsonNode.Parse(monster.GetRawText());
        }
        document["state_hash"] = null;
        using NativeEnv.Session restored = NativeEnv.ImportCombatSnapshot(document.ToJsonString());
        edited.RequireMatch(restored);
        ulong before = restored.StateHash;
        using NativeEnv.Session advanced = restored.Predict(new EnvAction(3));
        foreach (var creature in combat.Enemies)
        {
            var monster = creature.Monster!;
            monster.MoveStateMachine!.OnMovePerformed(monster.NextMove);
            monster.RollMove(combat.PlayerCreatures);
            monster.OnSideSwitch();
        }
        EnvLiveMonsters.Capture(combat.Creatures.ToArray()).RequireMatch(advanced);
        Require(restored.StateHash == before, "prediction mutated imported monster source");
        JsonObject corruptLayout = JsonNode.Parse(layout.RootElement.GetRawText())!.AsObject();
        corruptLayout["machines"]![0]!["states"]![0]!["on_perform"] = "Wrong.Callback";
        try { edited.Bind(JsonSerializer.SerializeToElement(corruptLayout)); throw new Exception("unknown callback accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("move_definition_mismatch")) { }
        first.NextMove = new("STUNNED", _ => Task.CompletedTask);
        try { EnvLiveMonsters.Capture(combat.Creatures.ToArray()).Bind(layout.RootElement); throw new Exception("unmapped stun accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("unknown_state:STUNNED")) { }
        Console.WriteLine("live monster component: native FSM transition APIs, complete RNG, runtime name binding, follow-up/flag restore, pure prediction and explicit definition/stun rejection passed; combat_effects_not_compared full_root=false");
    }

    private static void CheckToadpoles()
    {
        Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all, 1);
        RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(acts => acts[0].ToMutable()).ToList(),
            [], GameMode.Standard, 0, "DUAL001");
        CombatState combat = new(runState: run);
        combat.AddPlayer(player);
        player.PlayerCombatState = new(player);
        foreach (bool front in new[] { true, false })
        {
            var monster = ModelDb.GetById<MonsterModel>(new("MONSTER", "TOADPOLE")).ToMutable();
            ((MegaCrit.Sts2.Core.Models.Monsters.Toadpole)monster).IsFront = front;
            combat.AddCreature(combat.CreateCreature(monster, CombatSide.Enemy, null));
            monster.SetUpForCombat();
            monster.RollMove(combat.PlayerCreatures);
        }
        using NativeEnv.Session root = NativeEnv.OpenCombat("DUAL001", "TOADPOLES_WEAK");
        EnvLiveMonsters.Capture(combat.Creatures.ToArray()).RequireMatch(root);
        using NativeEnv.Session next = root.Predict(new EnvAction(3));
        foreach (var creature in combat.Enemies)
        {
            var monster = creature.Monster!;
            monster.MoveStateMachine!.OnMovePerformed(monster.NextMove);
            monster.RollMove(combat.PlayerCreatures);
            monster.OnSideSwitch();
        }
        EnvLiveMonsters.Capture(combat.Creatures.ToArray()).RequireMatch(next);
        var machine = combat.Enemies[0].Monster!.MoveStateMachine!;
        var conditional = (MegaCrit.Sts2.Core.MonsterMoves.MonsterMoveStateMachine.ConditionalBranchState)machine.States["INIT_MOVE"];
        conditional.States[0] = new(machine.States["WHIRL_MOVE"], conditional.States[1]._conditionalLambda);
        try { EnvLiveMonsters.Capture(combat.Creatures.ToArray()); throw new Exception("swapped vanilla conditional accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("state_lambda_unmapped")) { }
        Console.WriteLine("live monster conditional branches: native front/back Toadpole transitions and multi-attack definition passed; combat_effects_not_compared full_root=false");
    }

    private static void CheckRandomBranches()
    {
        Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all, 1);
        RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(acts => acts[0].ToMutable()).ToList(),
            [], GameMode.Standard, 0, "DUAL001");
        CombatState combat = new(runState: run);
        combat.AddPlayer(player);
        player.PlayerCombatState = new(player);
        MonsterModel monster = ModelDb.GetById<MonsterModel>(new("MONSTER", "SLUDGE_SPINNER")).ToMutable();
        combat.AddCreature(combat.CreateCreature(monster, CombatSide.Enemy, null));
        monster.SetUpForCombat();
        monster.RollMove(combat.PlayerCreatures);
        NativeEnv.Session current = NativeEnv.OpenCombat("DUAL001", "SLUDGE_SPINNER_WEAK");
        try
        {
            for (int turn = 0; turn <= 3; turn++)
            {
                EnvLiveMonsters.Capture(combat.Creatures.ToArray()).RequireMatch(current);
                EnvStateSnapshot snapshot = EnvStateSnapshot.Parse(current.ExportSnapshotJson());
                var rng = run.Rng.MonsterAi;
                EnvRngComparison.RequireMatch(new(rng._counter, rng._random._s0, rng._random._s1, rng._random._s2, rng._random._s3),
                    snapshot.GetRunRng(EnvRunRngStream.MonsterAi), EnvRunRngStream.MonsterAi, "fixture");
                if (turn == 3) break;
                NativeEnv.Session next = current.Predict(new EnvAction(3));
                current.Dispose();
                current = next;
                monster.MoveStateMachine!.OnMovePerformed(monster.NextMove);
                monster.RollMove(combat.PlayerCreatures);
                monster.OnSideSwitch();
            }
        }
        finally { current.Dispose(); }
        Console.WriteLine("live monster random branches: vanilla scalar weights, repeat rules and three native RNG/FSM transitions passed; combat_effects_not_compared full_root=false");
    }

    private static void CheckMonsterCoordinateRng()
    {
        foreach ((int? col, int? row) in new[] { ((int?)null, (int?)null), ((int?)0, (int?)-1), ((int?)-3, (int?)5) })
        {
            Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all, 1);
            RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(acts => acts[0].ToMutable()).ToList(),
                [], GameMode.Standard, 0, "DUAL001");
            if (col is { } c && row is { } r) run.AddVisitedMapCoord(new(c, r));
            CombatState combat = new(runState: run);
            combat.AddPlayer(player);
            MonsterModel monster = ModelDb.GetById<MonsterModel>(new("MONSTER", "CALCIFIED_CULTIST")).ToMutable();
            combat.CreateCreature(monster, CombatSide.Enemy, null);
            var rng = monster._rng!;
            Console.WriteLine($"native monster RNG root coord={col?.ToString() ?? "null"},{row?.ToString() ?? "null"} id={monster.Creature.CombatId} counter={rng._counter} state={rng._random._s0},{rng._random._s1},{rng._random._s2},{rng._random._s3}");
        }
    }

    private static void CheckRunContext()
    {
        GameBootstrap.UseInMemorySaves();
        GameBootstrap.LoadLocalization();
        Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all, 1);
        var selectionRng = new MegaCrit.Sts2.Core.Random.Rng(StringHelper.GetDeterministicHashCode("DUAL001"), "act_selection");
        RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(acts => selectionRng.NextItem(acts.Where(act => act.IsUnlocked(UnlockState.all)).ToList())!.ToMutable()).ToList(),
            [], GameMode.Standard, 0, "DUAL001");
        RunManager.Instance.SetUpNewSingleplayer(run, shouldSave: false);
        RunManager.Instance.FinalizeStartingRelics().GetAwaiter().GetResult();
        run.Map = MegaCrit.Sts2.Core.Map.StandardActMap.CreateFor(run, replaceTreasureWithElites: false);
        run.AddVisitedMapCoord(new(3, 0));
        run.AddVisitedMapCoord(new(-3, 5));
        run.ActFloor = 6;
        CombatState combat = new(runState: run);
        combat.AddPlayer(player);
        player.PlayerCombatState = new(player);
        // Advance every actual stream independently before capture; counters
        // alone cannot prove that all state words were transferred.
        foreach (var stream in Enum.GetValues<MegaCrit.Sts2.Core.Entities.Rngs.RunRngType>()) run.Rng.GetRng(stream).NextInt(100);
        foreach (var stream in Enum.GetValues<MegaCrit.Sts2.Core.Entities.Rngs.PlayerRngType>()) player.PlayerRng.GetRng(stream).NextInt(100);
        run.Odds.UnknownMapPoint.MonsterOdds = BitConverter.UInt32BitsToSingle(0x80000000); // negative zero
        player.PlayerOdds.PotionReward.OverrideCurrentValue(0.713f);
        player.Gold = 137;
        player.ExtraFields.CardShopRemovalsUsed = 4;
        EnvLiveRunContext captured = EnvLiveRunContext.Capture(combat, player);
        string frozen = captured.Component.GetRawText();
        JsonElement runFields = captured.Component.GetProperty("run"), playerFields = captured.Component.GetProperty("player");
        Require(runFields.GetProperty("visited_map_coords")[1].GetProperty("col").GetInt32() == -3
            && runFields.GetProperty("odds").GetProperty("monster").GetProperty("f32_bits").GetUInt32() == 0x80000000,
            "live run lost negative coordinate or float bits");
        using NativeEnv.Session seed = NativeEnv.OpenCombat("DUAL001", "CULTISTS_NORMAL");
        // Explicit mixed fixture: all run context comes from native objects;
        // combat/pools come from the seeded Env. This is not a complete root.
        JsonObject document = JsonNode.Parse(seed.ExportSnapshotJson())!.AsObject();
        JsonObject rootRun = document["state"]!["run"]!.AsObject();
        foreach (var field in runFields.EnumerateObject()) rootRun[field.Name] = JsonNode.Parse(field.Value.GetRawText());
        foreach (var field in playerFields.EnumerateObject()) rootRun["player"]![field.Name] = JsonNode.Parse(field.Value.GetRawText());
        document["state_hash"] = null;
        using NativeEnv.Session imported = NativeEnv.ImportCombatSnapshot(document.ToJsonString());
        using JsonDocument result = JsonDocument.Parse(imported.ExportSnapshotJson());
        JsonElement actualRun = result.RootElement.GetProperty("state").GetProperty("run");
        foreach (var field in runFields.EnumerateObject()) Require(JsonElement.DeepEquals(field.Value, actualRun.GetProperty(field.Name)), "run component import differs:" + field.Name);
        foreach (var field in playerFields.EnumerateObject()) Require(JsonElement.DeepEquals(field.Value, actualRun.GetProperty("player").GetProperty(field.Name)), "player component import differs:" + field.Name);
        // Native Fisher-Yates supplies the expectation for a later reshuffle.
        var expectedShuffle = new MegaCrit.Sts2.Core.Random.Rng(run.Rng.Shuffle.ToSerializable());
        expectedShuffle.Shuffle(Enumerable.Range(0, 10).ToList());
        ulong before = imported.StateHash;
        using NativeEnv.Session turn2 = imported.Predict(new EnvAction(3));
        using NativeEnv.Session turn3 = turn2.Predict(new EnvAction(3));
        EnvStateSnapshot advanced = EnvStateSnapshot.Parse(turn3.ExportSnapshotJson());
        var expectedRng = expectedShuffle._random;
        Require(advanced.GetRunRng(EnvRunRngStream.Shuffle) == new EnvRngState(expectedShuffle._counter, expectedRng._s0, expectedRng._s1, expectedRng._s2, expectedRng._s3), "imported native Shuffle stream did not drive later reshuffle");
        Require(imported.StateHash == before, "run component prediction changed source");
        foreach (var stream in Enum.GetValues<EnvPlayerRngStream>())
            Require(advanced.GetPlayerRng(stream) == ReadRng(player.PlayerRng.GetRng((MegaCrit.Sts2.Core.Entities.Rngs.PlayerRngType)(int)stream)), "inactive player RNG changed");
        foreach (var stream in Enum.GetValues<MegaCrit.Sts2.Core.Entities.Rngs.RunRngType>()) run.Rng.GetRng(stream).NextInt(100);
        player.PlayerRng.Rewards.NextInt(100);
        player.Gold = 999;
        run.Map.startMapPoints.Clear();
        Require(captured.Component.GetRawText() == frozen, "live mutation changed owned run component");
        var savedRng = run.Rng._rngs[MegaCrit.Sts2.Core.Entities.Rngs.RunRngType.CombatTargets];
        run.Rng._rngs[MegaCrit.Sts2.Core.Entities.Rngs.RunRngType.CombatTargets] = run.Rng.Shuffle;
        try { EnvLiveRunContext.Capture(combat, player); throw new Exception("aliased streams accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("rng_type_or_alias_unmapped")) { }
        run.Rng._rngs[MegaCrit.Sts2.Core.Entities.Rngs.RunRngType.CombatTargets] = savedRng;
        // Reinsert an unchanged value: the game's roulette observes dictionary
        // enumeration order, so a value-only mapper would accept a wrong root.
        var roomOdds = run.Odds.UnknownMapPoint._nonEventOdds;
        float previousMonster = roomOdds[MegaCrit.Sts2.Core.Rooms.RoomType.Monster];
        roomOdds.Remove(MegaCrit.Sts2.Core.Rooms.RoomType.Monster);
        // Dictionary reuses the removed slot, so rebuild to produce an actual
        // changed enumeration rather than assuming removal moves it to the end.
        var reordered = roomOdds.ToArray();
        roomOdds.Clear();
        foreach (var pair in reordered) roomOdds.Add(pair.Key, pair.Value);
        roomOdds.Add(MegaCrit.Sts2.Core.Rooms.RoomType.Monster, previousMonster);
        try { EnvLiveRunContext.Capture(combat, player); throw new Exception("changed odds enumeration accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("odds_definition_unmapped")) { }
        roomOdds.Clear();
        foreach (var room in new[] { MegaCrit.Sts2.Core.Rooms.RoomType.Monster, MegaCrit.Sts2.Core.Rooms.RoomType.Elite, MegaCrit.Sts2.Core.Rooms.RoomType.Treasure, MegaCrit.Sts2.Core.Rooms.RoomType.Shop })
            roomOdds.Add(room, room == MegaCrit.Sts2.Core.Rooms.RoomType.Monster ? previousMonster : reordered.Single(pair => pair.Key == room).Value);
        run.Odds.UnknownMapPoint.SetBaseOdds(MegaCrit.Sts2.Core.Rooms.RoomType.Monster, 0.2f);
        try { EnvLiveRunContext.Capture(combat, player); throw new Exception("modified base odds accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("odds_definition_unmapped")) { }
        RunManager.Instance.CleanUp();
        var partialProfile = new UnlockState([], [], 0);
        Player partialPlayer = Player.CreateForNewRun<Ironclad>(partialProfile, 1);
        var partialSelection = new MegaCrit.Sts2.Core.Random.Rng(StringHelper.GetDeterministicHashCode("DUAL001"), "act_selection");
        RunState partialRun = RunState.CreateForNewRun([partialPlayer], ModelDb.ActsByIndex.Select(acts => partialSelection.NextItem(acts.Where(act => act.IsUnlocked(partialProfile)).ToList())!.ToMutable()).ToList(),
            [], GameMode.Standard, 0, "DUAL001");
        RunManager.Instance.SetUpNewSingleplayer(partialRun, shouldSave: false);
        RunManager.Instance.FinalizeStartingRelics().GetAwaiter().GetResult();
        partialRun.Map = MegaCrit.Sts2.Core.Map.StandardActMap.CreateFor(partialRun, replaceTreasureWithElites: false);
        CombatState partialCombat = new(runState: partialRun);
        partialCombat.AddPlayer(partialPlayer);
        partialPlayer.PlayerCombatState = new(partialPlayer);
        EnvLiveRunContext partial = EnvLiveRunContext.Capture(partialCombat, partialPlayer);
        Require(!partial.Component.GetProperty("profile").GetProperty("all_unlocked").GetBoolean(), "partial profile was labelled fully unlocked");
        var ownedProfile = partial.Component.GetProperty("profile_source").GetProperty("player");
        Require(ownedProfile.GetProperty("number_of_runs").GetInt32() == 0 && ownedProfile.GetProperty("unlocked_epochs").GetArrayLength() == 0
            && ownedProfile.GetProperty("encounters_seen").GetArrayLength() == 0, "partial profile source values were changed");
        foreach (var encoded in new[] { partial.Component.GetProperty("run").GetProperty("profile"),
            partial.Component.GetProperty("player").GetProperty("profile") })
            Require(encoded.GetProperty("number_of_runs").GetInt32() == 0
                && encoded.GetProperty("unlocked_epoch_bits").GetUInt64() == 0
                && encoded.GetProperty("encounters_seen_low").GetUInt64() == 0
                && encoded.GetProperty("encounters_seen_high").GetUInt64() == 0
                && !encoded.GetProperty("full_veteran_compatible").GetBoolean(),
                "partial profile bitsets were not captured from the game");
        string frozenPartial = partial.Component.GetRawText();
        partialProfile._unlockedEpochIds.UnionWith(UnlockState.all._unlockedEpochIds);
        partialProfile._encountersSeen.UnionWith(UnlockState.all._encountersSeen);
        Require(partial.Component.GetRawText() == frozenPartial, "profile capture retained live collections");
        RunManager.Instance.CleanUp();
        Console.WriteLine("live run component: all 15 RNG streams, exact odds, coordinates, acts/bags/map links, owned restore and native Shuffle-driven prediction passed; full_root=false");
        Console.WriteLine("live profile component: actual partial progress owned and bitsets mapped; full_root=false");

        static EnvRngState ReadRng(MegaCrit.Sts2.Core.Random.Rng rng) => new(rng._counter, rng._random._s0, rng._random._s1, rng._random._s2, rng._random._s3);
    }

    private static JsonElement Slot(JsonElement root, int index)
        => root.GetProperty("cards").GetProperty("slots").EnumerateArray().Single(slot => slot.GetProperty("index").GetInt32() == index).GetProperty("value");
    private static void Require(bool condition, string reason)
    {
        if (!condition) throw new InvalidOperationException(reason);
    }
}
