using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.CompilerServices;
using System.Text.Json.Nodes;
using System.Threading.Tasks;
using HarmonyLib;
using Godot;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Context;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Events;
using MegaCrit.Sts2.Core.Nodes.CommonUi;
using MegaCrit.Sts2.Core.Nodes.Screens.MainMenu;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.TestSupport;

internal static partial class OracleEnvOperationProbe
{
    private static NAbandonRunConfirmPopup? _trialPopup;
    private static Task? _trialAbandonTask;
    private static int _trialEndCalls;
    private static bool _trialVictory;

    // Only replace the node boundary. The shipped DoubleDown still executes;
    // confirmation invokes the shipped popup handler and native Abandon/Kill.
    private static NModalContainer? TrialModal() => null;
    private static NAbandonRunConfirmPopup TrialPopup(NMainMenu? mainMenu)
    {
        if (mainMenu != null || TestMode.IsOn) throw new InvalidOperationException("Trial must use the in-run production popup");
        return (NAbandonRunConfirmPopup)RuntimeHelpers.GetUninitializedObject(typeof(NAbandonRunConfirmPopup));
    }
    private static void AddTrialModal(NModalContainer? container, Node popup, bool showBackstop)
    {
        if (_trialPopup != null || !showBackstop) throw new InvalidOperationException("Unexpected Trial modal state");
        _trialPopup = (NAbandonRunConfirmPopup)popup;
    }
    private static IEnumerable<CodeInstruction> TrialModalBoundary(IEnumerable<CodeInstruction> code)
    {
        var replacements = new Dictionary<MethodInfo, MethodInfo>
        {
            [AccessTools.PropertyGetter(typeof(NModalContainer), "Instance")] = AccessTools.Method(typeof(OracleEnvOperationProbe), nameof(TrialModal)),
            [AccessTools.Method(typeof(NAbandonRunConfirmPopup), "Create")] = AccessTools.Method(typeof(OracleEnvOperationProbe), nameof(TrialPopup)),
            [AccessTools.Method(typeof(NModalContainer), "Add")] = AccessTools.Method(typeof(OracleEnvOperationProbe), nameof(AddTrialModal))
        };
        var counts = replacements.Keys.ToDictionary(key => key, _ => 0);
        foreach (CodeInstruction ins in code)
        {
            if (ins.operand is MethodInfo method && replacements.TryGetValue(method, out var replacement) && ins.Calls(method))
            {
                counts[method]++;
                yield return new CodeInstruction(ins) { opcode = OpCodes.Call, operand = replacement };
            }
            else yield return ins;
        }
        if (counts.Values.Any(n => n != 1)) throw new InvalidOperationException("Trial modal boundary changed");
    }
    private static bool TrialPortrait(Player? _) => false;
    private static IEnumerable<CodeInstruction> TrialPortraitBoundary(IEnumerable<CodeInstruction> code)
    {
        var method = AccessTools.Method(typeof(LocalContext), nameof(LocalContext.IsMe), new[] { typeof(Player) });
        int count = 0;
        foreach (CodeInstruction ins in code)
        {
            if (ins.Calls(method))
            {
                count++;
                yield return new CodeInstruction(ins) { opcode = OpCodes.Call,
                    operand = AccessTools.Method(typeof(OracleEnvOperationProbe), nameof(TrialPortrait)) };
            }
            else yield return ins;
        }
        if (count == 0) throw new InvalidOperationException("Missing Trial portrait boundary");
    }
    private static void TrialAbandonTask(Task __result) => _trialAbandonTask = __result;
    private static void TrialEnded(bool isVictory) { _trialEndCalls++; _trialVictory = isVictory; }

    public static void RunTrialConfirmation(Dictionary<string, string> opts, SingleThreadContext sync)
    {
        // Read the prompt and button labels from the shipped _Ready IL, not
        // from the simulator. Only the node itself is unavailable headlessly.
        var keys = PatchProcessor.GetOriginalInstructions(AccessTools.Method(typeof(NAbandonRunConfirmPopup), "_Ready"))
            .Where(i => i.opcode == OpCodes.Ldstr).Select(i => (string)i.operand)
            .Where(s => s.StartsWith("ABANDON_RUN_CONFIRMATION.", StringComparison.Ordinal)
                || s.StartsWith("GENERIC_POPUP.", StringComparison.Ordinal)).ToArray();
        if (keys.Length != 4) throw new InvalidOperationException("Popup localization contract changed");
        var harmony = new Harmony("sts2sim.oracle.trial-input");
        harmony.Patch(AccessTools.Method(typeof(Trial), "DoubleDown"), transpiler: new HarmonyMethod(typeof(OracleEnvOperationProbe), nameof(TrialModalBoundary)));
        foreach (string method in new[] { "Accept", "AddVfxAnchoredToPortrait" })
            harmony.Patch(AccessTools.Method(typeof(Trial), method), transpiler: new HarmonyMethod(typeof(OracleEnvOperationProbe), nameof(TrialPortraitBoundary)));
        harmony.Patch(AccessTools.Method(typeof(RunManager), "AbandonInternal"), postfix: new HarmonyMethod(typeof(OracleEnvOperationProbe), nameof(TrialAbandonTask)));
        harmony.Patch(AccessTools.Method(typeof(RunManager), nameof(RunManager.OnEnded)), postfix: new HarmonyMethod(typeof(OracleEnvOperationProbe), nameof(TrialEnded)));
        try
        {
            foreach (string seed in Split(opts.GetValueOrDefault("seeds", "TRIALCONFIRM01,TRIALCONFIRM02")))
            foreach (string sequence in new[] { "event:1,event:1,answer:0", "event:1,event:1,answer:1,event:1,answer:1,event:1,answer:0", "event:1,event:1,answer:1,event:0" })
            foreach (bool preventDeath in new[] { false, true })
            {
                _trialPopup = null; _trialAbandonTask = null; _trialEndCalls = 0; _trialVictory = false;
                try
                {
                    var run = Oracle.StartRun(new Dictionary<string, string> { ["seed"] = seed, ["character"] = "SILENT", ["ascension"] = "10" });
                    Await(RunManager.Instance.EnterAct(0), sync);
                    var player = run.Players[0];
                    if (preventDeath) Await(RelicCmd.Obtain(ModelDb.GetById<RelicModel>(new ModelId("RELIC", "LIZARD_TAIL")).ToMutable(), player), sync);
                    run.CurrentActIndex = 2;
                    Await(RunManager.Instance.EnterRoom(new EventRoom(ModelDb.Event<Trial>())), sync);
                    var ev = (run.CurrentRoom as EventRoom ?? throw new InvalidOperationException("Missing Trial room")).LocalMutableEvent;
                    JsonObject Capture()
                    {
                        var result = State(run, player);
                        result["description"] = ev.Description!.LocEntryKey;
                        result["event_finished"] = ev.IsFinished;
                        result["event_options"] = new JsonArray(ev.CurrentOptions.Select(o => (JsonNode)new JsonObject
                        {
                            ["key"] = o.TextKey, ["locked"] = o.IsLocked, ["proceed"] = o.IsProceed,
                            ["chosen"] = o.WasChosen, ["will_kill_player"] = o.WillKillPlayer?.Invoke(player) ?? false
                        }).ToArray());
                        result["event_page_index"] = (uint)AccessTools.Field(typeof(MegaCrit.Sts2.Core.Multiplayer.Game.EventSynchronizer), "_pageIndex").GetValue(RunManager.Instance.EventSynchronizer)!;
                        result["is_abandoned"] = RunManager.Instance.IsAbandoned;
                        result["game_over"] = run.IsGameOver;
                        result["end_calls"] = _trialEndCalls;
                        result["victory"] = _trialVictory;
                        result["choice"] = _trialPopup == null ? null : new JsonObject
                        {
                            ["source"] = nameof(NAbandonRunConfirmPopup), ["prompt"] = keys[1],
                            ["keys"] = new JsonArray(keys.Skip(2).Select(k => (JsonNode?)JsonValue.Create(k)).ToArray())
                        };
                        return result;
                    }
                    var states = new JsonArray(Capture());
                    foreach (string action in sequence.Split(','))
                    {
                        var parts = action.Split(':');
                        int index = int.Parse(parts[1]);
                        if (parts[0] == "event")
                        {
                            // NEventRoom.OptionButtonClicked's direct callback
                            // branch must not advance the shared page counter.
                            if (ev.CurrentOptions[index].IsProceed) Await(ev.CurrentOptions[index].Chosen(), sync);
                            else
                            {
                                RunManager.Instance.EventSynchronizer.ChooseLocalOption(index);
                                Await(RunManager.Instance.EventSynchronizer.AwaitPendingOptionTasks(), sync);
                            }
                        }
                        else
                        {
                            var popup = _trialPopup ?? throw new InvalidOperationException("No Trial confirmation");
                            _trialPopup = null;
                            AccessTools.Method(typeof(NAbandonRunConfirmPopup), index == 0 ? "OnYesButtonPressed" : "OnNoButtonPressed").Invoke(popup, new object?[] { null });
                            if (index == 0) Await(_trialAbandonTask ?? throw new InvalidOperationException("Native AbandonInternal was not called"), sync);
                        }
                        states.Add(Capture());
                    }
                    Program.Out.WriteLine(new JsonObject { ["seed"] = seed, ["sequence"] = sequence, ["prevent_death"] = preventDeath,
                        ["test_mode"] = TestMode.IsOn, ["states"] = states }.ToJsonString());
                }
                finally { RunManager.Instance.CleanUp(); }
            }
        }
        finally { harmony.UnpatchAll(harmony.Id); }
    }
}
