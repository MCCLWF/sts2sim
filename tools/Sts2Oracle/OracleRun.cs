using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.CompilerServices;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using HarmonyLib;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Context;
using MegaCrit.Sts2.Core.Events.Custom.CrystalSphereEvent;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Rngs;
using MegaCrit.Sts2.Core.Entities.Merchant;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Entities.Potions;
using MegaCrit.Sts2.Core.Entities.RestSite;
using MegaCrit.Sts2.Core.Entities.TreasureRelicPicking;
using MegaCrit.Sts2.Core.Events;
using MegaCrit.Sts2.Core.GameActions;
using MegaCrit.Sts2.Core.Hooks;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Map;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Events;
using MegaCrit.Sts2.Core.Models.Monsters;
using MegaCrit.Sts2.Core.Models.Relics;
using MegaCrit.Sts2.Core.Multiplayer.Game;
using MegaCrit.Sts2.Core.Nodes;
using MegaCrit.Sts2.Core.Random;
using MegaCrit.Sts2.Core.Rewards;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Nodes.Vfx.Utilities;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Saves;
using MegaCrit.Sts2.Core.TestSupport;

// `run`: drive a seeded policy through every decision point to the requested boundary.
// This file intentionally uses the game's model entry points (synchronizers,
// Commands, and room methods), so the trace is a replayable protocol rather than
// a second implementation of room logic.
internal static class OracleRun
{
    // Offline teacher observer. The normal oracle leaves it null. Called at
    // the real static Play boundary after enumerating native legal actions;
    // observer implementations must prove they leave the live run unchanged.
    internal static Action<CombatState, Player, int>? CombatObserver;
    // Offline consumers may replace actions at a frozen root. All returned
    // actions still pass this driver's ordinary native legality checks and
    // execute through the real game's action queue. Null preserves the script.
    internal static Func<CombatState, Player, JsonObject?>? CombatActionProvider;
    internal static Func<IReadOnlyList<CardModel>, int, int, IReadOnlyList<int>?>? CardSelectionProvider;
    internal static Func<bool>? StopRequested;
    // An offline consumer replacing the remainder of a frozen script needs
    // its own finite decision budget. Null preserves script.Count exactly;
    // this only limits the driver, never changes original game execution.
    internal static int? ScriptDecisionLimit;
    // `run --script FILE`: the actions come from a file instead of the policy
    // RNG, one JSON object per line in the format of this driver's own
    // "action" events. Each decision site takes the next action and refuses
    // one of another kind or one the game does not offer, so a script that
    // the game cannot follow ends the trace with an error at that decision.
    // The game logic that runs is the same as under the random policy.
    private sealed class ActionScript
    {
        private readonly List<JsonObject> _actions = new();
        private int _next;

        public ActionScript(string path)
        {
            foreach (string line in System.IO.File.ReadLines(path))
                if (line.Trim().Length > 0) _actions.Add((JsonObject)JsonNode.Parse(line)!);
        }

        public int Count => _actions.Count;
        public bool Exhausted => _next >= _actions.Count;
        public bool NextIs(string kind) =>
            !Exhausted && (string?)_actions[_next]["kind"] == kind;

        public JsonObject Take(string kind)
        {
            if (_next >= _actions.Count)
                throw new InvalidOperationException($"script ended, but the game asks for a {kind} action");
            JsonObject action = _actions[_next++];
            string actual = (string?)action["kind"] ?? "";
            if (actual != kind)
                throw new InvalidOperationException($"script action {_next - 1} is {actual}, but the game asks for {kind}");
            return action;
        }

        // The next action when it is a potion action for a `kind` screen
        // ({"kind":kind,"potion":{"type":"use"|"discard","slot":n}}), else null.
        public JsonObject? PeekPotion(string kind) =>
            _next < _actions.Count && (string?)_actions[_next]["kind"] == kind && _actions[_next]["potion"] is JsonObject
                ? _actions[_next] : null;

        public InvalidOperationException Refuse(string why) =>
            new($"script action {_next - 1}: {why}");
    }

    // A script that stops in the middle of a room (a prefix of a longer
    // trajectory) ends the trace before the next decision is emitted.
    private sealed class ScriptEnded : Exception
    {
        public ScriptEnded() : base("the script has no more actions") { }
    }

    private static bool IsScriptEnd(Exception? ex)
    {
        for (; ex != null; ex = ex.InnerException)
        {
            if (ex is ScriptEnded) return true;
            if (ex is AggregateException many && many.InnerExceptions.Any(IsScriptEnd)) return true;
        }
        return false;
    }

    // The potion popup works on every screen outside combat too. A script may
    // use or discard a potion before it answers such a screen; the screen then
    // shows its decision again. Returns the potion that must leave its slot,
    // or null when the next scripted action is not a potion action. The
    // action can complete inside the enqueue call itself.
    private static PotionModel? BeginScriptedPotion(Controller controller, Player player, string kind)
    {
        if (controller.Script?.PeekPotion(kind) == null) return null;
        JsonObject action = controller.Script.Take(kind);
        JsonObject what = (JsonObject)action["potion"]!;
        int slot = (int)what["slot"]!;
        PotionModel? potion = slot >= 0 && slot < player.PotionSlots.Count ? player.PotionSlots[slot] : null;
        if (potion == null || potion.IsQueued || !player.CanUseOrRemovePotions)
            throw controller.Script.Refuse("no potion can be used or removed in that slot");
        Emit((JsonObject)JsonNode.Parse(action.ToJsonString())!);
        if ((string?)what["type"] == "discard")
        {
            // NPotionPopup passes whether a combat is in progress; outside combat the action is a NonCombat one.
            RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(new DiscardPotionGameAction(player, (uint)slot, CombatManager.Instance.IsInProgress));
            return potion;
        }
        Creature? target = potion.TargetType is TargetType.Self or TargetType.AnyPlayer ? player.Creature : null;
        if (potion.Usage != PotionUsage.AnyTime || !potion.PassesCustomUsabilityCheck || !potion.IsValidTarget(target))
            throw controller.Script.Refuse("the potion cannot be used here");
        potion.EnqueueManualUse(target);
        return potion;
    }

    private static bool TryScriptedPotion(Controller controller, Player player, string kind)
    {
        PotionModel? potion = BeginScriptedPotion(controller, player, kind);
        if (potion == null) return false;
        // The action can also wait on a timer, so pump for a bounded time.
        Stopwatch waited = Stopwatch.StartNew();
        while (player.PotionSlots.Contains(potion) && waited.ElapsedMilliseconds < 10000)
        {
            controller.Sync.Pump();
            Thread.Sleep(1);
        }
        if (player.PotionSlots.Contains(potion)) throw controller.Script!.Refuse("the potion action never ran");
        return true;
    }

    private sealed class Controller : ICardSelector
    {
        private readonly RunState _state;
        private readonly Player _player;
        private readonly Random _policy;
        internal readonly SingleThreadContext Sync;
        private readonly int _max;
        private int _forcedSacrificesRemaining;
        private bool _rewardProceedSelected;
        public bool VictoryRecorded { get; private set; }
        public int Decisions;
        public bool LegacyPolicy { get; }
        private readonly bool _legalRewardAlternatives;
        private Reward? _activeReward;
        public ActionScript? Script;

        public Controller(RunState state, Random policy, SingleThreadContext sync, int max, int forcedSacrifices, bool legacyPolicy, bool legalRewardAlternatives)
        {
            _state = state;
            _player = state.Players[0];
            _policy = policy;
            Sync = sync;
            _max = max;
            _forcedSacrificesRemaining = forcedSacrifices;
            LegacyPolicy = legacyPolicy;
            _legalRewardAlternatives = legalRewardAlternatives;
        }

        public async Task SelectReward(Reward reward)
        {
            Reward? previous = _activeReward;
            _activeReward = reward;
            try { await RunManager.Instance.RewardsSetSynchronizer.SelectLocalReward(reward); }
            finally { _activeReward = previous; }
        }

        public void MarkRewardProceedSelected() => _rewardProceedSelected = true;

        public bool ConsumeRewardProceedSelected()
        {
            bool selected = _rewardProceedSelected;
            _rewardProceedSelected = false;
            return selected;
        }

        public void MarkVictoryRecorded() => VictoryRecorded = true;

        public int ChooseCrystalSphereOption()
        {
            const int width = 11;
            const int cellCount = width * width;
            const int optionCount = cellCount + 2;
            var options = new JsonArray();
            int index = 0;
            for (int y = 0; y < width; y++)
            for (int x = 0; x < width; x++)
                options.Add(new JsonObject { ["index"] = index++, ["key"] = $"CRYSTAL_SPHERE.cell.{x}.{y}", ["kind"] = "alternative" });
            options.Add(new JsonObject { ["index"] = index++, ["key"] = "CRYSTAL_SPHERE.button.DIVINATION_LABEL_SMALL", ["kind"] = "alternative" });
            options.Add(new JsonObject { ["index"] = index, ["key"] = "CRYSTAL_SPHERE.button.DIVINATION_LABEL_BIG", ["kind"] = "alternative" });
            EmitDecision("choice", new JsonObject
            {
                ["options"] = new JsonObject
                {
                    ["source"] = "crystal_sphere",
                    ["prompt"] = "CRYSTAL_SPHERE.minigame",
                    ["option_count"] = optionCount,
                    ["min_select"] = 1,
                    ["max_select"] = 1,
                    ["subset"] = false,
                    ["can_skip"] = false,
                    ["options"] = options
                }
            });
            int choice = Script == null ? _policy.Next(optionCount) : (int)Script.Take("choice")["index"]!;
            if (choice < 0 || choice >= optionCount) throw Script!.Refuse("no such Crystal Sphere option");
            Emit(new JsonObject { ["event"] = "action", ["kind"] = "choice", ["index"] = choice });
            return choice;
        }

        public async Task<IEnumerable<CardModel>> GetSelectedCards(IEnumerable<CardModel> options, int minSelect, int maxSelect)
        {
            List<CardModel> source = options.ToList();
            List<CardModel> list = source.ToList();
            List<int> indexes = new List<int>();
            if (Script != null)
            {
                // The decision is emitted below, after the selection, as under the policy.
                indexes = CardSelectionProvider?.Invoke(source, minSelect, maxSelect)?.ToList()
                    ?? Script.Take("card_select")["indices"]!.AsArray().Select(i => (int)i!).ToList();
                int most = Math.Min(maxSelect, source.Count), least = Math.Min(minSelect, source.Count);
                if (most < minSelect) most = least;
                if (indexes.Count < least || indexes.Count > most || indexes.Distinct().Count() != indexes.Count
                    || indexes.Any(i => i < 0 || i >= source.Count))
                    throw Script.Refuse("the card selection is not a legal one");
                list = indexes.Select(i => source[i]).ToList();
            }
            else if (list.Count > 0)
            {
                int count = Math.Min(maxSelect, list.Count);
                if (count < minSelect) count = Math.Min(minSelect, list.Count);
                if (LegacyPolicy)
                    list = list.Select((c, i) => (c, i)).OrderBy(_ => _policy.Next()).Select(x => x.c).ToList();
                else
                {
                    count = _policy.Next(Math.Min(minSelect, list.Count), count + 1);
                    // Fisher-Yates uses only the policy RNG. The ordered prefix
                    // covers every legal cardinality and selection order.
                    for (int i = list.Count - 1; i > 0; --i)
                    {
                        int j = _policy.Next(i + 1);
                        (list[i], list[j]) = (list[j], list[i]);
                    }
                }
                indexes = list.Take(count).Select(c => source.IndexOf(c)).ToList();
                list = list.Take(count).ToList();
            }
            EmitDecision("card_select", new JsonObject
            {
                ["options"] = new JsonObject
                {
                    ["cards"] = Cards(options),
                    ["min"] = minSelect,
                    ["max"] = maxSelect,
                    ["can_skip"] = minSelect == 0,
                    ["source"] = SelectionSource()
                }
            });
            Emit(new JsonObject { ["event"] = "action", ["kind"] = "card_select", ["indices"] = new JsonArray(indexes.Select(i => (JsonNode)i).ToArray()) });
            await Task.CompletedTask;
            return list;
        }

        public CardRewardSelection GetSelectedCardReward(IReadOnlyList<CardCreationResult> options, IReadOnlyList<MegaCrit.Sts2.Core.Entities.CardRewardAlternatives.CardRewardAlternative> alternatives)
        {
            CardReward? sourceReward = _activeReward as CardReward;
            if (_legalRewardAlternatives && sourceReward == null)
                throw new InvalidOperationException("Card reward selection has no active native reward instance");
            // OnSelect retains its backend callback list, whereas Populate
            // refreshes the real UI with Generate after Reroll disables itself.
            // Keep callback indices for replay, sampling only native UI options.
            List<int> legalIndices = Enumerable.Range(0, options.Count).ToList();
            if (!LegacyPolicy)
                legalIndices.AddRange(Enumerable.Range(0, alternatives.Count)
                    .Where(i => !_legalRewardAlternatives || alternatives[i].OptionId != "REROLL" || sourceReward!.CanReroll)
                    .Select(i => options.Count + i));
            int forcedAlternative = _forcedSacrificesRemaining > 0
                ? alternatives.Select((alternative, index) => (alternative, index))
                    .Where(item => item.alternative.OptionId == "SACRIFICE")
                    .Select(item => item.index).DefaultIfEmpty(-1).First()
                : -1;
            int index;
            if (Script != null)
            {
                JsonArray chosen = Script.Take("card_select")["indices"]!.AsArray();
                index = chosen.Count == 0 ? -1 : (int)chosen[0]!;
                if (chosen.Count > 1 || (index >= 0 && !legalIndices.Contains(index)))
                    throw Script.Refuse("the card reward does not offer that option");
            }
            else index = forcedAlternative >= 0 ? options.Count + forcedAlternative
                : legalIndices.Count == 0 ? -1 : legalIndices[_policy.Next(legalIndices.Count)];
            if (forcedAlternative >= 0) _forcedSacrificesRemaining--;
            string[] alternativeIds = alternatives.Select(alternative => alternative.OptionId).ToArray();
            EmitDecision("card_select", new JsonObject
            {
                ["options"] = new JsonObject
                {
                    ["cards"] = Cards(options.Select(x => x.Card)),
                    ["min"] = 0,
                    ["max"] = 1,
                    ["can_skip"] = true,
                    ["can_reroll"] = alternativeIds.Contains("REROLL", StringComparer.Ordinal),
                    ["alternatives"] = new JsonArray(alternativeIds.Select(id => (JsonNode?)JsonValue.Create(id)).ToArray()),
                    ["source"] = SelectionSource()
                }
            });
            var action = new JsonObject { ["event"] = "action", ["kind"] = "card_select", ["indices"] = index < 0 ? new JsonArray() : new JsonArray(index) };
            if (_legalRewardAlternatives)
            {
                action["legal_reward_indices"] = new JsonArray(legalIndices.Select(i => (JsonNode?)JsonValue.Create(i)).ToArray());
                action["native_reward_can_reroll"] = sourceReward!.CanReroll;
            }
            Emit(action);
            return index >= options.Count && index >= 0
                ? new CardRewardSelection { alternative = alternatives[index - options.Count] }
                : new CardRewardSelection { card = index < 0 ? null : options[index].Card };
        }

        private static string SelectionSource()
        {
            foreach (StackFrame frame in new StackTrace(false).GetFrames() ?? Array.Empty<StackFrame>())
            {
                MethodBase? method = frame.GetMethod();
                if (method?.DeclaringType?.Name == "CardSelectCmd") return method.Name;
            }
            return "CardSelectCmd";
        }

        private void EmitDecision(string kind, JsonObject fields)
        {
            if (StopRequested?.Invoke() == true) throw new ScriptEnded();
            if (Script is { Exhausted: true }) throw new ScriptEnded();
            Decisions++;
            fields["event"] = "decision";
            fields["kind"] = kind;
            fields["run"] = DumpRun(_state, _player);
            Emit(fields);
        }
    }

    // The run being recorded (for the patches below).
    private static (Controller controller, Random policy, RunState state, Player player)? _current;
    private static bool _patched;
    private static NGame? _headlessGame;

    // CardSelectCmd.FromChooseABundleScreen takes bundle 0 under TestMode; the
    // client shows NChooseABundleSelectionScreen instead. The oracle asks its
    // policy, as the screen asks the player (docs/audits/deviations.md O-014).
    private static void PatchChoiceScreens()
    {
        if (_patched) return;
        _patched = true;
        new Harmony("sts2oracle.run").Patch(
            AccessTools.Method(typeof(CardSelectCmd), nameof(CardSelectCmd.FromChooseABundleScreen)),
            prefix: new HarmonyMethod(typeof(OracleRun), nameof(ChooseBundle)));
        new Harmony("sts2oracle.run.victory").Patch(
            AccessTools.Method(typeof(RunManager), nameof(RunManager.OnEnded)),
            postfix: new HarmonyMethod(typeof(OracleRun), nameof(RunEndedPostfix)));
        Harmony headless = new("sts2oracle.run.headless-presentation");
        headless.Patch(
            AccessTools.Method(typeof(CrystalSphereMinigame), nameof(CrystalSphereMinigame.PlayMinigame)),
            prefix: new HarmonyMethod(typeof(OracleRun), nameof(PlayCrystalSphereHeadless)));
        headless.Patch(
            AccessTools.PropertyGetter(typeof(NGame), nameof(NGame.Instance)),
            prefix: new HarmonyMethod(typeof(OracleRun), nameof(HeadlessNGameInstance)));
        headless.Patch(
            AccessTools.Method(typeof(NGame), nameof(NGame.ScreenShakeTrauma)),
            prefix: new HarmonyMethod(typeof(OracleRun), nameof(SkipScreenShakeTrauma)));
        headless.Patch(
            AccessTools.Method(typeof(NGame), nameof(NGame.ScreenRumble)),
            prefix: new HarmonyMethod(typeof(OracleRun), nameof(SkipScreenRumble)));
        headless.Patch(
            AccessTools.Method(typeof(NGame), nameof(NGame.ScreenShake), new[] { typeof(ShakeStrength), typeof(ShakeDuration), typeof(float) }),
            prefix: new HarmonyMethod(typeof(OracleRun), nameof(SkipScreenShake)));
        headless.Patch(
            AccessTools.Method(typeof(NGame), nameof(NGame.DoHitStop)),
            prefix: new HarmonyMethod(typeof(OracleRun), nameof(SkipHitStop)));
        headless.Patch(
            AccessTools.Method(typeof(SoulNexus), "AfterDeath", new[] { typeof(Creature) }),
            prefix: new HarmonyMethod(typeof(OracleRun), nameof(SkipSoulNexusDeathAnimation)));
        Harmony trial = new("sts2oracle.run.trial-presentation");
        trial.Patch(AccessTools.Method(typeof(Trial), "Accept"), transpiler: new HarmonyMethod(typeof(OracleRun), nameof(SkipTrialPortraitContext)));
        trial.Patch(AccessTools.Method(typeof(Trial), "AddVfxAnchoredToPortrait"), transpiler: new HarmonyMethod(typeof(OracleRun), nameof(SkipTrialPortraitContext)));

        Harmony amalgamator = new("sts2oracle.run.amalgamator-presentation");
        foreach (string methodName in new[] { "CombineStrikes", "CombineDefends" })
        {
            MethodInfo method = AccessTools.Method(typeof(Amalgamator), methodName, Type.EmptyTypes)
                ?? throw new MissingMethodException(typeof(Amalgamator).FullName, methodName);
            Type stateMachine = method.GetCustomAttribute<AsyncStateMachineAttribute>()?.StateMachineType
                ?? throw new InvalidOperationException($"{typeof(Amalgamator).FullName}.{methodName} is no longer async");
            MethodInfo moveNext = AccessTools.Method(stateMachine, "MoveNext")
                ?? throw new MissingMethodException(stateMachine.FullName, "MoveNext");
            amalgamator.Patch(moveNext, transpiler: new HarmonyMethod(typeof(OracleRun), nameof(SkipAmalgamatorScreenShake)));
        }
    }

    private static bool HeadlessNGameInstance(ref NGame? __result)
    {
        if (__result != null) return true;
        string? caller = new StackTrace(false).GetFrame(1)?.GetMethod()?.DeclaringType?.FullName;
        if (caller == null || !(caller.Contains("Models.Events.DenseVegetation", StringComparison.Ordinal)
            || caller.Contains("Models.Events.PunchOff", StringComparison.Ordinal)
            || caller.Contains("Models.Events.TheArchitect", StringComparison.Ordinal))) return true;
        _headlessGame ??= (NGame)RuntimeHelpers.GetUninitializedObject(typeof(NGame));
        __result = _headlessGame;
        return false;
    }

    private static bool SkipScreenShakeTrauma(NGame __instance, ShakeStrength strength) => false;
    private static bool SkipScreenRumble(NGame __instance, ShakeStrength strength, ShakeDuration duration, RumbleStyle style) => false;
    private static bool SkipHitStop(NGame __instance, ShakeStrength strength, ShakeDuration duration) => false;

    private static bool SkipSoulNexusDeathAnimation(SoulNexus __instance, Creature __0)
    {
        MethodInfo callbackMethod = AccessTools.Method(typeof(SoulNexus), "AfterDeath", new[] { typeof(Creature) })!;
        Delegate callback = Delegate.CreateDelegate(typeof(Action<Creature>), __instance, callbackMethod);
        typeof(Creature).GetEvent(nameof(Creature.Died))?.RemoveEventHandler(__0, callback);
        return false;
    }

    private static bool SkipScreenShake(NGame __instance, ShakeStrength strength, ShakeDuration duration, float degAngle = -1f)
    {
        // NGame.ScreenShake consumes this presentation-only Chaotic draw before applying the VFX.
        if (degAngle < 0f) Rng.Chaotic.NextFloat(360f);
        return false;
    }

    private static bool NotLocalTrialPortrait(Player? _) => false;

    private static void DiscardAmalgamatorScreenShake(NGame? game, ShakeStrength strength) { }

    private static IEnumerable<CodeInstruction> SkipAmalgamatorScreenShake(IEnumerable<CodeInstruction> instructions)
    {
        MethodInfo original = AccessTools.Method(typeof(NGame), nameof(NGame.ScreenShakeTrauma), new[] { typeof(ShakeStrength) })
            ?? throw new MissingMethodException(typeof(NGame).FullName, nameof(NGame.ScreenShakeTrauma));
        MethodInfo replacement = AccessTools.Method(typeof(OracleRun), nameof(DiscardAmalgamatorScreenShake))!;
        int replaced = 0;
        foreach (CodeInstruction instruction in instructions)
        {
            if (instruction.Calls(original))
            {
                var rewritten = new CodeInstruction(instruction)
                {
                    opcode = OpCodes.Call,
                    operand = replacement
                };
                replaced++;
                yield return rewritten;
            }
            else
            {
                yield return instruction;
            }
        }
        if (replaced != 2)
            throw new InvalidOperationException($"Expected exactly 2 {nameof(NGame.ScreenShakeTrauma)} calls in an {typeof(Amalgamator).FullName} merge method, found {replaced}");
    }

    private static IEnumerable<CodeInstruction> SkipTrialPortraitContext(IEnumerable<CodeInstruction> instructions)
    {
        MethodInfo original = AccessTools.Method(typeof(LocalContext), nameof(LocalContext.IsMe), new[] { typeof(Player) })!;
        MethodInfo replacement = AccessTools.Method(typeof(OracleRun), nameof(NotLocalTrialPortrait))!;
        foreach (CodeInstruction instruction in instructions)
        {
            if (instruction.Calls(original)) yield return new CodeInstruction(OpCodes.Call, replacement);
            else yield return instruction;
        }
    }

    private static bool PlayCrystalSphereHeadless(CrystalSphereMinigame __instance, ref Task __result)
    {
        if (_current is not { } current) return true;
        __result = ResolveCrystalSphereHeadless(__instance, current.controller);
        return false;
    }

    private static async Task ResolveCrystalSphereHeadless(CrystalSphereMinigame game, Controller controller)
    {
        const int width = 11;
        while (!game.IsFinished)
        {
            int choice = controller.ChooseCrystalSphereOption();
            if (choice == width * width)
                game.SetTool(CrystalSphereMinigame.CrystalSphereToolType.Small);
            else if (choice == width * width + 1)
                game.SetTool(CrystalSphereMinigame.CrystalSphereToolType.Big);
            else
                await game.CellClicked(game.cells[choice % width, choice / width]);
        }
        MethodInfo complete = AccessTools.Method(typeof(CrystalSphereMinigame), "CompleteMinigame")
            ?? throw new MissingMethodException(typeof(CrystalSphereMinigame).FullName, "CompleteMinigame");
        await (Task)complete.Invoke(game, null)!;
    }

    private static void RunEndedPostfix(bool isVictory)
    {
        if (isVictory && _current is { } cur)
            cur.controller.MarkVictoryRecorded();
    }

    private static bool ChooseBundle(Player player, IReadOnlyList<IReadOnlyList<CardModel>> bundles, ref Task<IEnumerable<CardModel>> __result)
    {
        if (_current is not { } cur) return true;
        if (CombatManager.Instance.IsEnding || bundles.Count == 0)
        {
            __result = Task.FromResult<IEnumerable<CardModel>>(Array.Empty<CardModel>());
            return false;
        }
        EmitDecision("bundle_select", new JsonObject { ["options"] = new JsonObject { ["bundles"] = new JsonArray(bundles.Select(b => (JsonNode)Cards(b)).ToArray()) }, ["run"] = DumpRun(cur.state, cur.player) }, cur.controller);
        int index = cur.controller.Script == null ? cur.policy.Next(bundles.Count) : (int)cur.controller.Script.Take("bundle_select")["index"]!;
        if (index < 0 || index >= bundles.Count) throw cur.controller.Script!.Refuse("no such bundle");
        Emit(new JsonObject { ["event"] = "action", ["kind"] = "bundle_select", ["index"] = index });
        __result = Task.FromResult<IEnumerable<CardModel>>(bundles[index]);
        return false;
    }

    public static void Run(Dictionary<string, string> opts, SingleThreadContext sync)
    {
        // Crusher/Rocket.BeforeDeath only drive audio and background animations;
        // both dereference an absent audio singleton in headless full runs.
        // Keep CreatureCmd's death, combat, and run hooks in the original code.
        OracleCombat.InstallKaiserHeadlessPatch();
        PatchChoiceScreens();
        string[] seeds = Split(opts.GetValueOrDefault("seeds") ?? opts.GetValueOrDefault("seed", ""));
        string[] characters = Split(opts.GetValueOrDefault("characters") ?? opts.GetValueOrDefault("character", "IRONCLAD"));
        string[] policies = Split(opts.GetValueOrDefault("policy-seeds") ?? opts.GetValueOrDefault("policy-seed", "1"));
        string policyVersion = opts.GetValueOrDefault("policy-version", "coverage-v3");
        if (policyVersion is not ("legacy-v1" or "coverage-v2" or "coverage-v3"))
            throw new ArgumentException("unsupported --policy-version value: " + policyVersion);
        string[] relicSets = opts.TryGetValue("relic-matrix", out string? matrix)
            ? Split(matrix).Select(id => id).ToArray()
            : new[] { opts.GetValueOrDefault("relics", "") };
        if (relicSets.Length == 0) relicSets = new[] { "" };
        if (seeds.Length == 0) throw new ArgumentException("run requires --seed or --seeds");
        foreach (string seed in seeds)
        foreach (string character in characters)
        foreach (string policySeed in policies)
        foreach (string relicSet in relicSets)
        {
            var one = new Dictionary<string, string>(opts)
            {
                ["seed"] = seed,
                ["character"] = character,
                ["policy-seed"] = policySeed,
                ["relics"] = relicSet
            };
            int asc = int.Parse(opts.GetValueOrDefault("ascension", "0"), CultureInfo.InvariantCulture);
            JsonObject begin = new()
            {
                ["event"] = "begin",
                ["seed"] = seed,
                ["character"] = character.ToUpperInvariant(),
                ["ascension"] = asc,
                ["policy_seed"] = int.Parse(policySeed, CultureInfo.InvariantCulture),
                ["policy_version"] = policyVersion,
                ["oracle_test_mode"] = MegaCrit.Sts2.Core.TestSupport.TestMode.IsOn,
                ["neow"] = !opts.ContainsKey("no-neow"),
                ["hp"] = opts.TryGetValue("hp", out string? hpOpt) ? int.Parse(hpOpt, CultureInfo.InvariantCulture) : null,
                ["deck"] = opts.GetValueOrDefault("deck"),
                ["relics"] = relicSet
            };
            if (opts.TryGetValue("force-sacrifice", out string? forcedSacrifices))
                begin["force_sacrifice"] = int.Parse(forcedSacrifices, CultureInfo.InvariantCulture);
            if (opts.TryGetValue("stop", out string? requestedStop))
                begin["stop"] = NormalizeStop(requestedStop);
            Emit(begin);
            try
            {
                OracleRngLedger.BeginRun();
                RunOne(one, sync);
            }
            catch (Exception ex) when (IsScriptEnd(ex))
            {
                Emit(new JsonObject { ["event"] = "end", ["reason"] = "max_decisions", ["run"] = RunManager.Instance.DebugOnlyGetState() is RunState s ? DumpRun(s, s.Players[0]) : null });
            }
            catch (Exception ex)
            {
                Emit(new JsonObject { ["event"] = "end", ["reason"] = "error", ["message"] = ex.Message, ["run"] = RunManager.Instance.DebugOnlyGetState() is RunState s ? DumpRun(s, s.Players[0]) : null });
                Console.Error.WriteLine(ex);
            }
            finally
            {
                _current = null;
                RunManager.Instance.CleanUp();
                RewardsSet.testSelector = null;
                CardSelectCmd.Reset();
            }
        }
    }

    private static void RunOne(Dictionary<string, string> opts, SingleThreadContext sync)
    {
        Exception? fault = null;
        Action<Exception> captureFault = ex => fault ??= ex;
        TaskHelper.UnobservedFault += captureFault;
        try
        {
            RunOneCore(opts, sync, () => fault);
            if (fault != null) throw new InvalidOperationException("Run task failed asynchronously", fault);
        }
        finally { TaskHelper.UnobservedFault -= captureFault; }
    }

    private static void RunOneCore(Dictionary<string, string> opts, SingleThreadContext sync, Func<Exception?> backgroundFault)
    {
        RunState state = Oracle.StartRun(opts);
        // --no-neow: a profile without the Neow epoch (ExtraRunFields.StartedWithNeow);
        // act 1 starts on the map with a monster at its first point.
        if (opts.ContainsKey("no-neow")) state.ExtraFields.StartedWithNeow = false;
        Player player = state.Players[0];
        // --hp N: a test aid (the simulator's replay does the same) so that the
        // seeded random policy lives long enough to reach the later rooms.
        if (opts.TryGetValue("hp", out string? hp))
        {
            player.Creature.SetMaxHpInternal(int.Parse(hp, CultureInfo.InvariantCulture));
            player.Creature.SetCurrentHpInternal(int.Parse(hp, CultureInfo.InvariantCulture));
        }
        Random policy = new Random(int.Parse(opts.GetValueOrDefault("policy-seed", "1"), CultureInfo.InvariantCulture));
        int max = int.Parse(opts.GetValueOrDefault("max-decisions", "3000"), CultureInfo.InvariantCulture);
        // A scripted run stops where its script does: one action per decision.
        ActionScript? script = opts.TryGetValue("script", out string? scriptPath) ? new ActionScript(scriptPath) : null;
        if (script != null)
        {
            if (ScriptDecisionLimit is <= 0) throw new ArgumentException("nonpositive replacement script decision limit");
            max = ScriptDecisionLimit ?? script.Count;
        }
        int forced = int.Parse(opts.GetValueOrDefault("force-sacrifice", "0"), CultureInfo.InvariantCulture);
        if (forced < 0) throw new ArgumentException("negative force-sacrifice count");
        Controller controller = new Controller(state, policy, sync, max, forced,
            opts.GetValueOrDefault("policy-version", "coverage-v3") == "legacy-v1",
            opts.GetValueOrDefault("policy-version", "coverage-v3") == "coverage-v3");
        controller.Script = script;
        _current = (controller, policy, state, player);
        using IDisposable selector = CardSelectCmd.UseSelector(controller);
        RewardsSet.testSelector = set => SelectRewards(set, controller, policy);
        // Starting relics can call card selection and reward APIs in AfterObtained.
        // They run before the map exists, so give those callbacks a temporary
        // history entry for the game's skip/reward bookkeeping and remove it
        // immediately after the loadout has been applied.
        RunManager.Instance.TreasureRoomRelicSynchronizer.RelicsAwarded += results =>
        {
            results.Sort((r1, r2) => r1.type.CompareTo(r2.type));
            foreach (RelicPickingResult result in results)
            {
                if (result.type != RelicPickingResultType.Skipped)
                    TaskHelper.RunSafely(RelicCmd.Obtain(result.relic.ToMutable(), result.player));
                foreach (Player other in state.Players)
                    if (other != result.player) other.RelicGrabBag.MoveToFallback(result.relic);
            }
        };
        if (opts.TryGetValue("deck", out string? deck))
        {
            foreach (CardModel card in player.Deck.Cards.ToList()) CardPileCmd.RemoveFromDeck(card);
            foreach (string spec in Split(deck))
            {
                string[] parts = spec.Split('+', 2);
                int upgrades = parts.Length == 2 ? int.Parse(parts[1], CultureInfo.InvariantCulture) : 0;
                if (upgrades < 0) throw new ArgumentException("negative card upgrade: " + spec);
                CardModel card = state.CreateCard(ModelDb.GetById<CardModel>(new ModelId("CARD", parts[0])), player);
                Await(CardPileCmd.Add(card, PileType.Deck), sync);
                for (int i = 0; i < upgrades; i++) CardCmd.Upgrade(card);
            }
        }
        if (opts.TryGetValue("relics", out string? relics))
        {
            int originalHistoryCount = state.MapPointHistory.Count;
            state.AppendToMapPointHistory(MapPointType.Unassigned, RoomType.Map, null);
            System.Collections.IList temporaryActHistory = (System.Collections.IList)state.MapPointHistory[state.CurrentActIndex];
            int temporaryEntryIndex = temporaryActHistory.Count - 1;
            object temporaryEntry = temporaryActHistory[temporaryEntryIndex]!;
            try
            {
                foreach (string id in Split(relics))
                {
                    RelicModel relic = ModelDb.GetById<RelicModel>(new ModelId("RELIC", id)).ToMutable();
                    // Darv configures Dusty Tome's saved ancient card before
                    // offering it. Reproduce that setup when the test loadout
                    // injects the relic directly.
                    if (relic is DustyTome dustyTome) dustyTome.SetupForPlayer(player);
                    if (id == "MASSIVE_SCROLL")
                    {
                        // This headless test runner controls one player. Add a
                        // temporary second reference only while exercising the
                        // multiplayer-only relic's card pool and pickup effect.
                        FieldInfo playersField = typeof(RunState).GetField("_players", BindingFlags.Instance | BindingFlags.NonPublic)
                            ?? throw new MissingFieldException(typeof(RunState).FullName, "_players");
                        IList<Player> players = (IList<Player>)playersField.GetValue(state)!;
                        players.Add(player);
                        try { Await(RelicCmd.Obtain(relic, player), sync); }
                        finally { players.RemoveAt(players.Count - 1); }
                    }
                    else Await(RelicCmd.Obtain(relic, player), sync);
                }
            }
            finally
            {
                System.Collections.IList actHistory = (System.Collections.IList)state.MapPointHistory[state.CurrentActIndex];
                if (temporaryEntryIndex >= actHistory.Count || !ReferenceEquals(actHistory[temporaryEntryIndex], temporaryEntry))
                    throw new InvalidOperationException("starting relic selection changed temporary map history");
                actHistory.RemoveAt(temporaryEntryIndex);
                // AppendToMapPointHistory may create an otherwise-empty act list.
                // Empty lists have no game-visible history entries.
                if (state.MapPointHistory.Count > originalHistoryCount && actHistory.Count == 0)
                    actHistory.Clear();
            }
        }
        if (opts.TryGetValue("stop", out string? setupStop) && NormalizeStop(setupStop) == "after-loadout")
        {
            Emit(new JsonObject
            {
                ["event"] = "end",
                ["reason"] = "boundary",
                ["stop"] = "after-loadout",
                ["run"] = DumpRun(state, player),
                ["boundary_state"] = DumpBoundaryState(state, player, includeMap: false)
            });
            return;
        }
        Await(RunManager.Instance.EnterAct(0), sync);
        bool combatRewardsOffered = false;
        CombatRoom? lastCombatRoom = null;
        bool treasureHandled = false;
        bool restHandled = false;
        bool shopHandled = false;
        for (;;)
        {
            sync.Pump();
            if (backgroundFault() is Exception fault)
                throw new InvalidOperationException("Run task failed asynchronously", fault);
            if (controller.VictoryRecorded && state.CurrentRoom?.IsVictoryRoom == true)
            {
                JsonObject end = new() { ["event"] = "end", ["reason"] = "victory", ["run"] = DumpRun(state, player) };
                if (opts.TryGetValue("stop", out string? requestedStop) && NormalizeStop(requestedStop) == "victory")
                    end["stop"] = "victory";
                Emit(end);
                return;
            }
            if (player.Creature.IsDead)
            {
                Emit(new JsonObject { ["event"] = "end", ["reason"] = "death", ["run"] = DumpRun(state, player) });
                return;
            }
            if (controller.Decisions >= max)
            {
                Emit(new JsonObject { ["event"] = "end", ["reason"] = "max_decisions", ["run"] = DumpRun(state, player) });
                return;
            }
            CombatState? combat = CombatManager.Instance.DebugOnlyGetState();
            // A first-turn hook can end a newly entered combat before this
            // driver ever observes IsInProgress. Reward ownership belongs to
            // the room, not to whether PlayCombat happened (Fur Coat + Popper).
            if (state.CurrentRoom is CombatRoom currentCombatRoom && !ReferenceEquals(lastCombatRoom, currentCombatRoom))
            {
                lastCombatRoom = currentCombatRoom;
                combatRewardsOffered = false;
            }
            if (combat != null && CombatManager.Instance.IsInProgress)
            {
                combatRewardsOffered = false;
                PlayCombat(state, player, combat, policy, sync, controller);
                continue;
            }
            if (state.CurrentRoom is CombatRoom combatRoom && !combatRewardsOffered && !CombatManager.Instance.IsEnding)
            {
                combatRewardsOffered = true;
                // NCombatUi.OnCombatWon bypasses room rewards for encounters
                // whose ShouldGiveRewards is false. The old headless loop
                // offered them anyway, inventing a normal card reward for
                // Battleworn Dummy before the parent event could resume.
                if (combatRoom.Encounter.ShouldGiveRewards)
                    Await(combatRoom.OfferRoomEndRewards(), sync);
                else
                    Await(RunManager.Instance.ProceedFromTerminalRewardsScreen(), sync);
                if (controller.ConsumeRewardProceedSelected() && state.CurrentRoom?.RoomType == RoomType.Boss)
                {
                    if (state.Map.SecondBossMapPoint != null && state.CurrentMapCoord == state.Map.BossMapPoint.coord)
                    {
                        // NRewardsScreen.OnProceedButtonPressed uses this path after the first
                        // boss on a DoubleBoss map; it returns to the same act's map.
                        Await(RunManager.Instance.ProceedFromTerminalRewardsScreen(), sync);
                    }
                    else
                    {
                        // Match the terminal boss Proceed path: queue the local ready vote so
                        // VoteToMoveToNextActAction and ActChangeSynchronizer invoke EnterNextAct.
                        int currentAct = state.CurrentActIndex;
                        RunManager.Instance.ActChangeSynchronizer.SetLocalPlayerReady();
                        Await(RunManager.Instance.ActionExecutor.FinishedExecutingActions(), sync);
                        PumpUntil(sync, () => currentAct == state.Acts.Count - 1
                            ? state.CurrentRoom?.IsVictoryRoom == true
                            : state.CurrentActIndex == currentAct + 1 && state.CurrentRoom is MapRoom && !state.CurrentMapCoord.HasValue && state.VisitedMapCoords.Count == 0,
                            "terminal boss Proceed did not complete the original act transition");
                    }
                }
                continue;
            }
            if (state.CurrentRoom is EventRoom eventRoom)
            {
                EventModel ev = eventRoom.LocalMutableEvent;
                if (!ev.IsFinished && ev.CurrentOptions.Count > 0)
                {
                    ChooseEvent(state, player, ev, policy, controller);
                    continue;
                }
                if (ev is FakeMerchant fakeMerchant && !fakeMerchant.StartedFight && !shopHandled)
                {
                    MerchantInventory inventory = fakeMerchant.Inventory
                        ?? throw new InvalidOperationException("FakeMerchant inventory was not initialized before entering its event room");
                    ChooseShop(state, player, inventory, policy, controller);
                    // NFakeMerchant's Proceed button only opens the map. Marking
                    // the shop handled lets the next loop iteration take that map choice.
                    shopHandled = true;
                    continue;
                }
                if (eventRoom.IsVictoryRoom && !ev.IsFinished && ev.CurrentOptions.Count == 0)
                    throw new NotSupportedException("The Architect event has no selectable option in this headless run; its dialogue still depends on the unavailable visual speaker node.");
            }
            if (state.CurrentRoom is RestSiteRoom rest)
            {
                // MiniatureTent can leave options after a successful choice.
                // Only a script requesting another rest action stays here;
                // the ordinary one-option policy and old map scripts keep
                // their previous room-exit behaviour. Validate the original
                // synchronizer's current (reindexed) options every time.
                if (!restHandled || controller.Script?.NextIs("rest") == true)
                {
                    IReadOnlyList<RestSiteOption> options = rest.Options;
                    List<int> legal = options.Select((o, i) => (o, i)).Where(x => x.o.IsEnabled).Select(x => x.i).ToList();
                    EmitDecision("rest", new JsonObject { ["options"] = new JsonArray(options.Select(o => (JsonNode)new JsonObject { ["id"] = o.OptionId, ["enabled"] = o.IsEnabled }).ToArray()), ["run"] = DumpRun(state, player) }, controller);
                    if (TryScriptedPotion(controller, player, "rest")) continue;
                    int index;
                    if (controller.Script != null)
                    {
                        index = (int)controller.Script.Take("rest")["index"]!;
                        if (!legal.Contains(index)) throw controller.Script.Refuse("the rest site option is not enabled");
                    }
                    else index = legal.Count == 0 ? -1 : legal[policy.Next(legal.Count)];
                    // The action goes first: the option may ask for a card selection.
                    Emit(new JsonObject { ["event"] = "action", ["kind"] = "rest", ["index"] = index });
                    if (index >= 0) Await(RunManager.Instance.RestSiteSynchronizer.ChooseLocalOption(index), sync);
                    restHandled = true;
                    continue;
                }
            }
            if (state.CurrentRoom is TreasureRoom treasure && !treasureHandled)
            {
                ChooseTreasure(state, player, policy, controller, treasure);
                treasureHandled = true;
                continue;
            }
            if (state.CurrentRoom is MerchantRoom merchant && !shopHandled)
            {
                ChooseShop(state, player, merchant.GetLocalInventory(), policy, controller);
                shopHandled = true;
                continue;
            }
            // A map room (or a completed room after the player's action) is the
            // next travel decision. Stop only at the requested, observed boundary.
            MapPoint? point = state.CurrentMapPoint;
            // No point visited yet (a run without Neow): the map's starting point.
            // Mirror NMapScreen.RecalculateTravelability's boundary overrides.
            // Boss nodes live outside ActMap.Grid: MapTravel's free-travel branch
            // alone returns an empty row with active Winged Boots at the last
            // ordinary row. The real UI explicitly offers the boss there, and
            // offers the second boss first when DoubleBoss has one remaining.
            List<MapPoint> children = point == null
                ? new List<MapPoint> { state.Map.StartingMapPoint }
                : state.Map.SecondBossMapPoint != null && point.coord == state.Map.BossMapPoint.coord
                    ? new List<MapPoint> { state.Map.SecondBossMapPoint }
                    : point.coord.row == state.Map.GetRowCount() - 1
                        ? new List<MapPoint> { state.Map.BossMapPoint }
                        : MapTravel.GetTravelablePointsFrom(state, point).OrderBy(p => p.coord.col).ThenBy(p => p.coord.row).ToList();
            if (TryGetBoundaryStop(opts, state, children, out string? boundaryStop))
            {
                JsonObject end = new() { ["event"] = "end", ["reason"] = opts.ContainsKey("stop") ? "boundary" : "boss", ["run"] = DumpRun(state, player) };
                if (opts.ContainsKey("stop"))
                {
                    end["stop"] = boundaryStop;
                    end["boundary_state"] = DumpBoundaryState(state, player);
                }
                Emit(end);
                return;
            }
            EmitDecision("map", new JsonObject { ["options"] = new JsonArray(children.Select(p => (JsonNode)new JsonArray(p.coord.col, p.coord.row)).ToArray()) , ["run"] = DumpRun(state, player) }, controller);
            if (TryScriptedPotion(controller, player, "map")) continue;
            MapPoint next;
            if (controller.Script != null)
            {
                JsonArray coord = controller.Script.Take("map")["coord"]!.AsArray();
                next = children.FirstOrDefault(p => p.coord.col == (int)coord[0]! && p.coord.row == (int)coord[1]!)
                    ?? throw controller.Script.Refuse("the map point cannot be travelled to");
            }
            else next = children[policy.Next(children.Count)];
            Emit(new JsonObject { ["event"] = "action", ["kind"] = "map", ["coord"] = new JsonArray(next.coord.col, next.coord.row) });
            restHandled = treasureHandled = shopHandled = false;
            Await(RunManager.Instance.EnterMapCoord(next.coord), sync);
        }
    }

    private static string NormalizeStop(string stop)
    {
        string normalized = stop.Trim().ToLowerInvariant();
        if (normalized.Length == 0) return "before-act1-boss";
        return normalized switch
        {
            "before-act1-boss" or "after-act1" or "after-act2" or "after-loadout" or "victory" => normalized,
            _ => throw new ArgumentException("unsupported --stop value: " + stop)
        };
    }

    private static bool TryGetBoundaryStop(Dictionary<string, string> opts, RunState state, IReadOnlyList<MapPoint> children, out string? stop)
    {
        if (!opts.TryGetValue("stop", out string? requestedStop))
        {
            // Preserve the historical default trace boundary and reason.
            stop = null;
            return state.CurrentActIndex == 0 && children.Contains(state.Map.BossMapPoint);
        }

        stop = NormalizeStop(requestedStop);
        return stop switch
        {
            "before-act1-boss" => state.CurrentActIndex == 0 && children.Contains(state.Map.BossMapPoint),
            "after-act1" => state.CurrentActIndex == 1 && state.CurrentRoom is MapRoom && !state.CurrentMapCoord.HasValue && state.VisitedMapCoords.Count == 0,
            "after-act2" => state.CurrentActIndex == 2 && state.CurrentRoom is MapRoom && !state.CurrentMapCoord.HasValue && state.VisitedMapCoords.Count == 0,
            _ => false
        };
    }

    private static void PumpUntil(SingleThreadContext sync, Func<bool> predicate, string error)
    {
        for (int i = 0; i < 100000 && !predicate(); i++) sync.Pump();
        if (!predicate()) throw new InvalidOperationException(error);
    }

    private static JsonObject DumpBoundaryState(RunState state, Player player, bool includeMap = true)
    {
        JsonObject runRng = new();
        foreach (RunRngType type in Enum.GetValues<RunRngType>())
            runRng[type.ToString()] = DumpRng(state.Rng.GetRng(type).ToSerializable());

        JsonObject playerRng = new();
        foreach (var entry in player.PlayerRng.ToSerializable().Rngs.OrderBy(x => x.Key))
        {
            string name = entry.Key.ToString();
            string camelName = char.ToLowerInvariant(name[0]) + name.Substring(1);
            playerRng[camelName] = DumpRng(entry.Value);
        }

        JsonObject boundary = new()
        {
            ["run_rng"] = runRng,
            ["player_rng"] = playerRng
        };
        if (includeMap) boundary["map"] = Oracle.DumpMap(state.Map);
        return boundary;
    }

    private static JsonObject DumpRng(SerializableRng rng) => new()
    {
        ["counter"] = rng.counter,
        ["state0"] = rng.state0,
        ["state1"] = rng.state1,
        ["state2"] = rng.state2,
        ["state3"] = rng.state3
    };

    private static async Task SelectRewards(RewardsSet set, Controller controller, Random policy)
    {
        while (true)
        {
            // RewardsSet.Offer marks only CombatRoom screens as terminal.
            // The native synchronizer completes an all-selected custom set,
            // and NRewardsScreen closes it immediately without a Proceed.
            // Keep the terminal combat-room Proceed and unfinished-set Skip.
            if (set.Room is not CombatRoom && set.AllRewardsSuccessfullySelected) return;
            List<int> available = set.Rewards.Select((r, i) => (r, i)).Where(x => !x.r.SuccessfullySelected).Select(x => x.i).ToList();
            var options = new JsonArray(set.Rewards.Select(RewardJson).ToArray());
            EmitDecision("rewards", new JsonObject { ["options"] = options, ["run"] = DumpRun((RunState)set.Player.RunState, set.Player) }, controller);
            PotionModel? scriptedPotion = BeginScriptedPotion(controller, set.Player, "rewards");
            if (scriptedPotion != null)
            {
                Stopwatch waited = Stopwatch.StartNew();
                while (set.Player.PotionSlots.Contains(scriptedPotion) && waited.ElapsedMilliseconds < 10000) await Task.Yield();
                if (set.Player.PotionSlots.Contains(scriptedPotion)) throw controller.Script!.Refuse("the potion action never ran");
                continue;
            }
            int choice;
            if (controller.Script != null)
            {
                JsonObject scripted = controller.Script.Take("rewards");
                choice = scripted["proceed"] != null ? available.Count : available.IndexOf((int)scripted["index"]!);
                if (choice < 0) throw controller.Script.Refuse("the reward cannot be taken");
            }
            else choice = policy.Next(available.Count + 1); // last action is "proceed"
            if (choice == available.Count)
            {
                Emit(new JsonObject { ["event"] = "action", ["kind"] = "rewards", ["proceed"] = true });
                // Only a terminal combat-room screen can authorize the run
                // loop's boss act transition. A custom rest/event reward Skip
                // (e.g. TinyMailbox on floor 16) must not leave a stale vote
                // that survives a later ScriptEnded at the boss reward screen.
                if (set.Room is CombatRoom) controller.MarkRewardProceedSelected();
                if (available.Count > 0) RunManager.Instance.RewardsSetSynchronizer.SkipLocalRewardsSet();
                return;
            }
            int index = available[choice];
            Emit(new JsonObject { ["event"] = "action", ["kind"] = "rewards", ["index"] = index });
            await controller.SelectReward(set.Rewards[index]);
        }
    }

    private static void PlayCombat(RunState state, Player player, CombatState combat, Random policy, SingleThreadContext sync, Controller controller)
    {
        if (player.PlayerCombatState?.Phase != PlayerTurnPhase.Play || combat.CurrentSide != CombatSide.Player || RunManager.Instance.ActionExecutor.IsRunning) return;
        List<(int, CardModel, MegaCrit.Sts2.Core.Entities.Creatures.Creature?)> plays = new();
        for (int i = 0; i < player.PlayerCombatState.Hand.Cards.Count; i++)
        {
            CardModel card = player.PlayerCombatState.Hand.Cards[i];
            if ((controller.LegacyPolicy && (card.Id.Entry is "NEUTRALIZE" or "SURVIVOR")) || !card.CanPlay()) continue;
            if (card.TargetType == TargetType.AnyEnemy || card.TargetType == TargetType.AnyAlly)
            {
                foreach (var c in combat.Creatures) if (card.IsValidTarget(c)) plays.Add((i, card, c));
            }
            // Legacy's unbraced `else` attached to the inner target-validity
            // test, so it never enumerated untargeted cards. Preserve those RNG
            // trajectories explicitly; coverage-v2 includes every native play.
            else if (!controller.LegacyPolicy && card.IsValidTarget(null)) plays.Add((i, card, null));
        }
        List<(int slot, PotionModel potion, Creature? target)> uses = new();
        List<int> discards = new();
        if (!controller.LegacyPolicy && player.CanUseOrRemovePotions)
        {
            // NPotionPopup enables discard for every owned, unqueued potion,
            // including Automatic. It disables manual use for Automatic and
            // applies the potion's custom usability check. This driver already
            // runs only at a live player's Play-phase static decision boundary.
            for (int slot = 0; slot < player.PotionSlots.Count; ++slot)
            {
                PotionModel? potion = player.PotionSlots[slot];
                if (potion == null || potion.IsQueued) continue;
                discards.Add(slot);
                if (potion.Usage is not (PotionUsage.AnyTime or PotionUsage.CombatOnly)
                    || !potion.PassesCustomUsabilityCheck || CombatManager.Instance.PlayerActionsDisabled) continue;
                if (potion.TargetType is TargetType.AnyEnemy or TargetType.AnyAlly or TargetType.AnyPlayer or TargetType.Self)
                {
                    foreach (Creature target in combat.Creatures)
                        if (potion.IsValidTarget(target)) uses.Add((slot, potion, target));
                }
                else if (potion.IsValidTarget(null)) uses.Add((slot, potion, null));
            }
        }
        CombatObserver?.Invoke(combat, player, controller.Decisions);
        EmitDecision("combat", new JsonObject { ["combat"] = OracleCombat.Dump(state, combat, player) }, controller);
        int actionCount = plays.Count + uses.Count + discards.Count;
        int action = -1;  // -1: end the turn
        if (controller.Script != null)
        {
            JsonObject scripted = CombatActionProvider?.Invoke(combat, player) ?? controller.Script.Take("combat");
            int? target = (int?)scripted["target"];
            switch ((string?)scripted["type"])
            {
                case "play":
                    action = plays.FindIndex(x => x.Item1 == (int)scripted["hand_index"]! && x.Item3?.CombatId == (uint?)target);
                    break;
                case "use_potion":
                    action = uses.FindIndex(x => x.slot == (int)scripted["slot"]! && x.target?.CombatId == (uint?)target);
                    if (action >= 0) action += plays.Count;
                    break;
                case "discard_potion":
                    action = discards.IndexOf((int)scripted["slot"]!);
                    if (action >= 0) action += plays.Count + uses.Count;
                    break;
                case "end_turn":
                    action = -2;
                    break;
            }
            if (action == -1) throw controller.Script.Refuse("the combat action is not available");
        }
        else if (actionCount > 0 && policy.NextDouble() < .85) action = policy.Next(actionCount);
        if (action >= 0)
        {
            if (action < plays.Count)
            {
                var (i, card, target) = plays[action];
                Emit(new JsonObject { ["event"] = "action", ["kind"] = "combat", ["type"] = "play", ["hand_index"] = i, ["card"] = card.Id.Entry, ["target"] = target == null ? null : (int?)target.CombatId });
                RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(new PlayCardAction(card, target));
            }
            else if (action < plays.Count + uses.Count)
            {
                var (slot, potion, target) = uses[action - plays.Count];
                Emit(new JsonObject { ["event"] = "action", ["kind"] = "combat", ["type"] = "use_potion", ["slot"] = slot, ["potion"] = potion.Id.Entry, ["target"] = target == null ? null : (int?)target.CombatId });
                potion.EnqueueManualUse(target);
            }
            else
            {
                int slot = discards[action - plays.Count - uses.Count];
                Emit(new JsonObject { ["event"] = "action", ["kind"] = "combat", ["type"] = "discard_potion", ["slot"] = slot, ["potion"] = player.PotionSlots[slot]!.Id.Entry });
                RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(new DiscardPotionGameAction(player, (uint)slot, true));
            }
        }
        else
        {
            Emit(new JsonObject { ["event"] = "action", ["kind"] = "combat", ["type"] = "end_turn" });
            RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(new EndPlayerTurnAction(player, player.PlayerCombatState.TurnNumber));
        }
        sync.Pump();
    }

    private static void ChooseEvent(RunState state, Player player, EventModel ev, Random policy, Controller controller)
    {
        List<int> legal = ev.CurrentOptions.Select((o, i) => (o, i)).Where(x => !x.o.IsLocked && !(x.o.WillKillPlayer?.Invoke(player) ?? false)).Select(x => x.i).ToList();
        EmitDecision("event", new JsonObject { ["options"] = new JsonObject { ["event"] = ev.Id.Entry, ["options"] = new JsonArray(ev.CurrentOptions.Select(o => (JsonNode)new JsonObject { ["key"] = o.TextKey, ["locked"] = o.IsLocked }).ToArray()) }, ["run"] = DumpRun(state, player) }, controller);
        if (TryScriptedPotion(controller, player, "event")) return;
        int index;
        if (controller.Script != null)
        {
            // A script may take an option the policy avoids (one that kills the player).
            index = (int)controller.Script.Take("event")["index"]!;
            if (index < 0 || index >= ev.CurrentOptions.Count || ev.CurrentOptions[index].IsLocked)
                throw controller.Script.Refuse("the event option is locked or absent");
        }
        else index = legal.Count == 0 ? ev.CurrentOptions.Select((o, i) => (o, i)).FirstOrDefault(x => !x.o.IsLocked).i : legal[policy.Next(legal.Count)];
        Emit(new JsonObject { ["event"] = "action", ["kind"] = "event", ["index"] = index });
        Exception? unobservedFault = null;
        Action<Exception> captureFault = ex => unobservedFault ??= ex;
        TaskHelper.UnobservedFault += captureFault;
        try
        {
            RunManager.Instance.EventSynchronizer.ChooseLocalOption(index);
            AwaitPumping(RunManager.Instance.EventSynchronizer.AwaitPendingOptionTasks(), controller.Sync,
                $"event {ev.Id.Entry} option task");
        }
        finally
        {
            TaskHelper.UnobservedFault -= captureFault;
        }
        if (unobservedFault != null)
            throw new InvalidOperationException($"Event {ev.Id.Entry} option task failed asynchronously", unobservedFault);
    }

    private static void AwaitPumping(Task task, SingleThreadContext sync, string description)
    {
        TimeSpan timeout = TimeSpan.FromSeconds(60);
        Stopwatch elapsed = Stopwatch.StartNew();
        while (!task.IsCompleted)
        {
            sync.Pump();
            if (task.IsCompleted) break;
            if (elapsed.Elapsed >= timeout)
                throw new TimeoutException($"Timed out after {timeout.TotalSeconds:0}s waiting for {description}");
            Thread.Sleep(1);
        }
        sync.Pump();
        task.GetAwaiter().GetResult();
    }

    // NTreasureRoom.OpenChest: the gold and the extra rewards, then the relic
    // picking (the proceed button skips it).
    private static void ChooseTreasure(RunState state, Player player, Random policy, Controller controller, TreasureRoom room)
    {
        Await(room.DoNormalRewards(), controller.Sync);
        Await(room.DoExtraRewardsIfNeeded(), controller.Sync);
        FieldInfo field = typeof(TreasureRoomRelicSynchronizer).GetField("_currentRelics", BindingFlags.Instance | BindingFlags.NonPublic)!;
        var relics = ((IReadOnlyList<RelicModel>?)field.GetValue(RunManager.Instance.TreasureRoomRelicSynchronizer)) ?? Array.Empty<RelicModel>();
        if (relics.Count == 0) return;
        do EmitDecision("relic_select", new JsonObject { ["options"] = new JsonObject { ["relics"] = new JsonArray(relics.Select(r => (JsonNode)r.Id.Entry).ToArray()), ["can_skip"] = true }, ["run"] = DumpRun(state, player) }, controller);
        while (TryScriptedPotion(controller, player, "relic_select"));
        int index;
        if (controller.Script != null)
        {
            JsonObject scripted = controller.Script.Take("relic_select");
            index = scripted["skip"] != null ? -1 : (int)scripted["index"]!;
            if (index >= relics.Count) throw controller.Script.Refuse("no such treasure relic");
        }
        else index = policy.Next(relics.Count + 1) - 1;  // -1: skip
        Emit(index >= 0
            ? new JsonObject { ["event"] = "action", ["kind"] = "relic_select", ["index"] = index }
            : new JsonObject { ["event"] = "action", ["kind"] = "relic_select", ["skip"] = true });
        if (index >= 0) RunManager.Instance.TreasureRoomRelicSynchronizer.PickRelicLocally(index); else RunManager.Instance.TreasureRoomRelicSynchronizer.SkipRelicLocally();
    }

    // NMerchantRoom and NFakeMerchant both use NMerchantInventory: buy
    // affordable stocked entries until the policy leaves. The card removal
    // node's state changes (FillSlot / OnCardRemovalUsed) are mirrored here.
    private static void ChooseShop(RunState state, Player player, MerchantInventory inv, Random policy, Controller controller)
    {
        AbstractRoom? roomAtStart = state.CurrentRoom;
        // Relics such as Lord's Parasol can buy the card-removal entry during
        // AfterRoomEntered, before this headless shop driver runs. In the real
        // UI, NMerchantInventory.OnCardRemovalUsed marks the entry used; that
        // node is absent in the oracle, so mirror its state update here when
        // the removal count advanced after this entry was priced.
        if (inv.CardRemovalEntry != null && inv.CardRemovalEntry.Cost != new MerchantCardRemovalEntry(player).Cost)
            inv.CardRemovalEntry.SetUsed();
        if (inv.CardRemovalEntry != null && !Hook.ShouldAllowMerchantCardRemoval(state, player)) inv.CardRemovalEntry.SetUsed();
        for (;;)
        {
            List<MerchantEntry> stocked = inv.AllEntries.Where(e => e.IsStocked).ToList();
            var entries = stocked.Select(e => new JsonObject { ["kind"] = e switch { MerchantCardEntry => "card", MerchantRelicEntry => "relic", MerchantPotionEntry => "potion", _ => "remove" }, ["id"] = e switch { MerchantCardEntry c => c.CreationResult?.Card.Id.Entry, MerchantRelicEntry r => r.Model?.Id.Entry, MerchantPotionEntry p => p.Model?.Id.Entry, _ => "remove" }, ["cost"] = e.Cost, ["affordable"] = e.EnoughGold }).ToArray();
            EmitDecision("shop", new JsonObject { ["options"] = new JsonArray(entries), ["run"] = DumpRun(state, player) }, controller);
            List<int> legal = stocked.Select((e, i) => (e, i)).Where(x => x.e.EnoughGold).Select(x => x.i).ToList();
            if (TryScriptedPotion(controller, player, "shop")) continue;
            int choice;
            if (controller.Script != null)
            {
                JsonObject scripted = controller.Script.Take("shop");
                choice = scripted["leave"] != null ? legal.Count : legal.IndexOf((int)scripted["index"]!);
                if (choice < 0) throw controller.Script.Refuse("the shop entry is not stocked or not affordable");
            }
            else choice = policy.Next(legal.Count + 1);  // the last one leaves
            if (choice == legal.Count || (controller.Script == null && controller.Decisions >= 3000))
            {
                Emit(new JsonObject { ["event"] = "action", ["kind"] = "shop", ["leave"] = true });
                return;
            }
            int index = legal[choice];
            Emit(new JsonObject { ["event"] = "action", ["kind"] = "shop", ["index"] = index });
            if (stocked[index] is MerchantCardRemovalEntry removal)
            {
                Task<bool> bought = removal.OnTryPurchaseWrapper(inv);
                Await(bought, controller.Sync);
                if (bought.Result) removal.SetUsed();
            }
            else
            {
                Await(stocked[index].OnTryPurchaseWrapper(inv), controller.Sync);
            }
            if (ShopNeedsRunLoop(state, roomAtStart)) return;
        }
    }

    private static bool ShopNeedsRunLoop(RunState state, AbstractRoom? roomAtStart)
    {
        if (!ReferenceEquals(state.CurrentRoom, roomAtStart)) return true;
        if (CombatManager.Instance.DebugOnlyGetState() != null && CombatManager.Instance.IsInProgress) return true;
        return state.CurrentRoom is EventRoom eventRoom
            && eventRoom.LocalMutableEvent is FakeMerchant fakeMerchant
            && fakeMerchant.StartedFight;
    }

    internal static JsonObject RewardJson(Reward reward) => reward switch
    {
        GoldReward gold => new JsonObject { ["type"] = "Gold", ["amount"] = gold.Amount },
        PotionReward potion => new JsonObject { ["type"] = "Potion", ["id"] = potion.Potion?.Id.Entry },
        CardReward card => new JsonObject
        {
            ["type"] = "Card",
            ["cards"] = Cards(card.Cards),
            ["can_skip"] = card.CanSkip,
            ["can_reroll"] = card.CanReroll,
            ["has_been_rerolled"] = (bool)AccessTools.Field(typeof(CardReward), "_hasBeenRerolled").GetValue(card)!
        },
        RelicReward relic => new JsonObject { ["type"] = "Relic", ["id"] = relic.Relic?.Id.Entry },
        SpecialCardReward special => new JsonObject { ["type"] = "SpecialCard", ["card"] =
            CardName((CardModel)HarmonyLib.AccessTools.Field(typeof(SpecialCardReward), "_card").GetValue(special)!) },
        CardRemovalReward => new JsonObject { ["type"] = "RemoveCard" },
        _ => new JsonObject { ["type"] = reward.GetType().Name }
    };

    private static string CardName(CardModel card) => card.Id.Entry + (card.CurrentUpgradeLevel > 0 ? "+" + card.CurrentUpgradeLevel : "");
    private static JsonArray Cards(IEnumerable<CardModel> cards) => new(cards.Select(c => (JsonNode)CardName(c)).ToArray());

    internal static JsonObject DumpRun(RunState state, Player player)
    {
        var coord = state.CurrentMapCoord;
        var run = new JsonObject { ["act"] = state.CurrentActIndex, ["total_floor"] = state.TotalFloor, ["act_floor"] = state.ActFloor, ["coord"] = coord.HasValue ? new JsonArray(coord.Value.col, coord.Value.row) : null, ["room"] = state.CurrentRoom?.RoomType.ToString(), ["hp"] = player.Creature.CurrentHp, ["max_hp"] = player.Creature.MaxHp, ["gold"] = player.Gold, ["deck"] = Cards(player.Deck.Cards), ["relics"] = new JsonArray(player.Relics.Select(r => (JsonNode)new JsonObject { ["id"] = r.Id.Entry }.Also(o => { if (r.ShowCounter) o["counter"] = r.DisplayAmount; })).ToArray()), ["potions"] = new JsonArray(player.PotionSlots.Select(p => (JsonNode?)p?.Id.Entry).ToArray()), ["rng"] = RngCounters(state), ["player_rng"] = new JsonObject(), ["odds"] = new JsonObject { ["card_rarity"] = player.PlayerOdds.CardRarity.CurrentValue, ["potion_reward"] = player.PlayerOdds.PotionReward.CurrentValue, ["unknown"] = new JsonObject { ["Monster"] = state.Odds.UnknownMapPoint.MonsterOdds, ["Elite"] = state.Odds.UnknownMapPoint.EliteOdds, ["Treasure"] = state.Odds.UnknownMapPoint.TreasureOdds, ["Shop"] = state.Odds.UnknownMapPoint.ShopOdds } } };
        JsonObject prng = (JsonObject)run["player_rng"]!;
        foreach (var kv in player.PlayerRng.ToSerializable().Rngs) prng[kv.Key.ToString()] = kv.Value.counter;
        return run;
    }

    private static JsonObject RngCounters(RunState state)
    {
        var o = new JsonObject();
        foreach (RunRngType t in Enum.GetValues<RunRngType>()) o[t.ToString()] = state.Rng.GetRng(t).ToSerializable().counter;
        return o;
    }

    private static void EmitDecision(string kind, JsonObject fields, Controller controller)
    {
        // Stop at an ordinary decision boundary, after original combat
        // cleanup/reward generation. Stopping in the run-loop gap before
        // OfferRoomEndRewards would observe an earlier RNG state than Env.
        if (StopRequested?.Invoke() == true) throw new ScriptEnded();
        if (controller.Script is { Exhausted: true }) throw new ScriptEnded();
        fields["event"] = "decision"; fields["kind"] = kind;
        // The same optional observer as `combat` (OracleCombat.StateObserver).
        if (kind == "combat" && OracleCombat.StateObserver is { } observer
            && RunManager.Instance.DebugOnlyGetState() is RunState state && CombatManager.Instance.DebugOnlyGetState() is { } combat
            && observer(state, combat, state.Players[0]) is { } observed)
            fields["live"] = observed;
        Emit(fields); controller.Decisions++;
    }
    private static string[] Split(string s) => s.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
    private static void Emit(JsonObject o) => Program.Out.WriteLine(o.ToJsonString());
    private static void Await(Task task, SingleThreadContext sync) { for (int i = 0; i < 100000 && !task.IsCompleted; i++) sync.Pump(); task.GetAwaiter().GetResult(); }
}

internal static class OracleRunJsonExtensions
{
    public static T Also<T>(this T value, Action<T> action) { action(value); return value; }
}
