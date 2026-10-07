using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;
using HarmonyLib;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Events.Custom.CrystalSphereEvent;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Events;
using MegaCrit.Sts2.Core.Rewards;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.TestSupport;

internal static partial class OracleEnvOperationProbe
{
    private sealed record CrystalContext(RunState Run, Probe Probe, int[] Answers)
    {
        public CrystalSphereMinigame? Game { get; set; }
        public List<Exception> Errors { get; } = new();
    }
    private static CrystalContext? _crystal;

    // UI input is replaced, not placement/click/reveal/reward logic. The
    // original EventOption.Chosen awaits this task, then finishes its page.
    private static bool PlayCrystalApi(CrystalSphereMinigame __instance, ref Task __result)
    {
        if (_crystal is not { } context) return true;
        context.Game = __instance;
        __result = DriveCrystal(__instance, context);
        return false;
    }

    private static async Task DriveCrystal(CrystalSphereMinigame game, CrystalContext context)
    {
        foreach (int answer in context.Answers)
        {
            if (game.IsFinished) throw new ArgumentException("Click sequence continues after the final divination");
            context.Probe.Choices.Add(new JsonObject
            {
                ["kind"] = "crystal_sphere", ["selected_index"] = answer,
                ["state"] = State(context.Run, context.Run.Players[0])
            });
            if (answer == 121) game.SetTool(CrystalSphereMinigame.CrystalSphereToolType.Small);
            else if (answer == 122) game.SetTool(CrystalSphereMinigame.CrystalSphereToolType.Big);
            else await game.CellClicked(game.cells[answer % 11, answer / 11]);
        }
        if (!game.IsFinished) throw new ArgumentException("Click sequence must include every divination");
        // Original method invokes OneOffSynchronizer.DoLocalCrystalSphereRewards,
        // item.ToReward and RewardsCmd.OfferCustom; no reward reconstruction.
        await (Task)AccessTools.Method(typeof(CrystalSphereMinigame), "CompleteMinigame").Invoke(game, null)!;
    }

    private static JsonNode? CrystalSphereState()
    {
        if (_crystal?.Game is not { } game) return null;
        var grid = new JsonArray();
        for (int y = 0; y < 11; y++) for (int x = 0; x < 11; x++)
        {
            CrystalSphereCell cell = game.cells[x, y];
            int index = cell.Item == null ? -1 : game.Items.ToList().IndexOf(cell.Item);
            grid.Add(index < 0 ? cell.IsHidden ? 0 : 1 : index + (cell.IsHidden ? 2 : 18));
        }
        var items = new JsonArray();
        for (int index = 0; index < game.Items.Count; index++)
        {
            var item = game.Items[index];
            var serialized = item.ToSerializable();
            string kind = serialized.type.ToString() switch
            {
                "Relic" => "relic", "Potion" => "potion", "CardReward" => "card",
                "Curse" => "curse", "Gold" => "gold", _ => throw new NotSupportedException("Crystal item type")
            };
            items.Add(new JsonObject
            {
                ["index"] = index, ["kind"] = kind, ["width"] = item.Size.X, ["height"] = item.Size.Y,
                ["value"] = kind == "potion" ? (int)serialized.potionRarity : kind == "card" ? (int)serialized.cardRarity
                    : kind == "gold" ? serialized.isBigGold ? 30 : 10 : 0,
                ["x"] = item.Position.X, ["y"] = item.Position.Y
            });
        }
        var revealed = (System.Collections.IEnumerable)AccessTools.Field(typeof(CrystalSphereMinigame), "_revealed").GetValue(game)!;
        var order = new JsonArray();
        int mask = 0;
        foreach (object item in revealed)
        {
            int index = game.Items.ToList().FindIndex(i => ReferenceEquals(i, item));
            order.Add(index);
            mask |= 1 << index;
        }
        return new JsonObject
        {
            ["visibility"] = "privileged", ["width"] = 11, ["height"] = 11,
            ["active"] = !game.IsFinished,
            ["tool"] = game.CrystalSphereTool == CrystalSphereMinigame.CrystalSphereToolType.Small ? 1 : 2,
            ["divinations_remaining"] = game.DivinationCount, ["revealed_bits"] = mask,
            ["grid"] = grid, ["revealed_order"] = order, ["items"] = items
        };
    }

    public static void RunCrystalSphere(Dictionary<string, string> opts, SingleThreadContext sync)
    {
        string[] seeds = Split(opts.GetValueOrDefault("seeds", "RFCRYSTAL0001"));
        string[] choices = Split(opts.GetValueOrDefault("event-options", "0,1"));
        string[] sequences = opts.GetValueOrDefault("answer-orders", "60,26,38,47,121,17,122,30").Split(';');
        var harmony = new Harmony("sts2sim.oracle.crystal-api-input");
        var method = AccessTools.Method(typeof(CrystalSphereMinigame), "PlayMinigame");
        var prefix = AccessTools.Method(typeof(OracleEnvOperationProbe), nameof(PlayCrystalApi));
        harmony.Patch(method, prefix: new HarmonyMethod(prefix));
        try
        {
            foreach (string seed in seeds) foreach (string choice in choices) foreach (string sequence in sequences)
            {
                int option = int.Parse(choice);
                int[] answers = sequence.Split(',').Select(int.Parse).ToArray();
                if (option is not (0 or 1) || answers.Any(i => i < 0 || i > 122)) throw new ArgumentException("Crystal input out of range");
                // Use the requested prefix length for each event option. Tool
                // switches do not consume divinations; repeated clicks do.
                int count = option == 0 ? 3 : 6, cells = 0;
                answers = answers.TakeWhile(i => { if (cells == count) return false; if (i < 121) cells++; return true; }).ToArray();
                if (cells != count) throw new ArgumentException("Insufficient click inputs");
                Func<RewardsSet, Task>? previous = RewardsSet.testSelector;
                Action<Exception>? fault = null;
                try
                {
                    RunState run = Oracle.StartRun(new Dictionary<string, string>
                    {
                        ["seed"] = seed, ["character"] = opts.GetValueOrDefault("character", "IRONCLAD"),
                        ["ascension"] = opts.GetValueOrDefault("ascension", "0")
                    });
                    Await(RunManager.Instance.EnterAct(0), sync);
                    var player = run.Players[0];
                    int gold = int.Parse(opts.GetValueOrDefault("gold", "300"));
                    Await(PlayerCmd.SetGold(gold, player), sync);
                    run.CurrentActIndex = 1;
                    Await(RunManager.Instance.EnterRoom(new EventRoom(ModelDb.Event<CrystalSphere>())), sync);
                    var probe = new Probe(run, false);
                    _crystal = new CrystalContext(run, probe, answers);
                    fault = error => _crystal.Errors.Add(error);
                    TaskHelper.UnobservedFault += fault;
                    RewardsSet.testSelector = probe.SelectRewards;
                    var result = new JsonObject
                    {
                        ["format"] = "sts2oracle.env_crystal_sphere", ["version"] = 1,
                        ["seed"] = seed, ["character"] = player.Character.Id.Entry, ["ascension"] = run.AscensionLevel,
                        ["case_id"] = $"crystal:{option}:{sequence}", ["event_option"] = option,
                        ["fixture_gold"] = gold, ["fixture"] = "new_run_neow_replaced_by_act_2_crystal_sphere",
                        ["oracle_test_mode"] = TestMode.IsOn, ["source"] = State(run, player)
                    };
                    using (CardSelectCmd.UseSelector(probe))
                    {
                        RunManager.Instance.EventSynchronizer.ChooseLocalOption(option);
                        Await(RunManager.Instance.EventSynchronizer.AwaitPendingOptionTasks(), sync);
                    }
                    if (_crystal.Errors.Count != 0) throw new AggregateException(_crystal.Errors);
                    result["choices"] = probe.Choices;
                    result["final"] = State(run, player);
                    result["event_finished"] = (run.CurrentRoom as EventRoom
                        ?? throw new InvalidOperationException("Crystal event room was unexpectedly left")).LocalMutableEvent.IsFinished;
                    Program.Out.WriteLine(result.ToJsonString());
                }
                finally
                {
                    if (fault != null) TaskHelper.UnobservedFault -= fault;
                    _crystal = null;
                    RewardsSet.testSelector = previous;
                    CardSelectCmd.Reset();
                    RunManager.Instance.CleanUp();
                }
            }
        }
        finally { harmony.Unpatch(method, prefix); }
    }
}
