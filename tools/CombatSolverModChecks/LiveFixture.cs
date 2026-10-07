using System.Diagnostics;
using System.Text.Json;
using CombatSolver;
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
using MegaCrit.Sts2.Core.Models.Potions;
using MegaCrit.Sts2.Core.Models.Relics;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Unlocks;

// Tests the assembler as one document, using production game setup/turn/card
// methods with TestMode off. The room and map are explicit supported fixtures;
// this is not evidence for arbitrary installed profiles or a final Env backend.
internal static class LiveFixture
{
    internal static void Run(string characterId, string? runSavePath = null)
    {
        SingleThreadContext sync = SingleThreadContext.Install();
        NonInteractiveMode.AutoSlayerCheck = () => true;
        Action restorePresentation = Headless.InstallLiveCombatPresentation();
        CharacterModel character = ModelDb.AllCharacters.Single(c => c.Id.Entry == characterId);
        UnlockState unlocks = LoadProfile(characterId, runSavePath);
        Player player = Player.CreateForNewRun(character, unlocks, 1);
        RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(acts => acts[0].ToMutable()).ToList(),
            [], GameMode.Standard, 0, "LIVEROOT001");
        RunManager manager = RunManager.Instance;
        CombatManager combatManager = CombatManager.Instance;
        try
        {
            manager.SetUpNewSingleplayer(run, shouldSave: false);
            Await(manager.FinalizeStartingRelics());
            Await(RelicCmd.Obtain<DemonTongue>(player));
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
                    or GoldenPearl or NeowsSacrifice or NeowsTorment).index;
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
            run.Map = new SavedActMap(MegaCrit.Sts2.Core.Saves.Runs.SerializableActMap.FromActMap(
                StandardActMap.CreateFor(run, replaceTreasureWithElites: false)));
            Console.WriteLine("MAP_INPUT " + run.Map.GetType().Name + " relic=DEMON_TONGUE");
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
            
            Await(Hook.BeforeCombatStart(run, combat));
            Await(combatManager.StartTurn(combatManager._turnState));
            PlayerCombatState pcs = player.PlayerCombatState!;
            // Exercise the real in-combat potion inventory path. The Env route
            // may choose to spend it, but the live root remains read-only.
            Await(PotionCmd.TryToProcure<FirePotion>(player));
            EnvLiveCapture.Warmup(combat);
            LiveMapChecks.Run(run, combat, player);
            string captured = EnvLiveCapture.Capture(combat, player);
            using EnvSolverSession env = new();
            env.Import(captured);
            VerifyProfile(captured, unlocks);
            var config = EnvSolverSession.DefaultConfig();
            config.BeamWidth = 8; config.MaxNodes = 500; config.TimeBudgetMs = 1500;
            config.TransitionCacheMiB = 16;
            var result = env.Solve(config, CancellationToken.None);
            if (!result.GetProperty("replay_verified").GetBoolean()) throw new Exception("unverified result");
            if (captured != EnvLiveCapture.Capture(combat, player)) throw new Exception("search changed live root");
            Console.WriteLine("LIVE_CAPTURE_SEARCH potions=" + player.Potions.Count() + " " + result.GetProperty("outcome").GetRawText());
            // The second capture is taken from an actual mid-turn game state.
            int index = pcs.Hand.Cards.ToList().FindIndex(card => card.Id.Entry.StartsWith("STRIKE_"));
            if (index < 0) throw new Exception("fixture lacks Strike");
            CardModel card = pcs.Hand.Cards[index];
            var (energy, stars) = AwaitValue(card.SpendResources());
            Await(card.OnPlayWrapper(new ThrowingPlayerChoiceContext(), combat.Enemies[0], false,
                new() { EnergySpent = energy, EnergyValue = energy, StarsSpent = stars, StarValue = stars }));
            string midTurn = EnvLiveCapture.Capture(combat, player);
            if (midTurn == captured) throw new Exception("mid-turn root was replaced by opening root");
            env.Import(midTurn);
            Console.WriteLine("MID_TURN_CAPTURE_SEARCH " + env.Solve(config, CancellationToken.None).GetProperty("outcome").GetRawText());
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

    private static UnlockState LoadProfile(string characterId, string? path)
    {
        // A fully unlocked veteran need not have encountered every ordinary
        // monster and event variant. Use the game's actual discovery contract
        // instead of its synthetic all-encounters sentinel even without a save.
        if (path is null) return new UnlockState(UnlockState.all._unlockedEpochIds,
            ModelDb.Acts.SelectMany(act => act.BossDiscoveryOrder).Select(e => e.Id).Distinct(), 46);
        using JsonDocument save = JsonDocument.Parse(File.ReadAllText(path));
        JsonElement profile = save.RootElement.GetProperty("players").EnumerateArray()
            .Single(player => player.GetProperty("character_id").GetString() == "CHARACTER." + characterId)
            .GetProperty("unlock_state");
        var serializable = new SerializableUnlockState
        {
            NumberOfRuns = profile.GetProperty("number_of_runs").GetInt32(),
            UnlockedEpochs = profile.GetProperty("unlocked_epochs").EnumerateArray().Select(e => e.GetString()!).ToList(),
            EncountersSeen = profile.GetProperty("encounters_seen").EnumerateArray().Select(e => ModelId.Deserialize(e.GetString()!)).ToList(),
        };
        UnlockState unlocks = UnlockState.FromSerializable(serializable);
        Console.WriteLine($"PROFILE_INPUT runs={unlocks.NumberOfRuns} epochs={unlocks.EpochUnlockCount()} seen={unlocks._encountersSeen.Count}");
        return unlocks;
    }

    private static void VerifyProfile(string captured, UnlockState unlocks)
    {
        ulong low = 0, high = 0;
        ulong epochBits = 0;
        for (int i = 0; i < MegaCrit.Sts2.Core.Timeline.EpochModel.AllEpochIds.Count; i++)
            if (unlocks._unlockedEpochIds.Contains(MegaCrit.Sts2.Core.Timeline.EpochModel.AllEpochIds[i]))
                epochBits |= 1UL << i;
        foreach (ModelId encounter in unlocks._encountersSeen)
        {
            int id = EnvModelCatalog.Id(ModelDb.GetById<EncounterModel>(encounter), "encounters");
            if (id < 64) low |= 1UL << id;
            else high |= 1UL << (id - 64);
        }
        using JsonDocument snapshot = JsonDocument.Parse(captured);
        JsonElement run = snapshot.RootElement.GetProperty("state").GetProperty("run");
        foreach (JsonElement profile in new[] { run.GetProperty("profile"), run.GetProperty("player").GetProperty("profile") })
            if (profile.GetProperty("number_of_runs").GetInt32() != unlocks.NumberOfRuns
                || profile.GetProperty("unlocked_epoch_bits").GetUInt64() != epochBits
                || profile.GetProperty("encounters_seen_low").GetUInt64() != low
                || profile.GetProperty("encounters_seen_high").GetUInt64() != high)
                throw new Exception("captured profile facts were replaced");
        Console.WriteLine($"PROFILE_PRESERVED runs={unlocks.NumberOfRuns} seen_low={low:x} seen_high={high:x}");
    }
}
