using System.Diagnostics;
using System.Text.Json;
using CombatSolver;
using Sts2EnvCompare;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.GameActions.Multiplayer;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Hooks;
using MegaCrit.Sts2.Core.Map;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Encounters;
using MegaCrit.Sts2.Core.Models.Events;
using MegaCrit.Sts2.Core.Models.Powers;
using MegaCrit.Sts2.Core.Models.Orbs;
using MegaCrit.Sts2.Core.Models.Relics;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Unlocks;

// Tests the assembler as one document, using production game setup/turn/card
// methods with TestMode off. The room and map are explicit supported fixtures;
// this is not evidence for arbitrary installed profiles or a final Env backend.
internal static class LiveRootChecks
{
    internal static void Run(string variant = "strike")
    {
        SingleThreadContext sync = SingleThreadContext.Install();
        NonInteractiveMode.AutoSlayerCheck = () => true;
        Action restorePresentation = Headless.InstallLiveCombatPresentation();
        Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all, 1);
        RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(acts => acts[0].ToMutable()).ToList(),
            [], GameMode.Standard, 0, "LIVEROOT001");
        RunManager manager = RunManager.Instance;
        CombatManager combatManager = CombatManager.Instance;
        try
        {
            manager.SetUpNewSingleplayer(run, shouldSave: false);
            Await(manager.FinalizeStartingRelics());
            manager.Launch();
            // EventSynchronizer retains the completed Neow instance after the
            // event room exits. Exercise that real lifecycle before composing
            // the ordinary combat root so the event slot is tested end to end.
            manager.EventSynchronizer.BeginEvent(ModelDb.Event<Neow>(), isPrefinished: false);
            sync.Pump();
            EventModel neow = manager.EventSynchronizer.GetLocalEvent();
            int safeNeowOption = neow.CurrentOptions
                .Select((option, index) => (option, index))
                .First(pair => pair.option.Relic is CursedPearl or DowsingRod or BoomingConch
                    or FishingRod or GoldenPearl or NeowsSacrifice or NeowsTorment
                    or SilverCrucible or WingedBoots).index;
            manager.EventSynchronizer.ChooseLocalOption(safeNeowOption);
            Task neowTask = manager.EventSynchronizer.AwaitPendingOptionTasks();
            while (!neowTask.IsCompleted)
            {
                sync.Pump();
                Thread.Yield();
            }
            neowTask.GetAwaiter().GetResult();
            manager.EventSynchronizer.BeforeExitingRoom();
            foreach (EventModel eventModel in manager.EventSynchronizer.Events) eventModel.EnsureCleanup();
            run.Map = StandardActMap.CreateFor(run, replaceTreasureWithElites: false);
            // Replay recording is diagnostic IO; disabling it does not alter
            // gameplay state or the commands being compared in this fixture.
            manager.CombatReplayWriter.IsEnabled = false;
            EncounterModel encounter = ModelDb.Encounter<CultistsNormal>().ToMutable();
            encounter.GenerateMonstersWithSlots(run);
            CombatState combat = new(encounter, run);
            combat.AddPlayer(player);
            foreach (var (monster, slot) in encounter.MonstersWithSlots)
                combat.AddCreature(combat.CreateCreature(monster, CombatSide.Enemy, slot));
            run.PushRoom(new CombatRoom(combat));
            combatManager.SetUpCombat(combat);
            foreach (Creature creature in combat.Creatures) Await(combatManager.AfterCreatureAdded(creature));
            combatManager._turnState!.IsInProgress = true;
            combatManager._turnState.IsStarting = false;
            using LivePowerAllocationTrace powerTrace = new(combat.Creatures.ToArray());
            Await(Hook.BeforeCombatStart(run, combat));
            Await(combatManager.StartTurn(combatManager._turnState));
            PlayerCombatState pcs = player.PlayerCombatState!;
            if (variant == "rampage")
            {
                Rampage growth = combat.CreateCard<Rampage>(player);
                growth.UpgradeInternal(); growth.FinalizeUpgradeInternal();
                growth._extraDamageFromPlays = -1.2500m;
                growth.DynamicVars.Damage.BaseValue = 8.7500m;
                growth.DynamicVars["Increase"].BaseValue = 2.500m;
                pcs.Hand.AddInternal(growth, silent: true);
            }
            else if (variant == "barrage")
            {
                foreach (CardModel card in pcs.Hand.Cards.ToArray())
                { pcs.Hand.RemoveInternal(card, silent: true); pcs.DiscardPile.AddInternal(card, silent: true); }
                CardModel barrage = combat.CreateCard<Barrage>(player);
                barrage.DynamicVars.CalculationBase.BaseValue = -0.2500m;
                barrage.DynamicVars.CalculationExtra.BaseValue = 1.1250m;
                pcs.Hand.AddInternal(barrage, silent: true);
                Await(OrbCmd.AddSlots(player, 3));
                for (int index = 0; index < 3; index++)
                    Await(OrbCmd.Channel<FrostOrb>(new ThrowingPlayerChoiceContext(), player));
            }
            else if (variant is "thrash" or "thrash_body_slam" or "body_slam")
            {
                foreach (CardModel card in pcs.Hand.Cards.ToArray())
                { pcs.Hand.RemoveInternal(card, silent: true); pcs.DiscardPile.AddInternal(card, silent: true); }
                if (variant != "body_slam")
                {
                    Thrash growth = combat.CreateCard<Thrash>(player);
                    growth.UpgradeInternal(); growth.FinalizeUpgradeInternal();
                    growth._extraDamage = 0.1250m;
                    growth.DynamicVars.Damage.BaseValue = 6.1250m;
                    pcs.Hand.AddInternal(growth, silent: true);
                }
                CardModel fodder = variant == "thrash" ? combat.CreateCard<StrikeIronclad>(player)
                    : combat.CreateCard<BodySlam>(player);
                if (variant == "thrash") fodder.DynamicVars.Damage.BaseValue = 1.2500m;
                else
                {
                    fodder.DynamicVars.CalculationBase.BaseValue = -1.2500m;
                    fodder.DynamicVars.ExtraDamage.BaseValue = 0.8750m;
                    player.Creature.Block = 13;
                }
                pcs.Hand.AddInternal(fodder, silent: true);
                Await(PowerCmd.Apply<WeakPower>(new ThrowingPlayerChoiceContext(), player.Creature, 1,
                    combat.Enemies[0], null));
            }

            using NativeEnv.Session schema = NativeEnv.OpenCombat("DUAL001", "CULTISTS_NORMAL");
            string blank = NativeEnv.ExportBlankTemplateJson(schema);
            bool checkedCoverage = false;
            string Capture()
            {
                Creature[] creatures = combat.Creatures.ToArray();
                EnvLiveCards.Warmup(combat);
                EnvLiveCreatures.Warmup(combat);
                EnvLiveInventory.Warmup(player);
                Dictionary<CardModel, int> played = new(ReferenceEqualityComparer.Instance);
                foreach (var entry in combatManager.History.CardPlaysFinished)
                    if (entry._playerTurnNumbers.TryGetValue(player.NetId, out int turn)) played[entry.CardPlay.Card] = turn;
                EnvLiveRootComponents components = new(
                    EnvLiveCards.Capture(combat, player, creatures, played), EnvLiveMonsters.Capture(creatures),
                    EnvLiveRunContext.Capture(combat, player), powerTrace.Reindex(EnvLiveCreatures.Capture(combat, player, creatures)),
                    EnvLiveInventory.Capture(player), EnvLiveCombatContext.Capture(combat, player, creatures),
                    EnvLiveRoomContext.Capture(run, player, combat));
                if (!checkedCoverage)
                {
                    RootCoverageChecks.Run(components, schema, blank);
                    checkedCoverage = true;
                }
                return EnvLiveRootAssembler.Compose(components, schema, blank);
            }

            string liveRoot = Capture();
            using NativeEnv.Session imported = NativeEnv.ImportCombatSnapshot(liveRoot);
            using JsonDocument assembled = JsonDocument.Parse(liveRoot);
            using JsonDocument restored = JsonDocument.Parse(imported.ExportSnapshotJson());
            if (!JsonElement.DeepEquals(assembled.RootElement.GetProperty("state"), restored.RootElement.GetProperty("state")))
                throw new InvalidOperationException("whole_live_root_restore_changed_fields");
            LiveRoot rootView = LiveRoot.Capture(combat, player);
            using (EnvSearchRun diagnostic = NativeEnv.SolveImportedSnapshot(liveRoot, 12, 4, 2,
                rootView, retainSession: true))
            {
                if (!diagnostic.RootMatches || diagnostic.UnportedReason is not null
                    || diagnostic.ResultJson is null || diagnostic.Session is null
                    || diagnostic.Session.StateHash != imported.StateHash)
                    throw new InvalidOperationException("live_diagnostic_did_not_search_supplied_root");
                using JsonDocument diagnosticResult = JsonDocument.Parse(diagnostic.ResultJson);
                if (rootView.DifferenceFields(diagnosticResult.RootElement.GetProperty("root")).Length != 0
                    || diagnosticResult.RootElement.GetProperty("state_hash").GetUInt64() != imported.StateHash)
                    throw new InvalidOperationException("live_diagnostic_result_has_wrong_root");
            }

            int strikeIndex = pcs.Hand.Cards.ToList().FindIndex(card => variant switch {
                "rampage" => card is Rampage, "thrash" or "thrash_body_slam" => card is Thrash,
                "body_slam" => card is BodySlam, "barrage" => card is Barrage, _ => card is StrikeIronclad });
            if (strikeIndex < 0) throw new InvalidOperationException("native_fixture_has_no_strike");
            CardModel strike = pcs.Hand.Cards[strikeIndex];
            Creature target = combat.Enemies[0];
            NativePlanAction[] diagnosticPlan = [new("PlayCard", strike.Id.Entry,
                player.PlayerCombatState.Hand.Cards.Take(strikeIndex).Count(card => card.Id == strike.Id),
                target.CombatId, "", -1, "", 0, false)];
            if (!NativeEnv.VerifyNativePlanSnapshot(liveRoot, rootView, diagnosticPlan, out string replayReason))
                throw new InvalidOperationException("live_diagnostic_plan_replay_failed:" + replayReason);
            Console.WriteLine("live-root diagnostic: imported-root search and native-plan replay preserve supplied root; reference_policy=true; same_policy_backend=false");
            ulong hash = imported.StateHash;
            using NativeEnv.Session predicted = imported.Predict(new EnvAction(2, strikeIndex, (int)target.CombatId!.Value));
            var (energy, stars) = AwaitValue(strike.SpendResources());
            Await(strike.OnPlayWrapper(new ThrowingPlayerChoiceContext(), target, isAutoPlay: false,
                new() { EnergySpent = energy, EnergyValue = energy, StarsSpent = stars, StarValue = stars }));
            using JsonDocument nativeAfter = JsonDocument.Parse(Capture());
            using JsonDocument envAfter = JsonDocument.Parse(predicted.ExportSnapshotJson());
            if (imported.StateHash != hash) throw new InvalidOperationException("whole_live_root_prediction_mutated_source");
            RequireSame(nativeAfter.RootElement.GetProperty("state"), envAfter.RootElement.GetProperty("state"), "state");
            using NativeEnv.Session nextTurn = predicted.Predict(new EnvAction(3));
            EndNativeTurn();
            using JsonDocument nativeTurn = JsonDocument.Parse(Capture());
            using JsonDocument envTurn = JsonDocument.Parse(nextTurn.ExportSnapshotJson());
            RequireSame(nativeTurn.RootElement.GetProperty("state"), envTurn.RootElement.GetProperty("state"), "state");
            using NativeEnv.Session shuffleTurn = nextTurn.Predict(new EnvAction(3));
            EndNativeTurn();
            using JsonDocument nativeShuffle = JsonDocument.Parse(Capture());
            using JsonDocument envShuffle = JsonDocument.Parse(shuffleTurn.ExportSnapshotJson());
            RequireSame(nativeShuffle.RootElement.GetProperty("state"), envShuffle.RootElement.GetProperty("state"), "state");
            Console.WriteLine($"whole live root fixture {variant}: seed-free assemble/restore, production play and two full next-turn successors including shuffle matched; TestMode=false; installed_game_acceptance=false");

            void EndNativeTurn()
            {
                Await(combatManager.EndPlayerTurnPhaseOneInternal());
                Await(combatManager.EndPlayerTurnPhaseTwoInternal());
                Await(combatManager.SwitchFromPlayerToEnemySide(combatManager._turnState));
                Await(combatManager.StartTurn(combatManager._turnState)); // enemy turn, then SwitchSides
                Await(combatManager.StartTurn(combatManager._turnState)); // player setup
            }
        }
        finally
        {
            combatManager.Reset(graceful: false);
            manager.CleanUp();
            NonInteractiveMode.AutoSlayerCheck = () => false;
            restorePresentation();
            SynchronizationContext.SetSynchronizationContext(null);
        }

        void Await(Task task)
        {
            Stopwatch timeout = Stopwatch.StartNew();
            while (!task.IsCompleted && timeout.Elapsed < TimeSpan.FromSeconds(20))
            {
                sync.Pump();
                Thread.Yield();
            }
            if (!task.IsCompleted) throw new InvalidOperationException("native_live_root_fixture_timeout");
            task.GetAwaiter().GetResult();
        }
        T AwaitValue<T>(Task<T> task) { Await(task); return task.GetAwaiter().GetResult(); }
    }

    private static void RequireSame(JsonElement native, JsonElement env, string path)
    {
        if (JsonElement.DeepEquals(native, env)) return;
        if (native.ValueKind == JsonValueKind.Object && env.ValueKind == JsonValueKind.Object)
            foreach (JsonProperty property in native.EnumerateObject())
            {
                if (!env.TryGetProperty(property.Name, out JsonElement value))
                    throw new InvalidOperationException("whole_live_root_successor_field_missing:" + path + "." + property.Name);
                RequireSame(property.Value, value, path + "." + property.Name);
            }
        else if (native.ValueKind == JsonValueKind.Array && env.ValueKind == JsonValueKind.Array
            && native.GetArrayLength() == env.GetArrayLength())
            for (int index = 0; index < native.GetArrayLength(); index++)
                RequireSame(native[index], env[index], path + "[" + index + "]");
        throw new InvalidOperationException($"whole_live_root_successor_mismatch:{path};native={native.GetRawText()};env={env.GetRawText()}");
    }
}
