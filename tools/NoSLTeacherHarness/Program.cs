using System.Runtime.CompilerServices;
using System.Reflection;
using System.Text.Json;
using System.Text.Json.Nodes;
using CombatSolver;
using HarmonyLib;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Nodes;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.TestSupport;
using OfflineSearchHarness;

// A teacher is an offline privileged tool. None of this assembly is used by
// the public-observation student or the online NoSL executor.
internal static class NoSLTeacherProgram
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true,
        Converters = { new System.Text.Json.Serialization.JsonStringEnumConverter() }
    };

    private static int Main(string[] args)
    {
        Console.SetOut(Console.Error);
        var opts = new Dictionary<string, string>();
        for (int i = 0; i < args.Length; i++)
        {
            if (!args[i].StartsWith("--") || i + 1 >= args.Length) throw new ArgumentException("Expected --option value");
            if (!opts.TryAdd(args[i][2..], args[++i])) throw new ArgumentException("Duplicate option");
        }
        foreach (string key in opts.Keys)
            if (key is not ("seed" or "script" or "out" or "min-floor" or "beam" or "nodes" or "budget-ms" or "route" or "root-decision" or "capture-only"))
                throw new ArgumentException("Unknown teacher option: " + key);
        foreach (string key in new[] { "seed", "script", "out" })
            if (!opts.ContainsKey(key)) throw new ArgumentException("Missing --" + key);
        if (Environment.GetEnvironmentVariable("OFFLINE_HARNESS_COMBATSOLVER_DLL") is { Length: > 0 }
            || Environment.GetEnvironmentVariable("OFFLINE_HARNESS_RESOURCE_SETTINGS") is { Length: > 0 })
            throw new InvalidOperationException("Teacher refuses unrecorded solver overrides");
        global::GameBootstrap.InstallResolver(global::GameBootstrap.DefaultGameData);
        return Execute(opts);
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static int Execute(Dictionary<string, string> opts)
    {
        string output = Path.GetFullPath(opts["out"]);
        Directory.CreateDirectory(output);
        string resultPath = Path.Combine(output, "teacher.json");
        if (File.Exists(resultPath)) throw new InvalidOperationException("Refusing to overwrite teacher evidence");
        using StreamWriter trace = new(Path.Combine(output, "oracle.jsonl"));
        global::Program.Out = trace;
        bool captured = false;
        RoutePlayback? playback = null;
        Action? stopObservingHandSelection = null;
        int minFloor = int.Parse(opts.GetValueOrDefault("min-floor", "8"));
        int? rootDecision = opts.TryGetValue("root-decision", out string? requestedRoot)
            ? int.Parse(requestedRoot) : null;
        if (rootDecision < 0) throw new ArgumentException("Negative root decision");
        bool captureOnly = bool.Parse(opts.GetValueOrDefault("capture-only", "false"));
        if (captureOnly && opts.ContainsKey("route")) throw new ArgumentException("Capture-only cannot replay a route");
        OracleRun.CombatObserver = (combat, player, decision) =>
        {
            RunState run = RunManager.Instance.DebugOnlyGetState()!;
            int floor = (int)OracleRun.DumpRun(run, player)["total_floor"]!;
            if (captured || floor < minFloor || (rootDecision.HasValue && decision != rootDecision.Value)) return;
            captured = true;
            if (opts.TryGetValue("route", out string? routePath))
            {
                string searchDir = Path.GetDirectoryName(Path.GetFullPath(routePath))!;
                JsonObject source = (JsonObject)JsonNode.Parse(File.ReadAllText(routePath))!;
                if ((string?)source["status"] != "searched" || (string?)source["seed"] != opts["seed"]
                    || (int?)source["decision"] != decision || (bool?)source["all_enemies_dead"] != true
                    || (string?)source["solver_assembly_id"] != typeof(SolverController).Module.ModuleVersionId.ToString()
                    || !JsonNode.DeepEquals(Snapshot(run, combat, player), JsonNode.Parse(File.ReadAllText(Path.Combine(searchDir, "root-before.json")))))
                {
                    Write(resultPath, new { schema = "sts2.nosl.student-root-teacher.v1", status = "route_root_rejected",
                        seed = opts["seed"], decision, floor, oracle_test_mode = TestMode.IsOn,
                        reason = "Teacher replay does not match the frozen actual student root" });
                    throw new InvalidOperationException("Teacher replay does not match the frozen actual student root");
                }
                var actions = source["route_actions"]!.Deserialize<List<PlanAction>>(JsonOptions)!;
                playback = new RoutePlayback(combat, player, actions, (int)source["metrics"]!["finalHp"]!);
                OracleRun.CombatActionProvider = playback.Next;
                OracleRun.CardSelectionProvider = playback.Select;
                OracleRun.StopRequested = playback.Stop;
                stopObservingHandSelection = Headless.InstallHandSelectionObservation(
                    playback.HandSelectionMark, playback.ObserveHandSelection);
                Write(resultPath, new { schema = "sts2.nosl.student-root-teacher.v1", status = "native_replay_started",
                    seed = opts["seed"], decision, floor, oracle_test_mode = TestMode.IsOn,
                    replacement_decision_budget = OracleRun.ScriptDecisionLimit, teacher_route = Path.GetFullPath(routePath) });
            }
            else
            {
                SearchRoot(combat, player, run, decision, opts, output, resultPath);
                // Stop at the captured stable root, before EmitDecision can
                // execute the unused script carrier or any nested selection.
                if (captureOnly) OracleRun.StopRequested = () => true;
            }
        };
        try
        {
            if (opts.TryGetValue("route", out string? budgetRoute))
            {
                JsonObject source = (JsonObject)JsonNode.Parse(File.ReadAllText(budgetRoute))!;
                var planned = source["route_actions"]!.Deserialize<List<PlanAction>>(JsonOptions)!;
                int scriptCount = File.ReadLines(opts["script"]).Count(line => !string.IsNullOrWhiteSpace(line));
                int routeCount = planned.Sum(action => checked(1 + action.GetActionChoicesInExecutionOrder().Count
                    + (action.TurnStartChoices?.Count ?? 0)));
                OracleRun.ScriptDecisionLimit = checked(scriptCount + routeCount + 1);
            }
            int exit = Oracle.Run("run", new Dictionary<string, string>
            {
                ["seed"] = opts["seed"], ["character"] = "SILENT", ["ascension"] = "10",
                ["script"] = Path.GetFullPath(opts["script"]), ["stop"] = "victory", ["oracle-test-mode"] = "off"
            });
            trace.Flush();
            JsonNode? end = File.ReadLines(Path.Combine(output, "oracle.jsonl")).Select(line => JsonNode.Parse(line))
                .LastOrDefault(node => (string?)node?["event"] == "end");
            bool replayFailed = end == null || (string?)end["reason"] == "error";
            if (!captured)
            {
                Write(resultPath, new { schema = "sts2.nosl.student-root-teacher.v1",
                    status = replayFailed ? "replay_error" : "no_root", min_floor = minFloor, end });
                return replayFailed ? 4 : 3;
            }
            JsonObject evidence = (JsonObject)JsonNode.Parse(File.ReadAllText(resultPath))!;
            evidence["student_end"] = end?.DeepClone();
            if (playback != null && !replayFailed)
            {
                playback.AssertComplete();
                evidence["status"] = "native_route_replayed";
                evidence["actions_replayed"] = playback.ActionsReplayed;
                evidence["choices_replayed"] = playback.ChoicesReplayed;
                evidence["automatic_choices_replayed"] = playback.AutomaticChoicesReplayed;
                evidence["terminal_hp"] = playback.TerminalHp;
                evidence["predicted_hp_matched"] = true;
                evidence["bc_labels_qualified"] = false; // Env prefix comparison still required
            }
            Write(resultPath, evidence);
            return replayFailed ? 4 : exit;
        }
        finally
        {
            OracleRun.CombatObserver = null;
            OracleRun.CombatActionProvider = null;
            OracleRun.CardSelectionProvider = null;
            OracleRun.StopRequested = null;
            OracleRun.ScriptDecisionLimit = null;
            stopObservingHandSelection?.Invoke();
            ModRuntime.Session?.Dispose();
        }
    }

    private static void SearchRoot(CombatState combat, Player player, RunState run, int decision,
        Dictionary<string, string> opts, string output, string resultPath)
    {
        if (TestMode.IsOn) throw new InvalidOperationException("Teacher requires normal game rules");
        JsonObject before = Snapshot(run, combat, player);
        Write(Path.Combine(output, "root-before.json"), before);
        var evidence = new JsonObject
        {
            ["schema"] = "sts2.nosl.student-root-teacher.v1", ["status"] = "started",
            ["seed"] = opts["seed"], ["decision"] = decision, ["oracle_test_mode"] = TestMode.IsOn,
            ["floor"] = before["run"]!["total_floor"]!.DeepClone(),
            ["game_assembly_id"] = typeof(CombatState).Module.ModuleVersionId.ToString(),
            ["solver_assembly_id"] = typeof(SolverController).Module.ModuleVersionId.ToString()
        };
        Write(resultPath, evidence);
        try
        {
            // The real game initializes these as main-thread metadata and a
            // frozen empty mod-type registry. Neither changes a card/resource
            // or seeded RNG. Do not invoke upstream GameBootstrap, which
            // enables TestMode and substitutes empty localization tables.
            AccessTools.Field(typeof(NGame), "_mainThreadId").SetValue(null, Environment.CurrentManagedThreadId);
            AccessTools.Field(typeof(MegaCrit.Sts2.Core.Helpers.ReflectionHelper), "_modTypes")
                .SetValue(null, Array.Empty<Type>());
            AccessTools.PropertySetter(typeof(MegaCrit.Sts2.Core.Modding.ModManager), "State")
                .Invoke(null, new object[] { MegaCrit.Sts2.Core.Modding.ModManagerState.Initialized });
            SolverController.DisplayServerNameProvider = () => "headless";
            // ModRuntime needs its Harmony container, but initializing that
            // container does not require the upstream gameplay/bootstrap
            // bypasses. Install only its audited search-isolation patch set.
            AccessTools.Field(typeof(OfflineSearchHarness.GameBootstrap), "_harmony")
                .SetValue(null, new Harmony(OfflineSearchHarness.GameBootstrap.HarmonyId));
            TeacherPresentation.Install();
            string teacherDir = Path.Combine(output, "search");
            Directory.CreateDirectory(teacherDir);
            var settings = new HarnessOptions
            {
                OutputDirectory = teacherDir, Profile = "Custom", Beam = int.Parse(opts.GetValueOrDefault("beam", "30")),
                Nodes = int.Parse(opts.GetValueOrDefault("nodes", "10000")),
                BudgetMilliseconds = int.Parse(opts.GetValueOrDefault("budget-ms", "10000")),
                MaxDegreeOfParallelism = 1, SearchMode = "Evaluate"
            };
            evidence["initialization"] = ModRuntime.Initialize(settings);
            evidence["patches"] = JsonSerializer.SerializeToNode(ModRuntime.PatchLog, JsonOptions);
            Write(resultPath, evidence);
            if (ModRuntime.PatchLog.Any(line => line.Contains("失败", StringComparison.Ordinal)))
                throw new InvalidOperationException("A required search-isolation patch failed");
            SynchronizationContext? previous = SynchronizationContext.Current;
            ModRuntime.SearchOutcome outcome;
            using IDisposable neutralPresentation = TeacherPresentation.DeclareAuditedHeadlessType();
            try
            {
                MainLoopContext loop = new();
                SynchronizationContext.SetSynchronizationContext(loop);
                outcome = ModRuntime.RunSearchDetailed(combat, settings, loop, null);
            }
            finally { SynchronizationContext.SetSynchronizationContext(previous); }
            JsonObject after = Snapshot(run, combat, player);
            Write(Path.Combine(output, "root-after.json"), after);
            CombatRootSnapshot afterRoot = CombatRootSnapshot.Capture(combat);
            bool same = JsonNode.DeepEquals(before, after);
            bool stampsSame = outcome.RootContinuationStamp == afterRoot.ContinuationStamp.StateText
                && outcome.RootLiveStamp == afterRoot.LiveStamp.StateText;
            evidence["root_unchanged"] = same;
            evidence["root_stamps_unchanged"] = stampsSame;
            evidence["oracle_test_mode_after"] = TestMode.IsOn;
            evidence["metrics"] = JsonSerializer.SerializeToNode(outcome.LegacyMetrics, JsonOptions);
            evidence["policy"] = JsonSerializer.SerializeToNode(outcome.Policy, JsonOptions);
            evidence["route_actions"] = JsonSerializer.SerializeToNode(outcome.RouteActions, JsonOptions);
            evidence["wall_seconds"] = outcome.WallSeconds;
            evidence["time_boundary"] = outcome.TimeBoundaryObserved;
            evidence["all_enemies_dead"] = outcome.Result.Snapshot.AllEnemiesDead;
            evidence["player_dead"] = outcome.Result.Snapshot.PlayerDead;
            evidence["first_action"] = BindFirstAction(combat, player, outcome.Result.BestNode.Actions.FirstOrDefault());
            evidence["bc_labels_qualified"] = false; // full real route replay is required separately
            evidence["status"] = same && stampsSame && !TestMode.IsOn ? "searched" : "root_mutated";
            Write(resultPath, evidence);
            if (!same || !stampsSame || TestMode.IsOn) throw new InvalidOperationException("Search changed the live student root");
        }
        catch (Exception error)
        {
            evidence["status"] = "error";
            evidence["error"] = error.ToString();
            Write(resultPath, evidence);
            throw;
        }
    }

    private static JsonObject BindFirstAction(CombatState combat, Player player, PlanAction? action)
    {
        if (action == null) return new JsonObject { ["status"] = "empty_route" };
        var bound = new JsonObject { ["kind"] = action.Kind.ToString(), ["turn"] = action.Turn };
        if (action.Turn != player.PlayerCombatState!.TurnNumber) throw new InvalidOperationException("First action turn drift");
        if (action.Kind == PlanActionKind.PlayCard)
        {
            var hand = player.PlayerCombatState.Hand.Cards.ToList();
            CardModel card = SolverController.FindCardForDeployment(hand, action);
            var target = action.TargetCombatId is uint targetId ? combat.Creatures.Single(c => c.CombatId == targetId) : null;
            if (!card.CanPlay() || !card.IsValidTarget(target)) throw new InvalidOperationException("Teacher first card is illegal");
            bound["hand_index"] = hand.IndexOf(card);
            bound["card"] = card.Id.Entry;
            bound["target"] = action.TargetCombatId;
        }
        else if (action.Kind == PlanActionKind.UsePotion)
        {
            if (action.PotionSlot < 0 || action.PotionSlot >= player.PotionSlots.Count
                || player.PotionSlots[action.PotionSlot]?.Id.Entry != action.PotionId)
                throw new InvalidOperationException("Teacher first potion identity drift");
            bound["slot"] = action.PotionSlot;
            bound["potion"] = action.PotionId;
        }
        bound["action_choices"] = action.GetActionChoicesInExecutionOrder().Count;
        bound["status"] = "bound_identity";
        return bound;
    }

    private static JsonObject Snapshot(RunState run, CombatState combat, Player player)
        => new() { ["run"] = OracleRun.DumpRun(run, player), ["combat"] = OracleCombat.Dump(run, combat, player) };
    private static void Write(string path, object value)
        => File.WriteAllText(path, JsonSerializer.Serialize(value, JsonOptions) + "\n");
}
