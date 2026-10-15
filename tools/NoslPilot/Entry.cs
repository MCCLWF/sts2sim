using System.Diagnostics;
using System.Reflection;
using System.Text.Json;
using System.Text.Json.Nodes;
using Godot;
using HarmonyLib;
using MegaCrit.Sts2.Core.Assets;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Context;
using MegaCrit.Sts2.Core.Entities.CardRewardAlternatives;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Merchant;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Entities.Potions;
using MegaCrit.Sts2.Core.Entities.RestSite;
using MegaCrit.Sts2.Core.Events;
using MegaCrit.Sts2.Core.Events.Custom.CrystalSphereEvent;
using MegaCrit.Sts2.Core.GameActions;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Map;
using MegaCrit.Sts2.Core.Modding;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Events;
using MegaCrit.Sts2.Core.Nodes;
using MegaCrit.Sts2.Core.Nodes.Cards.Holders;
using MegaCrit.Sts2.Core.Nodes.CommonUi;
using MegaCrit.Sts2.Core.Nodes.Events;
using MegaCrit.Sts2.Core.Nodes.Events.Custom;
using MegaCrit.Sts2.Core.Nodes.GodotExtensions;
using MegaCrit.Sts2.Core.Nodes.RestSite;
using MegaCrit.Sts2.Core.Nodes.Rewards;
using MegaCrit.Sts2.Core.Nodes.Rooms;
using MegaCrit.Sts2.Core.Nodes.Screens;
using MegaCrit.Sts2.Core.Nodes.Screens.Capstones;
using MegaCrit.Sts2.Core.Nodes.Screens.CardSelection;
using MegaCrit.Sts2.Core.Nodes.Screens.GameOverScreen;
using MegaCrit.Sts2.Core.Nodes.Screens.Map;
using MegaCrit.Sts2.Core.Nodes.Screens.Overlays;
using MegaCrit.Sts2.Core.Nodes.Screens.Shops;
using MegaCrit.Sts2.Core.Nodes.Screens.TreasureRoomRelic;
using MegaCrit.Sts2.Core.Random;
using MegaCrit.Sts2.Core.Rewards;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Saves;
using MegaCrit.Sts2.Core.Settings;
using MegaCrit.Sts2.Core.TestSupport;
using MegaCrit.Sts2.Core.Unlocks;

namespace NoslPilot;

// The NoSL policy operating a whole run in the game
// (docs/plans/2026-10-04-nosl-q5a-live-pilot.md).
//
// The decisions come from the native pilot (nosl/pilot.h): the policy network
// on the actor's observation of a shadow run kept in step by seed and by the
// actions themselves. This mod waits for the game to stand at the decision the
// pilot is about to make, checks that the game's public state reads the same
// as the pilot's, takes the action from the pilot and performs it the way the
// interface would. Nothing is tried and undone, and the game is never asked
// about its future. On a mismatch, a timeout or a pilot error it stops acting
// and leaves the game as it is.
//
// Normal startup uses Interactive.cs and the in-game control panel. The following
// environment variables opt into the isolated developer autorun instead:
//   NOSL_PILOT_SEED, NOSL_PILOT_WEIGHTS   the run seed and the exported weights (required)
//   NOSL_PILOT_ASCENSION (10), NOSL_PILOT_FILTER (1), NOSL_PILOT_POLICY_SEED (1), NOSL_PILOT_GREEDY (0)
//   NOSL_PILOT_MODE       shadow (default): decisions and observations come from the native pilot's shadow run, and
//                         the game-side encoder (Observer.cs) is compared with it at every decision.
//                         direct: no shadow and no seed in the actor's path; the mod finds the decision on screen,
//                         encodes it from the game's objects and asks the actor (nosl/actor.h) on those tables.
//   NOSL_PILOT_LOG        JSONL log path (default: <user data>/nosl-pilot.jsonl)
//   NOSL_PILOT_FAST       Normal | Fast | Instant (default Fast)
//   NOSL_PILOT_INTERACTIVE  1 (default): the game stays in its normal interactive mode. 0: the unattended mode of
//                         the game's own smoke test (no waits, no pausing); faster, but the Punch Off event's
//                         cosmetic loop crashes the game in it, as it does at Instant speed
//   NOSL_PILOT_QUIT       1: quit the game when the run is over or the pilot stops (default 1); never after a hand-over
//   NOSL_PILOT_CONTROL    a file read before every decision; its last non-empty line is one of
//                         pause     the pilot does nothing and the game is the player's
//                         resume    the pilot goes on; in direct mode it reads the screen again, so it continues
//                                   from wherever the player left the game
//                         takeover  the pilot ends for this run and leaves the game running for the player
//                         With a window, F8 toggles the pause and F9 hands over.
[ModInitializer(nameof(Initialize))]
public static partial class Entry
{
    private sealed class PilotStop : Exception
    {
        public PilotStop(string reason) : base(reason) { }
    }

    private static NativePilot? _pilot;
    private static NativeActor? _actor;   // direct mode
    private static bool _direct;
    private static bool _treasureResolved;
    private static readonly string[] ActionNames = { "StartRun", "MapMove", "PlayCard", "EndTurn", "TakeReward", "Proceed", "EventOption",
        "RestOption", "ShopBuy", "TreasurePick", "UsePotion", "DiscardPotion", "Answer", "Resume" };
    private static NGame _host = null!;
    private static RunState _run = null!;
    private static Player _player = null!;
    private static StreamWriter? _log;
    private static int _taken;            // actions taken from the pilot, by anyone
    private static string? _fatal;        // set where an exception cannot be thrown to the driver (inside a game callback)
    private static Task? _pendingPurchase;
    private static bool _chestOpened;
    private static AbstractRoom? _chestRoom;
    private static readonly Stopwatch Clock = Stopwatch.StartNew();
    private static readonly List<double> NextMs = new();
    private static int _digestChecks;
    // The game-side encoder (Observer.cs) against the shadow's observation, decision by decision.
    private static int _obsChecks, _obsSame, _obsExact, _obsErrors, _obsReported;
    private static CardReward? _activeCardReward;
    private static readonly Dictionary<string, int[]> ObsByKind = new();  // decision -> {checked, same}

    private const string RoomPath = "/root/Game/RootSceneContainer/Run/RoomContainer/";

    public static void Initialize()
    {
        NativeRuntime.Initialize();
        if (System.Environment.GetEnvironmentVariable("NOSL_PILOT_AUTORUN") != "1")
        {
            _ = InitializeInteractiveAsync();
            return;
        }
        GD.Print("[NoslPilot] autorun requested");
        _ = AutoRunAsync();
    }

    private static string Env(string name, string fallback) =>
        System.Environment.GetEnvironmentVariable(name) is { Length: > 0 } value ? value : fallback;

    private static async Task AutoRunAsync()
    {
        string outcome = "stopped";
        string reason = "";
        try
        {
            _host = NGame.Instance ?? throw new PilotStop("NGame.Instance is null at mod initialization");
            await _host.GameStartupComplete;
            string logPath = Env("NOSL_PILOT_LOG", Path.Combine(OS.GetUserDataDir(), "nosl-pilot.jsonl"));
            _log = new StreamWriter(logPath, append: false) { AutoFlush = true };
            string seed = SeedHelper.CanonicalizeSeed(Env("NOSL_PILOT_SEED", ""));
            string weights = Env("NOSL_PILOT_WEIGHTS", "");
            if (seed.Length == 0 || weights.Length == 0) throw new PilotStop("NOSL_PILOT_SEED and NOSL_PILOT_WEIGHTS are required");
            int ascension = int.Parse(Env("NOSL_PILOT_ASCENSION", "10"));
            int filter = int.Parse(Env("NOSL_PILOT_FILTER", "1"));
            ulong policySeed = ulong.Parse(Env("NOSL_PILOT_POLICY_SEED", "1"));
            bool greedy = Env("NOSL_PILOT_GREEDY", "0") == "1";
            Emit(new JsonObject { ["event"] = "begin", ["seed"] = seed, ["ascension"] = ascension, ["actor_filter"] = filter,
                ["policy_seed"] = policySeed, ["greedy"] = greedy, ["weights"] = weights });

            // No tutorials; the game speed is a normal setting. The unattended mode (AutoSlayer.PlayRunAsync) is opt-in.
            SaveManager.Instance.PrefsSave.FastMode = Enum.Parse<FastModeType>(Env("NOSL_PILOT_FAST", "Fast"));
            SaveManager.Instance.SetFtuesEnabled(enabled: false);
            if (Env("NOSL_PILOT_INTERACTIVE", "1") != "1") NonInteractiveMode.AutoSlayerCheck = () => true;

            _controlPath = Env("NOSL_PILOT_CONTROL", "");
            _direct = Env("NOSL_PILOT_MODE", "shadow") == "direct";
            // In direct mode the seed only starts the run in the game (a test convenience); nothing native receives it.
            if (_direct) _actor = new NativeActor(weights, filter, policySeed, greedy);
            else _pilot = new NativePilot(seed, ascension, filter, weights, policySeed, greedy);
            await StartRunAsync(seed, ascension);
            using IDisposable selector = CardSelectCmd.UseSelector(new Selector());
            InstallChoiceHooks();
            outcome = _direct ? await PlayDirectAsync() : await PlayAsync();
        }
        catch (PilotStop stop) { reason = stop.Message; if (_handedOver) outcome = "handed_over"; }
        catch (Exception ex) { reason = ex.ToString(); }
        try
        {
            NextMs.Sort();
            Emit(new JsonObject
            {
                ["event"] = "end", ["mode"] = _direct ? "direct" : "shadow", ["outcome"] = outcome, ["reason"] = reason, ["actions"] = _taken,
                ["digest_checks"] = _digestChecks,
                ["floor"] = _run?.TotalFloor ?? -1, ["hp"] = _player?.Creature.CurrentHp ?? -1,
                ["live_digest"] = _run == null ? "" : LiveDigest(), ["wall_s"] = Math.Round(Clock.Elapsed.TotalSeconds, 1),
                ["obs_checks"] = _obsChecks, ["obs_same"] = _obsSame, ["obs_exact"] = _obsExact, ["obs_errors"] = _obsErrors,
                ["obs_by_decision"] = new JsonObject(ObsByKind.OrderBy(kv => kv.Key).Select(kv =>
                    new KeyValuePair<string, JsonNode?>(kv.Key, new JsonArray(kv.Value[0], kv.Value[1])))),
                ["next_ms_median"] = NextMs.Count == 0 ? 0 : NextMs[NextMs.Count / 2],
                ["next_ms_p95"] = NextMs.Count == 0 ? 0 : NextMs[(int)(NextMs.Count * 0.95)],
                ["next_ms_max"] = NextMs.Count == 0 ? 0 : NextMs[^1],
            });
            GD.Print($"[NoslPilot] END outcome={outcome} reason={reason}");
        }
        finally
        {
            _log?.Dispose();
            _pilot?.Dispose();
            _actor?.Dispose();
            _actor = null;
            _pilot = null;
            if (Env("NOSL_PILOT_QUIT", "1") == "1" && !_handedOver) _host?.GetTree().Quit(outcome == "stopped" ? 1 : 0);
        }
    }

    // A new run on the seed with everything unlocked, which is what the shadow assumes (Oracle.StartRun, and
    // NGame.StartRun for the scene). A profile that has not unlocked everything would diverge at once.
    private static async Task StartRunAsync(string seed, int ascension)
    {
        CharacterModel character = ModelDb.AllCharacters.Single(model => model.Id.Entry == "SILENT");
        UnlockState unlocks = UnlockState.all;
        Rng actRng = new(StringHelper.GetDeterministicHashCode(seed), "act_selection");
        List<ActModel> acts = new();
        foreach (IReadOnlyList<ActModel> candidates in ModelDb.ActsByIndex)
            acts.Add(actRng.NextItem(candidates.Where(act => act.IsUnlocked(unlocks)).ToList())!);
        Player starting = Player.CreateForNewRun(character, unlocks, 1uL);
        RunState run = RunState.CreateForNewRun(new[] { starting }, acts.Select(act => act.ToMutable()).ToList(),
            System.Array.Empty<ModifierModel>(), GameMode.Standard, ascension, seed);
        RunManager.Instance.SetUpNewSingleplayer(run, shouldSave: false);
        await PreloadManager.LoadRunAssets(run.Players.Select(player => player.Character));
        await PreloadManager.LoadActAssets(run.Acts[0]);
        await RunManager.Instance.FinalizeStartingRelics();
        RunManager.Instance.Launch();
        _host.RootSceneContainer.SetCurrentScene(NRun.Create(run));
        await RunManager.Instance.EnterAct(0, doTransition: false);
        _run = run;
        _player = run.Players[0];
        Emit(new JsonObject { ["event"] = "run_started", ["acts"] = new JsonArray(acts.Select(act => (JsonNode)act.Id.Entry).ToArray()) });
    }

    // ---- the driver ----

    // ---- direct mode: no shadow ----

    // The decision the screen shows, or null while the game is busy or between screens.
    private static (string decision, bool cardReward, bool nestedRewards)? Detect()
    {
        if (RunManager.Instance.ActionExecutor.IsRunning) return null;
        if (!RelicsSettled()) return null;
        if (_pendingPurchase is { IsCompleted: false }) return null;
        IOverlayScreen? top = NOverlayStack.Instance?.Peek();
        // Only the game's own game-over screen, or the victory room, ends the run. A player at 0 HP is not the end
        // while a revive is still to resolve (Fairy in a Bottle): read at that moment it looked like a death.
        if (top is NGameOverScreen || _run.CurrentRoom?.IsVictoryRoom == true) return ("Ended", false, false);
        if (_player.Creature.IsDead) return null;
        if (top is NCardRewardSelectionScreen) return ("Choice", true, false);
        // An open map during a combat is the player looking at it, never a map decision.
        if (NMapScreen.Instance is { IsOpen: true } map)
            return map is { IsTravelEnabled: true, IsTraveling: false } && !CombatManager.Instance.IsInProgress ? ("Map", false, false) : null;
        if (top is NRewardsScreen rewards)
            return (bool)AccessTools.Field(typeof(NRewardsScreen), "_isTerminal").GetValue(rewards)! ? ("Rewards", false, false) : ("Choice", false, true);
        if (top != null) return null;
        if (CombatManager.Instance.IsInProgress) return IsReady("Combat", false) ? ("Combat", false, false) : null;
        switch (_run.CurrentRoom)
        {
            case EventRoom when FakeShop() != null:
                return IsReady("Shop", false) ? ("Shop", false, false) : null;
            case EventRoom room:
                return !room.LocalMutableEvent.IsFinished && IsReady("Event", false) ? ("Event", false, false) : null;
            case RestSiteRoom rest:
                return rest.Options.Count > 0 && IsReady("RestSite", false) ? ("RestSite", false, false) : null;
            case MerchantRoom:
                return IsReady("Shop", false) ? ("Shop", false, false) : null;
            case TreasureRoom:
            {
                if (!_chestOpened || !ReferenceEquals(_chestRoom, _run.CurrentRoom)) return null;  // OpenChestOnce comes first
                bool relics = RunManager.Instance.TreasureRoomRelicSynchronizer.CurrentRelics is { Count: > 0 };
                return relics && !_treasureResolved && IsReady("Treasure", false) ? ("Treasure", false, false) : null;
            }
            default: return null;
        }
    }

    private static JsonObject Decide(string decision, Obs o, Choice? choice)
    {
        Stopwatch timer = Stopwatch.StartNew();
        int index = _actor!.Decide(o, out int rows, out double prob, out float value);
        double ms = timer.Elapsed.TotalMilliseconds;
        int[] legal = o.Legal[index];
        JsonObject act = new()
        {
            ["step"] = _taken, ["decision"] = decision, ["forced"] = rows == 1,
            ["action"] = new JsonObject { ["kind"] = ActionNames[legal[0]], ["index"] = legal[1], ["target"] = legal[2], ["col"] = legal[3], ["row"] = legal[4] },
        };
        if (choice != null)
            act["choice"] = new JsonObject
            {
                ["kind"] = choice.Kind == 0 ? "CardSelect" : choice.Kind == 1 ? "Bundle" : "Option", ["source"] = choice.Source, ["prompt"] = choice.Prompt,
                ["subset"] = choice.Subset, ["min"] = choice.Min, ["max"] = choice.Max, ["options"] = choice.Options.Count, ["can_skip"] = choice.CanSkip,
            };
        act["digest"] = LiveDigest();
        act["rows"] = rows;
        act["prob"] = prob;
        act["value"] = value;
        act["ms"] = ms;
        if (rows > 1) NextMs.Add(ms);
        act["event"] = "action";
        act["t"] = Math.Round(Clock.Elapsed.TotalSeconds, 2);
        _taken++;
        Emit(act);
        return act;
    }

    private static async Task<string> PlayDirectAsync()
    {
        Stopwatch idle = Stopwatch.StartNew();
        double nudgeAt = 0.5;
        while (true)
        {
            if (_fatal != null) throw new PilotStop(_fatal);
            if (await Hold()) { idle.Restart(); nudgeAt = 0.5; }
            // A normal menu start publishes CurrentRoom before async event assets and
            // its per-player event exist. The autorun entry awaited EnterAct first.
            if (_run.CurrentRoom is EventRoom && RunManager.Instance.EventSynchronizer.Events.Count == 0)
            {
                if (idle.Elapsed.TotalSeconds > 60) throw new PilotStop("事件场景加载超过 60 秒，请手动检查后重试。");
                await Frame();
                continue;
            }
            if (_run.CurrentRoom is TreasureRoom) await OpenChestOnce();
            (string decision, bool cardReward, bool nestedRewards)? at = Detect();
            if (at == null)
            {
                if (idle.Elapsed.TotalSeconds > 60) throw new PilotStop($"no decision appeared for 60s ({Where()})");
                if (idle.Elapsed.TotalSeconds > nudgeAt)
                {
                    nudgeAt = idle.Elapsed.TotalSeconds + 1.0;
                    await ProceedToMap();
                }
                await Frame();
                continue;
            }
            idle.Restart();
            nudgeAt = 0.5;
            (string decision, bool cardReward, bool nestedRewards) = at.Value;
            if (decision == "Ended")
            {
                bool victory = !_player.Creature.IsDead;
                Emit(new JsonObject { ["step"] = _taken, ["decision"] = "Ended", ["victory"] = victory, ["floor"] = _run.TotalFloor, ["digest"] = LiveDigest(),
                    ["event"] = "action", ["t"] = Math.Round(Clock.Elapsed.TotalSeconds, 2) });
                _taken++;
                return victory ? "victory" : "death";
            }
            Obs o = ObserveHere(decision, cardReward, nestedRewards);
            Choice? choice = null;
            if (nestedRewards)
            {
                RewardsSet set = RewardsOf((NRewardsScreen)NOverlayStack.Instance!.Peek()!);
                choice = Observer.NestedRewards(set, set.DisallowSkipping);
            }
            else if (cardReward) choice = new Choice { Kind = 0, Source = "CardSelectCmd", Prompt = "CardReward", Min = 0, Max = 1 };
            JsonObject act = Decide(decision, o, choice);
            if (cardReward) ((JsonObject)act["choice"]!)["options"] = o.Actions.Count;
            string kind = (string)act["action"]!["kind"]!;
            if (decision == "Treasure" && kind is "TreasurePick" or "Proceed") _treasureResolved = true;
            await Execute(decision, act, cardReward, nestedRewards, act["choice"] as JsonObject);
        }
    }

    // A selection the game asks for from inside an action, decided on the game-side tables.
    private static int DecideChoice(Choice choice)
    {
        Obs o = Observer.Build(_run, _player, Observer.DChoice, choice);
        JsonObject act = Decide("Choice", o, choice);
        return (int)act["action"]!["index"]!;
    }

    // ---- pause, resume, hand over (NOSL_PILOT_CONTROL, F8, F9) ----

    private static string _controlPath = "";
    private static bool _pausedByKey, _handedOver, _f8Down, _f9Down;

    private static string ControlCommand()
    {
        try
        {
            if (_controlPath.Length == 0 || !File.Exists(_controlPath)) return "";
            return File.ReadLines(_controlPath).Select(line => line.Trim().ToLowerInvariant()).LastOrDefault(line => line.Length > 0) ?? "";
        }
        catch (IOException) { return ""; }
    }

    // Waits while the pilot is paused; true when it did wait. Throws when the run is handed over.
    private static async Task<bool> Hold()
    {
        if (_interactive) return await HoldInteractiveAsync();
        bool waited = false;
        while (true)
        {
            string command = ControlCommand();
            bool f8 = Input.IsKeyPressed(Key.F8), f9 = Input.IsKeyPressed(Key.F9);
            if (f8 && !_f8Down) _pausedByKey = !_pausedByKey;
            bool handOver = command == "takeover" || (f9 && !_f9Down);
            _f8Down = f8;
            _f9Down = f9;
            if (handOver)
            {
                _handedOver = true;
                Emit(new JsonObject { ["event"] = "takeover", ["where"] = Where(), ["t"] = Math.Round(Clock.Elapsed.TotalSeconds, 2) });
                throw new PilotStop("handed over to the player");
            }
            if (command != "pause" && !_pausedByKey)
            {
                if (waited) Emit(new JsonObject { ["event"] = "resumed", ["where"] = Where(), ["t"] = Math.Round(Clock.Elapsed.TotalSeconds, 2) });
                return waited;
            }
            if (!waited) Emit(new JsonObject { ["event"] = "paused", ["where"] = Where(), ["t"] = Math.Round(Clock.Elapsed.TotalSeconds, 2) });
            waited = true;
            await Frame();
        }
    }

    // ---- shadow mode ----

    private static async Task<string> PlayAsync()
    {
        while (true)
        {
            if (_fatal != null) throw new PilotStop(_fatal);
            await Hold();
            JsonObject peek = Parse(_pilot!.Peek());
            string decision = (string)peek["decision"]!;
            string expected = (string)peek["digest"]!;
            if (decision == "Ended")
            {
                await WaitUntil(() => _player.Creature.IsDead || NOverlayStack.Instance?.Peek() is NGameOverScreen
                    || _run.CurrentRoom?.IsVictoryRoom == true, 60, "the game to end where the pilot's run ended");
                JsonObject last = TakeNext();
                bool victory = (bool)last["victory"]!;
                bool dead = _player.Creature.IsDead;
                if (victory == dead) throw new PilotStop($"the pilot's run ended with victory={victory}, the game has dead={dead}");
                return victory ? "victory" : "death";
            }
            JsonObject? choice = peek["choice"] as JsonObject;
            bool cardReward = choice != null && (string)choice["kind"]! == "CardSelect" && (string)choice["prompt"]! == "CardReward";
            bool nestedRewards = choice != null && (string)choice["kind"]! == "Option" && (string)choice["source"]! == "rewards";
            if (decision == "Choice" && !cardReward && !nestedRewards)
            {
                // Answered from inside the game's own call (the card selector or a patched screen), not from here.
                int before = _taken;
                await WaitUntil(() => _taken != before || _fatal != null, 30, $"the game to ask for the selection {choice!.ToJsonString()}");
                continue;
            }
            string live = await WaitReady(decision, cardReward, expected);
            if (live != expected)
            {
                Emit(new JsonObject { ["event"] = "mismatch", ["decision"] = decision, ["pilot"] = expected, ["game"] = live });
                throw new PilotStop("the game's public state differs from the pilot's at a " + decision + " decision");
            }
            _digestChecks++;
            CheckObservation(decision + (cardReward ? ":CardReward" : nestedRewards ? ":rewards" : ""), () => ObserveHere(decision, cardReward, nestedRewards));
            JsonObject act = TakeNext();
            await Execute(decision, act, cardReward, nestedRewards, choice);
        }
    }

    // The observation of the decision the game stands at, read from the game's own objects.
    private static Obs ObserveHere(string decision, bool cardReward, bool nestedRewards)
    {
        switch (decision)
        {
            case "Map": return Observer.Build(_run, _player, Observer.DMap);
            case "Combat": return Observer.Build(_run, _player, Observer.DCombat);
            case "Rewards": return Observer.Build(_run, _player, Observer.DRewards, rewards: RewardsOf((NRewardsScreen)NOverlayStack.Instance!.Peek()!));
            case "Event": return Observer.Build(_run, _player, Observer.DEvent);
            case "RestSite":
                return Observer.Build(_run, _player, Observer.DRestSite, restProceed: Room<NRestSiteRoom>("RestSiteRoom")!.ProceedButton.IsEnabled);
            case "Shop": return Observer.Build(_run, _player, Observer.DShop, shop: ShopInventory());
            case "Treasure":
                return Observer.Build(_run, _player, Observer.DTreasure,
                    treasure: RunManager.Instance.TreasureRoomRelicSynchronizer.CurrentRelics ?? (IReadOnlyList<RelicModel>)System.Array.Empty<RelicModel>());
            case "Choice" when cardReward:
            {
                NCardRewardSelectionScreen screen = (NCardRewardSelectionScreen)NOverlayStack.Instance!.Peek()!;
                IReadOnlyList<CardRewardAlternative> alternatives =
                    (IReadOnlyList<CardRewardAlternative>)AccessTools.Field(typeof(NCardRewardSelectionScreen), "_extraOptions").GetValue(screen)!;
                // A player may open a reward while paused, or before pressing Start. Bind
                // to the reward that owns the actual visible screen, not the last AI click.
                CardReward reward = UiHelperFindAll<NRewardsScreen>(_host).SelectMany(s => RewardsOf(s).Rewards)
                    .OfType<CardReward>().FirstOrDefault(r => ReferenceEquals(
                        AccessTools.Field(typeof(CardReward), "_currentlyShownScreen").GetValue(r), screen))
                    ?? (_activeCardReward != null && ReferenceEquals(
                        AccessTools.Field(typeof(CardReward), "_currentlyShownScreen").GetValue(_activeCardReward), screen)
                        ? _activeCardReward : throw new PilotStop("无法识别当前卡牌奖励，请手动完成后再开始。"));
                _activeCardReward = reward;
                return Observer.Build(_run, _player, Observer.DChoice, Observer.CardRewardChoice(reward, alternatives));
            }
            case "Choice" when nestedRewards:
            {
                RewardsSet set = RewardsOf((NRewardsScreen)NOverlayStack.Instance!.Peek()!);
                return Observer.Build(_run, _player, Observer.DChoice, Observer.NestedRewards(set, set.DisallowSkipping));
            }
            default: throw new InvalidOperationException("no encoder for a " + decision + " decision");
        }
    }

    // Compares as the model sees them: the scalars, the tokens as a multiset, and each action row with the tokens
    // it points at (token indices themselves depend on order).
    private static string? Difference(Obs live, Obs shadow)
    {
        static string Row(int[] t) => string.Join(",", t);
        List<string> a = live.Tokens.Select(Row).OrderBy(x => x, StringComparer.Ordinal).ToList();
        List<string> b = shadow.Tokens.Select(Row).OrderBy(x => x, StringComparer.Ordinal).ToList();
        if (a.Count != b.Count || !a.SequenceEqual(b))
        {
            List<string> onlyGame = a.Where(x => !b.Contains(x)).Take(8).ToList(), onlyShadow = b.Where(x => !a.Contains(x)).Take(8).ToList();
            return $"tokens: game {a.Count}, shadow {b.Count}; only in game [{string.Join(" / ", onlyGame)}]; only in shadow [{string.Join(" / ", onlyShadow)}]";
        }
        for (int i = 0; i < Obs.GlobalWidth; i++)
            if (live.Global[i] != shadow.Global[i]) return $"global[{i}]: game {live.Global[i]}, shadow {shadow.Global[i]}";
        if (live.Actions.Count != shadow.Actions.Count) return $"actions: game {live.Actions.Count}, shadow {shadow.Actions.Count}";
        static string Act(Obs o, int i)
        {
            int[] r = o.Actions[i];
            string token = r[1] >= 0 ? Row(o.Tokens[r[1]]) : "-", target = r[2] >= 0 ? Row(o.Tokens[r[2]]) : "-";
            return $"{r[0]}|{token}|{target}|{r[3]}|{Row(o.Legal[i])}";
        }
        for (int i = 0; i < live.Actions.Count; i++)
            if (Act(live, i) != Act(shadow, i)) return $"action {i}: game {Act(live, i)}; shadow {Act(shadow, i)}";
        return null;
    }

    private static void CheckObservation(string where, Func<Obs> build)
    {
        if (!ObsByKind.TryGetValue(where, out int[]? tally)) ObsByKind[where] = tally = new int[2];
        try
        {
            Obs live = build();
            Obs shadow = _pilot!.Observe();
            _obsChecks++;
            tally[0]++;
            string? difference = Difference(live, shadow);
            if (difference == null)
            {
                _obsSame++;
                tally[1]++;
                // Identical tables in identical order give the network identical arithmetic, not only identical content.
                if (live.Tokens.Count == shadow.Tokens.Count && Enumerable.Range(0, live.Tokens.Count).All(i => live.Tokens[i].SequenceEqual(shadow.Tokens[i]))
                    && Enumerable.Range(0, live.Actions.Count).All(i => live.Actions[i].SequenceEqual(shadow.Actions[i]))) _obsExact++;
            }
            else if (_obsReported++ < 40) Emit(new JsonObject { ["event"] = "obs_difference", ["where"] = where, ["step"] = _taken, ["difference"] = difference });
        }
        catch (Exception ex)
        {
            _obsErrors++;
            if (_obsReported++ < 40) Emit(new JsonObject { ["event"] = "obs_error", ["where"] = where, ["step"] = _taken, ["error"] = ex.Message });
        }
    }

    private static JsonObject TakeNext()
    {
        string line = _pilot!.Next();
        _taken++;
        JsonObject act = Parse(line);
        if (act["forced"] is JsonValue forced && !(bool)forced && act["ms"] != null) NextMs.Add((double)act["ms"]!);
        act["event"] = "action";
        act["t"] = Math.Round(Clock.Elapsed.TotalSeconds, 2);
        Emit(act);
        return act;
    }

    // Waits until the game stands at the decision and reads like the pilot; returns the game's digest (the pilot's
    // when they agree, the last one seen when the wait ran out).
    private static async Task<string> WaitReady(string decision, bool cardReward, string expected)
    {
        Stopwatch waited = Stopwatch.StartNew();
        double nudgeAt = 0.5;
        string live = "";
        while (waited.Elapsed.TotalSeconds < 60)
        {
            if (_fatal != null) throw new PilotStop(_fatal);
            if (decision == "Treasure") await OpenChestOnce();
            if (IsReady(decision, cardReward))
            {
                live = LiveDigest();
                if (live == expected) return live;
            }
            else if (decision == "Map" && waited.Elapsed.TotalSeconds > nudgeAt)
            {
                // The shadow goes from a finished event, a used rest site or a chest with nothing to pick straight to
                // the map; the game shows a proceed control first.
                nudgeAt = waited.Elapsed.TotalSeconds + 1.0;
                await ProceedToMap();
            }
            await Frame();
        }
        if (live.Length == 0) throw new PilotStop($"the game did not reach the {decision} decision the pilot expects ({Where()})");
        return live;
    }

    // 21 relics show another number for about a second after they trigger: while their private IsActivating is
    // set, DisplayAmount returns the trigger count instead of the counter (HappyFlower.DoActivateVisuals and the
    // like). It is a visual window; the observation the policy was trained on is the settled display. A decision is
    // therefore read only once no relic the player owns is inside that window.
    private static readonly Dictionary<Type, FieldInfo?> ActivatingField = new();

    private static bool RelicsSettled()
    {
        foreach (RelicModel relic in _player.Relics)
        {
            Type type = relic.GetType();
            if (!ActivatingField.TryGetValue(type, out FieldInfo? field))
                ActivatingField[type] = field = type.GetField("_isActivating", BindingFlags.Instance | BindingFlags.NonPublic);
            if (field?.GetValue(relic) is true) return false;
        }
        return true;
    }

    private static async Task WaitRelicsSettled()
    {
        Stopwatch waited = Stopwatch.StartNew();
        while (!RelicsSettled() && waited.Elapsed.TotalSeconds < 5) await Frame();
    }

    private static bool IsReady(string decision, bool cardReward)
    {
        if (RunManager.Instance.ActionExecutor.IsRunning) return false;
        if (!RelicsSettled()) return false;
        if (_pendingPurchase is { IsCompleted: false }) return false;
        IOverlayScreen? top = NOverlayStack.Instance?.Peek();
        switch (decision)
        {
            case "Combat":
            {
                CombatState? combat = CombatManager.Instance.DebugOnlyGetState();
                return combat != null && CombatManager.Instance.IsInProgress && top == null
                    && _player.PlayerCombatState?.Phase == PlayerTurnPhase.Play && combat.CurrentSide == CombatSide.Player;
            }
            case "Rewards": return top is NRewardsScreen;
            case "Choice": return cardReward ? top is NCardRewardSelectionScreen : top is NRewardsScreen;
            // The terminal rewards screen stays on the overlay stack under the open map until the next room is entered.
            case "Map": return NMapScreen.Instance is { IsOpen: true, IsTravelEnabled: true, IsTraveling: false };
            case "Event":
                return top == null && !CombatManager.Instance.IsInProgress && NMapScreen.Instance?.IsOpen != true
                    && _run.CurrentRoom is EventRoom room && room.LocalMutableEvent.CurrentOptions.Count > 0
                    && Room<Node>("EventRoom") is { } node
                    && UiHelperFindAll<NEventOptionButton>(node).Count >= room.LocalMutableEvent.CurrentOptions.Count;
            case "RestSite": return top == null && Room<NRestSiteRoom>("RestSiteRoom") != null && NMapScreen.Instance?.IsOpen != true;
            case "Shop":
                return top == null && NMapScreen.Instance?.IsOpen != true
                    && (Room<NMerchantRoom>("MerchantRoom") != null || (FakeShop() != null && FakeShopNode()?.Inventory?.Inventory != null));
            case "Treasure": return top == null && Room<NTreasureRoom>("TreasureRoom") != null && _chestOpened;
            default: return false;
        }
    }

    // The Fake Merchant event is a shop inside an event room, with a scene node of its own and relics only.
    private static FakeMerchant? FakeShop() =>
        _run.CurrentRoom is EventRoom { LocalMutableEvent: FakeMerchant { StartedFight: false } fake } ? fake : null;

    private static NFakeMerchant? FakeShopNode() =>
        Room<Node>("EventRoom") is { } node ? UiHelperFindAll<NFakeMerchant>(node).FirstOrDefault() : null;

    private static MerchantInventory ShopInventory() =>
        _run.CurrentRoom is MerchantRoom merchant ? merchant.GetLocalInventory()
        : FakeShop()?.Inventory ?? throw new PilotStop("a shop decision without a merchant");

    private static string Where()
    {
        IOverlayScreen? top = NOverlayStack.Instance?.Peek();
        return $"room={_run.CurrentRoom?.GetType().Name} overlay={top?.GetType().Name ?? "none"} map_open={NMapScreen.Instance?.IsOpen} "
            + $"combat={CombatManager.Instance.IsInProgress} executor_running={RunManager.Instance.ActionExecutor.IsRunning} live=\"{LiveDigest()}\"";
    }

    private static async Task OpenChestOnce()
    {
        if (!ReferenceEquals(_chestRoom, _run.CurrentRoom)) { _chestRoom = _run.CurrentRoom; _chestOpened = false; _treasureResolved = false; }
        if (_chestOpened || Room<NTreasureRoom>("TreasureRoom") is not { } room) return;
        if (room.GetNodeOrNull<NClickableControl>("Chest") is not { } chest) return;
        chest.ForceClick();
        _chestOpened = true;
        // NTreasureRoom.OpenChest gives the gold and shows the relics over the next moments.
        Stopwatch waited = Stopwatch.StartNew();
        while (waited.Elapsed.TotalSeconds < 5 && !room.ProceedButton.IsEnabled
               && !UiHelperFindAll<NTreasureRoomRelicHolder>(room).Any(holder => holder.IsEnabled && holder.Visible))
            await Frame();
    }

    private static async Task ProceedToMap()
    {
        // Only the room the run is in can need a proceed click, and only while nothing else is going on. The scene
        // node of the room just left stays in the tree for a moment after travel: clicking its proceed control then
        // reopens the map over the room that was just entered (seen in direct mode, where nothing says which
        // decision comes next).
        if (NMapScreen.Instance?.IsOpen == true || CombatManager.Instance.IsInProgress || RunManager.Instance.ActionExecutor.IsRunning)
        {
            await Frame();
            return;
        }
        if (_run.CurrentRoom is EventRoom { LocalMutableEvent.IsFinished: true } && Room<Node>("EventRoom") is { } eventRoom
            && UiHelperFindAll<NEventOptionButton>(eventRoom).FirstOrDefault(b => !b.Option.IsLocked && b.Option.IsProceed) is { } proceed)
        {
            Note("clicking the event's proceed option");
            proceed.ForceClick();
        }
        else if (_run.CurrentRoom is TreasureRoom && Room<NTreasureRoom>("TreasureRoom") is { } treasure)
        {
            await OpenChestOnce();  // an empty chest (Hook.ShouldGenerateTreasure) is still opened before leaving
            bool relicLeft = RunManager.Instance.TreasureRoomRelicSynchronizer.CurrentRelics is { Count: > 0 } && !_treasureResolved;
            if (treasure.ProceedButton.IsEnabled && (!_direct || !relicLeft))
            {
                Note("clicking the treasure room's proceed button");
                treasure.ProceedButton.ForceClick();
            }
        }
        else if (_run.CurrentRoom is RestSiteRoom && Room<NRestSiteRoom>("RestSiteRoom") is { } rest && rest.ProceedButton.IsEnabled
                 && (!_direct || _run.CurrentRoom is RestSiteRoom { Options.Count: 0 }))
        {
            Note("clicking the rest site's proceed button");
            rest.ProceedButton.ForceClick();
        }
        await Frame();
    }

    private static async Task Execute(string decision, JsonObject act, bool cardReward, bool nestedRewards, JsonObject? choice)
    {
        JsonObject action = (JsonObject)act["action"]!;
        string kind = (string)action["kind"]!;
        int index = (int)action["index"]!;
        int target = (int)action["target"]!;
        switch (kind)
        {
            case "MapMove":
            {
                int col = (int)action["col"]!, row = (int)action["row"]!;
                NMapScreen map = NMapScreen.Instance!;
                NMapPoint point = UiHelperFindAll<NMapPoint>(map).FirstOrDefault(p => p.Point.coord.col == col && p.Point.coord.row == row)
                    ?? throw new PilotStop($"no map point at ({col}, {row}) on the map screen");
                AbstractRoom? before = _run.CurrentRoom;
                int floor = _run.TotalFloor;
                point.ForceClick();
                await WaitUntil(() => _run.TotalFloor != floor || !ReferenceEquals(_run.CurrentRoom, before), 30, "the room to be entered after the map click");
                // The map screen stays open, with travel still enabled, for a moment after the room has been entered.
                // Read then, it looks like a second map decision: wait until the screen has really closed.
                await WaitUntil(() => NMapScreen.Instance?.IsOpen != true, 30, "the map screen to close after the room was entered");
                break;
            }
            case "PlayCard":
            {
                CardModel card = _player.PlayerCombatState!.Hand.Cards[index];
                RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(new PlayCardAction(card, Target(target)));
                await Settle();
                break;
            }
            case "EndTurn":
                RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(new EndPlayerTurnAction(_player, _player.PlayerCombatState!.TurnNumber));
                await Settle();
                break;
            case "UsePotion":
            {
                PotionModel potion = _player.PotionSlots[index] ?? throw new PilotStop($"no potion in slot {index}");
                Creature? creature = CombatManager.Instance.IsInProgress
                    ? Target(target)
                    : potion.TargetType is TargetType.Self or TargetType.AnyPlayer ? _player.Creature : null;
                potion.EnqueueManualUse(creature);
                await WaitUntil(() => !_player.PotionSlots.Contains(potion), 15, "the potion to be used");
                await Settle();
                break;
            }
            case "DiscardPotion":
            {
                PotionModel potion = _player.PotionSlots[index] ?? throw new PilotStop($"no potion in slot {index}");
                RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(
                    new DiscardPotionGameAction(_player, (uint)index, CombatManager.Instance.IsInProgress));
                await WaitUntil(() => !_player.PotionSlots.Contains(potion), 15, "the potion to be discarded");
                await Settle();
                break;
            }
            case "TakeReward":
                await TakeReward(index);
                break;
            case "Answer" when cardReward:
                await AnswerCardReward(index);
                break;
            case "Answer" when nestedRewards:
                if (index == (int)choice!["options"]! - 1) await ProceedFromRewards();
                else await TakeReward(index);
                break;
            case "Proceed":
                await Proceed(decision);
                break;
            case "EventOption":
            {
                EventRoom room = (EventRoom)_run.CurrentRoom!;
                EventModel ev = room.LocalMutableEvent;
                EventOption option = ev.CurrentOptions[index];
                Node node = Room<Node>("EventRoom")!;
                NEventOptionButton button = UiHelperFindAll<NEventOptionButton>(node).FirstOrDefault(b => ReferenceEquals(b.Option, option))
                    ?? throw new PilotStop($"no button for event option {index} of {ev.Id.Entry}");
                List<EventOption> shown = ev.CurrentOptions.ToList();
                button.ForceClick();
                // Until the page changes, a screen opens, a combat starts or the event is over.
                await WaitUntil(() => NOverlayStack.Instance?.ScreenCount > 0 || NMapScreen.Instance?.IsOpen == true
                    || CombatManager.Instance.IsInProgress || _run.CurrentRoom is not EventRoom || ev.IsFinished || _taken > 0 && _fatal != null
                    || !ev.CurrentOptions.SequenceEqual(shown), 20, "the event to respond to the chosen option");
                await Settle();
                break;
            }
            case "RestOption":
            {
                RestSiteRoom room = (RestSiteRoom)_run.CurrentRoom!;
                RestSiteOption option = room.Options[index];
                NRestSiteRoom node = Room<NRestSiteRoom>("RestSiteRoom")!;
                NRestSiteButton button = UiHelperFindAll<NRestSiteButton>(node).FirstOrDefault(b => ReferenceEquals(b.Option, option))
                    ?? throw new PilotStop($"no button for rest site option {index} ({option.OptionId})");
                int before = _taken;
                button.ForceClick();
                await WaitUntil(() => node.ProceedButton.IsEnabled || NOverlayStack.Instance?.ScreenCount > 0 || _taken != before, 20,
                    "the rest site option to respond");
                await Settle();
                break;
            }
            case "ShopBuy":
            {
                MerchantInventory inventory;
                if (FakeShop() is { } fake) inventory = fake.Inventory;
                else
                {
                    NMerchantRoom room = Room<NMerchantRoom>("MerchantRoom")!;
                    if (!room.Inventory.Visible) { room.OpenInventory(); await Frames(10); }
                    inventory = room.Inventory.Inventory ?? throw new PilotStop("the merchant has no inventory");
                }
                List<MerchantEntry> stocked = inventory.AllEntries.Where(entry => entry.IsStocked).ToList();
                if (index < 0 || index >= stocked.Count) throw new PilotStop($"no stocked shop entry {index}");
                _pendingPurchase = stocked[index].OnTryPurchaseWrapper(inventory);
                await Frames(2);
                break;
            }
            case "TreasurePick":
            {
                NTreasureRoom room = Room<NTreasureRoom>("TreasureRoom")!;
                NTreasureRoomRelicHolder holder = UiHelperFindAll<NTreasureRoomRelicHolder>(room).FirstOrDefault(h => h.Index == index && h.Visible)
                    ?? throw new PilotStop($"no treasure relic {index}");
                int relics = _player.Relics.Count;
                holder.ForceClick();
                await WaitUntil(() => _player.Relics.Count != relics, 15, "the treasure relic to be obtained");
                await Settle();
                break;
            }
            default:
                throw new PilotStop($"no way to perform {kind} at a {decision} decision");
        }
    }

    private static Creature? Target(int combatId)
    {
        if (combatId < 0) return null;
        CombatState combat = CombatManager.Instance.DebugOnlyGetState()!;
        return combat.Creatures.FirstOrDefault(c => c.CombatId == (uint)combatId) ?? throw new PilotStop($"no creature with combat id {combatId}");
    }

    private static RewardsSet RewardsOf(NRewardsScreen screen) =>
        (RewardsSet)AccessTools.Field(typeof(NRewardsScreen), "_rewardsSet").GetValue(screen)!;

    private static async Task TakeReward(int index)
    {
        NRewardsScreen screen = NOverlayStack.Instance?.Peek() as NRewardsScreen ?? throw new PilotStop("no rewards screen on top");
        RewardsSet set = RewardsOf(screen);
        if (index < 0 || index >= set.Rewards.Count) throw new PilotStop($"no reward {index}");
        Reward reward = set.Rewards[index];
        if (reward is CardReward opened) _activeCardReward = opened;
        NRewardButton button = UiHelperFindAll<NRewardButton>(screen).FirstOrDefault(b => ReferenceEquals(b.Reward, reward))
            ?? throw new PilotStop($"no button for reward {index} ({reward.GetType().Name})");
        button.ForceClick();
        // Taken, or its own screen (the card reward) opened on top. A reward that cannot be taken (a potion with every
        // slot full) leaves the screen as it was, in the game and in the shadow alike; the next digest check covers it.
        Stopwatch waited = Stopwatch.StartNew();
        bool responded = false;
        while (!responded && waited.Elapsed.TotalSeconds < 2)
        {
            await Frame();
            responded = NOverlayStack.Instance?.Peek() != screen || !GodotObject.IsInstanceValid(button) || !button.IsInsideTree() || !button.IsEnabled;
        }
        if (!responded) Note($"reward {index} ({reward.GetType().Name}) stayed on the screen after the click");
        await Settle();
    }

    private static async Task AnswerCardReward(int index)
    {
        NCardRewardSelectionScreen screen = NOverlayStack.Instance?.Peek() as NCardRewardSelectionScreen
            ?? throw new PilotStop("no card reward screen on top");
        List<NGridCardHolder> holders = screen.GetNode<Control>("UI/CardRow").GetChildren().OfType<NGridCardHolder>().ToList();
        if (index < holders.Count)
        {
            holders[index].EmitSignal(NCardHolder.SignalName.Pressed, holders[index]);
        }
        else
        {
            List<NCardRewardAlternativeButton> alternatives = screen.GetNode<Control>("UI/RewardAlternatives").GetChildren()
                .OfType<NCardRewardAlternativeButton>().ToList();
            int which = index - holders.Count;
            if (which >= alternatives.Count) throw new PilotStop($"the card reward screen has no alternative {which}");
            alternatives[which].ForceClick();
        }
        int deck = _player.Deck.Cards.Count;
        await WaitUntil(() => NOverlayStack.Instance?.Peek() != screen || !GodotObject.IsInstanceValid(screen)
            || _player.Deck.Cards.Count != deck, 15, "the card reward screen to respond");
        await Settle();
    }

    private static async Task ProceedFromRewards()
    {
        NRewardsScreen screen = NOverlayStack.Instance?.Peek() as NRewardsScreen ?? throw new PilotStop("no rewards screen on top");
        NProceedButton button = UiHelperFindAll<NProceedButton>(screen).FirstOrDefault() ?? throw new PilotStop("the rewards screen has no proceed button");
        button.ForceClick();
        await WaitUntil(() => !GodotObject.IsInstanceValid(screen) || NOverlayStack.Instance?.Peek() != screen || NMapScreen.Instance?.IsOpen == true,
            30, "the rewards screen to close");
        await Settle();
    }

    private static async Task Proceed(string decision)
    {
        switch (decision)
        {
            case "Rewards":
                await ProceedFromRewards();
                break;
            case "RestSite":
            {
                NRestSiteRoom room = Room<NRestSiteRoom>("RestSiteRoom")!;
                room.ProceedButton.ForceClick();
                await WaitUntil(() => NMapScreen.Instance?.IsOpen == true, 20, "the map to open after the rest site");
                break;
            }
            case "Shop" when FakeShop() != null:
            {
                NFakeMerchant node = FakeShopNode() ?? throw new PilotStop("the Fake Merchant's node is gone");
                NProceedButton leave = UiHelperFindAll<NProceedButton>(node).FirstOrDefault() ?? throw new PilotStop("the Fake Merchant has no proceed button");
                await WaitUntil(() => leave.IsEnabled, 10, "the Fake Merchant's proceed button");
                leave.ForceClick();
                await WaitUntil(() => NMapScreen.Instance?.IsOpen == true, 20, "the map to open after the Fake Merchant");
                break;
            }
            case "Shop":
            {
                NMerchantRoom room = Room<NMerchantRoom>("MerchantRoom")!;
                if (room.Inventory.Visible && UiHelperFindAll<NBackButton>(room).FirstOrDefault() is { } back) { back.ForceClick(); await Frames(10); }
                room.ProceedButton.ForceClick();
                await WaitUntil(() => NMapScreen.Instance?.IsOpen == true, 20, "the map to open after the shop");
                break;
            }
            case "Treasure":
            {
                NTreasureRoom room = Room<NTreasureRoom>("TreasureRoom")!;
                await WaitUntil(() => room.ProceedButton.IsEnabled, 10, "the treasure room's proceed button");
                room.ProceedButton.ForceClick();
                await WaitUntil(() => NMapScreen.Instance?.IsOpen == true, 20, "the map to open after the treasure room");
                break;
            }
            default:
                throw new PilotStop($"no way to proceed at a {decision} decision");
        }
    }

    // ---- selections the game asks for from inside an action ----

    private sealed class Selector : MegaCrit.Sts2.Core.TestSupport.ICardSelector
    {
        public async Task<IEnumerable<CardModel>> GetSelectedCards(IEnumerable<CardModel> options, int minSelect, int maxSelect)
        {
            List<CardModel> source = options.ToList();
            await WaitRelicsSettled();
            return Select(source, minSelect, maxSelect);
        }

        private static IEnumerable<CardModel> Select(List<CardModel> source, int minSelect, int maxSelect)
        {
            if (_direct)
            {
                try
                {
                    string asked = SelectionSource();
                    int count = DecideChoice(Observer.CardSelection(source, minSelect, maxSelect, asked, true, new List<int>()));
                    List<int> chosen = new();
                    for (int i = 0; i < count; i++)
                        chosen.Add(DecideChoice(Observer.CardSelection(source, minSelect, maxSelect, asked, false, chosen)));
                    return (chosen.Select(i => source[i]).ToList());
                }
                catch (Exception ex)
                {
                    _fatal ??= ex is PilotStop ? ex.Message : ex.ToString();
                    return (source.Take(Math.Min(Math.Max(minSelect, 0), source.Count)).ToList());
                }
            }
            try
            {
                string method = SelectionSource();
                CheckObservation("Choice:" + method + ":count",
                    () => Observer.Build(_run, _player, Observer.DChoice, Observer.CardSelection(source, minSelect, maxSelect, method, true, new List<int>())));
                JsonObject first = TakeNext();
                JsonObject choice = first["choice"] as JsonObject
                    ?? throw new PilotStop($"the game asks for a card selection, the pilot is at a {first["decision"]} decision");
                if ((string)choice["kind"]! != "CardSelect" || (int)choice["options"]! != source.Count)
                    throw new PilotStop($"the game offers {source.Count} cards, the pilot's selection is {choice.ToJsonString()}");
                List<CardModel> picked = new();
                int index = (int)first["action"]!["index"]!;
                if (!(bool)choice["subset"]!)
                {
                    if (index >= 0 && index < source.Count) picked.Add(source[index]);
                }
                else
                {
                    // An ordered subset: the count first, then one index at a time.
                    List<int> selected = new();
                    for (int i = 0; i < index; i++)
                    {
                        CheckObservation("Choice:" + method + ":index",
                            () => Observer.Build(_run, _player, Observer.DChoice, Observer.CardSelection(source, minSelect, maxSelect, method, false, selected)));
                        int pick = (int)TakeNext()["action"]!["index"]!;
                        selected.Add(pick);
                        picked.Add(source[pick]);
                    }
                }
                return (picked);
            }
            catch (Exception ex)
            {
                // The game is in the middle of an action: give it a legal answer and let the driver stop.
                _fatal ??= ex is PilotStop ? ex.Message : ex.ToString();
                int count = Math.Min(Math.Max(minSelect, 0), source.Count);
                return (source.Take(count).ToList());
            }
        }

        // The CardSelectCmd method that asked (the simulator's `source`), as the oracle driver finds it.
        private static string SelectionSource()
        {
            foreach (StackFrame frame in new StackTrace(false).GetFrames() ?? System.Array.Empty<StackFrame>())
            {
                MethodBase? method = frame.GetMethod();
                Type? type = method?.DeclaringType;
                // Async methods run inside a compiler-made state machine nested in CardSelectCmd: <FromHand>d__12.MoveNext.
                if (type?.DeclaringType?.Name == "CardSelectCmd" && type.Name.StartsWith('<'))
                    return type.Name.Substring(1, type.Name.IndexOf('>') - 1);
                if (type?.Name == "CardSelectCmd") return method!.Name;
            }
            return "CardSelectCmd";
        }

        public CardRewardSelection GetSelectedCardReward(IReadOnlyList<CardCreationResult> options, IReadOnlyList<CardRewardAlternative> alternatives)
        {
            // The game shows its card reward screen to a local player; this is only reached without one.
            _fatal ??= "the game asked the selector for a card reward";
            return default;
        }
    }

    // CardSelectCmd.FromChooseABundleScreen shows a screen; the pilot answers in its place, as the oracle driver does.
    private static bool ChooseBundle(Player player, IReadOnlyList<IReadOnlyList<CardModel>> bundles, ref Task<IEnumerable<CardModel>> __result)
    {
        if (_interactive && _interactiveSelector == null) return true;
        if (_pilot == null && _actor == null) return true;
        if (CombatManager.Instance.IsEnding || bundles.Count == 0)
        {
            __result = Task.FromResult<IEnumerable<CardModel>>(System.Array.Empty<CardModel>());
            return false;
        }
        Choice bundle = new() { Kind = 1, Source = "FromChooseABundleScreen", Prompt = "bundle" };
        foreach (IReadOnlyList<CardModel> cards in bundles)
            bundle.Options.Add(new ChoiceOption { Kind = 1, Card = cards.Count > 0 ? cards[0] : null, Cards = cards.Count });
        if (_direct)
        {
            try { __result = Task.FromResult<IEnumerable<CardModel>>(bundles[DecideChoice(bundle)]); }
            catch (Exception ex)
            {
                _fatal ??= ex is PilotStop ? ex.Message : ex.ToString();
                __result = Task.FromResult<IEnumerable<CardModel>>(bundles[0]);
            }
            return false;
        }
        try
        {
            CheckObservation("Choice:bundle", () => Observer.Build(_run, _player, Observer.DChoice, bundle));
            JsonObject act = TakeNext();
            if ((string?)act["choice"]?["kind"] != "Bundle") throw new PilotStop("the game asks for a bundle, the pilot is elsewhere");
            __result = Task.FromResult<IEnumerable<CardModel>>(bundles[(int)act["action"]!["index"]!]);
        }
        catch (Exception ex)
        {
            _fatal ??= ex is PilotStop ? ex.Message : ex.ToString();
            __result = Task.FromResult<IEnumerable<CardModel>>(bundles[0]);
        }
        return false;
    }

    private static bool PlayCrystalSphere(CrystalSphereMinigame __instance, ref Task __result)
    {
        if (_interactive && _interactiveSelector == null) return true;
        if (!_direct && _pilot == null) return true;
        __result = ResolveCrystalSphere(__instance);
        return false;
    }

    // The minigame as the Env asks it (game/Models/Events/CrystalSphere.h, CellRequest): one option per cell, row by
    // row, then the small and the big tool. The observation does not describe the grid (a known gap of nosl.obs v1),
    // so these tables are the same at every step of the minigame.
    private static Choice CrystalSphereChoice()
    {
        const int width = 11;
        Choice choice = new() { Kind = 2, Source = "crystal_sphere", Prompt = "CRYSTAL_SPHERE.minigame", Min = 1, Max = 1 };
        for (int y = 0; y < width; y++)
            for (int x = 0; x < width; x++)
                choice.Options.Add(new ChoiceOption { Kind = 3, Key = $"CRYSTAL_SPHERE.cell.{x}.{y}" });
        choice.Options.Add(new ChoiceOption { Kind = 3, Key = "CRYSTAL_SPHERE.button.DIVINATION_LABEL_SMALL" });
        choice.Options.Add(new ChoiceOption { Kind = 3, Key = "CRYSTAL_SPHERE.button.DIVINATION_LABEL_BIG" });
        return choice;
    }

    private static async Task ResolveCrystalSphere(CrystalSphereMinigame game)
    {
        const int width = 11;
        try
        {
            while (!game.IsFinished)
            {
                await WaitRelicsSettled();
                int pick;
                if (_direct) pick = DecideChoice(CrystalSphereChoice());
                else
                {
                    CheckObservation("Choice:crystal_sphere", () => Observer.Build(_run, _player, Observer.DChoice, CrystalSphereChoice()));
                    JsonObject act = TakeNext();
                    if ((string?)act["choice"]?["source"] != "crystal_sphere") throw new PilotStop("the game plays the Crystal Sphere, the pilot is elsewhere");
                    pick = (int)act["action"]!["index"]!;
                }
                if (pick == width * width) game.SetTool(CrystalSphereMinigame.CrystalSphereToolType.Small);
                else if (pick == width * width + 1) game.SetTool(CrystalSphereMinigame.CrystalSphereToolType.Big);
                else await game.CellClicked(game.cells[pick % width, pick / width]);
            }
        }
        catch (Exception ex)
        {
            _fatal ??= ex is PilotStop ? ex.Message : ex.ToString();
        }
        MethodInfo complete = AccessTools.Method(typeof(CrystalSphereMinigame), "CompleteMinigame")
            ?? throw new MissingMethodException(typeof(CrystalSphereMinigame).FullName, "CompleteMinigame");
        await (Task)complete.Invoke(game, null)!;
    }

    // ---- what the screen shows, in the pilot's words (nosl::Pilot::Digest) ----

    private static string LiveDigest()
    {
        Creature self = _player.Creature;
        string s = $"a{_run.CurrentActIndex} f{_run.TotalFloor} hp{self.CurrentHp}/{self.MaxHp} g{_player.Gold} d{_player.Deck.Cards.Count}"
            + $" r{_player.Relics.Count} p[{string.Join(",", _player.PotionSlots.Select(p => p?.Id.Entry ?? "-"))}]";
        PlayerCombatState? pcs = _player.PlayerCombatState;
        CombatState? combat = CombatManager.Instance.DebugOnlyGetState();
        // A lost combat stays "in progress" until the game next checks for its end (LoseCombat only marks the loss);
        // when the player dies at the end of their own turn the game-over screen is up before that. The simulator has
        // processed the loss by then, so a combat about to be lost is described as over here too.
        if (!CombatManager.Instance.IsInProgress || CombatManager.Instance.IsAboutToLose || pcs == null || combat == null) return s;
        string hand = string.Join(",", pcs.Hand.Cards.Select(c => c.Id.Entry + (c.CurrentUpgradeLevel > 0 ? "+" + c.CurrentUpgradeLevel : "")));
        string enemies = string.Join(",", combat.Enemies.Select(e => $"{e.Monster!.Id.Entry}:{e.CurrentHp}:{e.Block}"));
        return s + $" | b{self.Block} e{pcs.Energy} t{pcs.TurnNumber} h[{hand}] dr{pcs.DrawPile.Cards.Count} di{pcs.DiscardPile.Cards.Count}"
            + $" ex{pcs.ExhaustPile.Cards.Count} | m[{enemies}]";
    }

    // ---- small helpers ----

    private static readonly Dictionary<Type, FieldInfo?> RoomField = new();

    // The scene node of the room the run is in. For a moment after travel the node of the room just left is still at
    // the same path; with two rooms of one kind in a row (two treasure rooms) its chest and relic holders are the old
    // ones. A node that records which room it shows (NTreasureRoom, NRestSiteRoom: `_room`) is returned only when
    // that is the room the run is in now.
    private static T? Room<T>(string name) where T : Node
    {
        if (((SceneTree)Engine.GetMainLoop()).Root.GetNodeOrNull(RoomPath + name) is not T node) return null;
        Type type = node.GetType();
        if (!RoomField.TryGetValue(type, out FieldInfo? field))
            RoomField[type] = field = type.GetField("_room", BindingFlags.Instance | BindingFlags.NonPublic);
        if (field?.GetValue(node) is AbstractRoom shown && !ReferenceEquals(shown, _run.CurrentRoom)) return null;
        return node;
    }

    private static List<T> UiHelperFindAll<T>(Node start) where T : Node
    {
        List<T> found = new();
        void Walk(Node node)
        {
            if (!GodotObject.IsInstanceValid(node)) return;
            if (node is T item) found.Add(item);
            foreach (Node child in node.GetChildren()) Walk(child);
        }
        if (GodotObject.IsInstanceValid(start)) Walk(start);
        return found;
    }

    private static async Task Frame()
    {
        await _host.ToSignal(_host.GetTree(), SceneTree.SignalName.ProcessFrame);
        if (_interactive && _sessionActive && !CurrentSessionIsValid()) throw new PilotStop("对局已关闭或切换");
    }

    private static async Task Frames(int count)
    {
        for (int i = 0; i < count; i++) await Frame();
    }

    // The queued game actions have run (the action just requested included).
    private static async Task Settle()
    {
        await Frame();
        Task finished = RunManager.Instance.ActionExecutor.FinishedExecutingActions();
        if (_interactive) while (!finished.IsCompleted) await Frame();
        await finished;
        await Frame();
    }

    private static async Task WaitUntil(Func<bool> condition, double seconds, string what)
    {
        Stopwatch waited = Stopwatch.StartNew();
        while (!condition())
        {
            if (_interactive && NCapstoneContainer.Instance?.InUse == true)
            {
                waited.Stop();
                await Frame();
                continue;
            }
            waited.Start();
            if (waited.Elapsed.TotalSeconds > seconds) throw new PilotStop($"timed out after {seconds:0}s waiting for {what} ({Where()})");
            await Frame();
        }
    }

    private static JsonObject Parse(string line) => (JsonObject)JsonNode.Parse(line)!;

    private static void Note(string text) => Emit(new JsonObject { ["event"] = "note", ["text"] = text, ["t"] = Math.Round(Clock.Elapsed.TotalSeconds, 2) });

    private static void Emit(JsonObject value)
    {
        _log?.WriteLine(value.ToJsonString());
        if (_interactive) RecordPanelAction(value);
    }
}
