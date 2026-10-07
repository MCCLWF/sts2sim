using System.Text.Json;
using System.Text.Json.Nodes;
using System.Reflection;
using Godot;
using HarmonyLib;
using MegaCrit.Sts2.Core.Assets;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Context;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Entities.Rngs;
using MegaCrit.Sts2.Core.Events;
using MegaCrit.Sts2.Core.GameActions;
using MegaCrit.Sts2.Core.GameActions.Multiplayer;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Map;
using MegaCrit.Sts2.Core.MonsterMoves.Intents;
using MegaCrit.Sts2.Core.Modding;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Nodes;
using MegaCrit.Sts2.Core.Nodes.Screens;
using MegaCrit.Sts2.Core.Rewards;
using MegaCrit.Sts2.Core.Multiplayer.Game;
using MegaCrit.Sts2.Core.Nodes.Screens.CardSelection;
using MegaCrit.Sts2.Core.Nodes.Cards.Holders;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Random;
using MegaCrit.Sts2.Core.Saves;
using MegaCrit.Sts2.Core.Unlocks;

namespace Sts2LiveDiff;

// Read-only observer. A failed comparison disables only this combat's Env
// session; it never changes or cancels a game action.
[ModInitializer(nameof(Initialize))]
public static class Entry
{
    private static readonly string TracePath = ResolveTracePath();
    private static readonly JsonSerializerOptions TraceJsonOptions = new() { IncludeFields = true };
    private static CombatState? _combat;
    private static NativeEnv? _env;
    private static NativeEnv? _runEnv;
    private static int _sequence;
    private static string? _blocked;
    private static ActionExecutor? _attachedExecutor;
    private static bool _initialRunProbed;
    private static EventModel? _observedEvent;
    private static readonly HashSet<EventOption> ObservedOptions = new();
    private static int _runSequence;
    private static bool _runActionPending;
    private static bool _combatContinuous;
    private static bool _rewardsObserved;
    private static RewardsSet? _observedRewardsSet;
    private static NRewardsScreen? _observedRewardsScreen;
    private static bool _nestedRewards;
    private static CardReward? _pendingRewardChoice;
    private static CardReward? _skippedCardReward;

    public static void Initialize()
    {
        RunManager.Instance.RunStarted += _ => AttachExecutor();
        RunManager.Instance.RoomEntered += OnRoomEntered;
        CombatManager.Instance.TurnStarted += OnTurnStarted;
        new Harmony("sts2sim.live-diff.rewards").Patch(
            AccessTools.Method(typeof(NRewardsScreen), nameof(NRewardsScreen.ShowScreen)),
            postfix: new HarmonyMethod(typeof(Entry), nameof(AfterRewardsScreenShown)));
        new Harmony("sts2sim.live-diff.reward-selection").Patch(
            AccessTools.Method(typeof(RewardsSetSynchronizer), nameof(RewardsSetSynchronizer.SelectLocalReward)),
            prefix: new HarmonyMethod(typeof(Entry), nameof(BeforeLocalRewardSelected)),
            postfix: new HarmonyMethod(typeof(Entry), nameof(AfterLocalRewardSelected)));
        new Harmony("sts2sim.live-diff.reward-proceed").Patch(
            AccessTools.Method(typeof(RunManager), nameof(RunManager.ProceedFromTerminalRewardsScreen)),
            prefix: new HarmonyMethod(typeof(Entry), nameof(BeforeTerminalRewardsProceed)),
            postfix: new HarmonyMethod(typeof(Entry), nameof(AfterTerminalRewardsProceed)));
        new Harmony("sts2sim.live-diff.player-choice").Patch(
            AccessTools.Method(typeof(PlayerChoiceSynchronizer), nameof(PlayerChoiceSynchronizer.SyncLocalChoice)),
            prefix: new HarmonyMethod(typeof(Entry), nameof(BeforeLocalPlayerChoice)));
        new Harmony("sts2sim.live-diff.nested-reward-skip").Patch(
            AccessTools.Method(typeof(RewardsSetSynchronizer), nameof(RewardsSetSynchronizer.SkipLocalRewardsSet)),
            prefix: new HarmonyMethod(typeof(Entry), nameof(BeforeLocalRewardsSkipped)),
            postfix: new HarmonyMethod(typeof(Entry), nameof(AfterLocalRewardsSkipped)));
        new Harmony("sts2sim.live-diff.nested-reward-complete").Patch(
            AccessTools.Method(typeof(RewardsSetSynchronizer), "CompleteRewardsSetIfNecessary"),
            postfix: new HarmonyMethod(typeof(Entry), nameof(AfterNestedRewardsCompleted)));
        GD.Print($"[Sts2LiveDiff] READY trace={TracePath}");
        Emit(new { event_name = "ready", protocol = "sts2-live-diff-v1", trace_path = TracePath });
        if (System.Environment.GetEnvironmentVariable("STS2_LIVE_DIFF_AUTOPROBE") == "1")
            _ = AutoProbeAsync();
    }

    private static void AttachExecutor()
    {
        _env?.Dispose();
        _env = null;
        _runEnv?.Dispose();
        _runEnv = null;
        DetachEvent();
        _combat = null;
        _blocked = null;
        _sequence = 0;
        _initialRunProbed = false;
        _runSequence = 0;
        _runActionPending = false;
        _combatContinuous = false;
        _rewardsObserved = false;
        _observedRewardsSet = null;
        _observedRewardsScreen = null;
        _nestedRewards = false;
        _pendingRewardChoice = null;
        _skippedCardReward = null;
        ActionExecutor executor = RunManager.Instance.ActionExecutor;
        if (!ReferenceEquals(_attachedExecutor, executor))
        {
            if (_attachedExecutor != null)
            {
                _attachedExecutor.BeforeActionExecuted -= BeforeAction;
                _attachedExecutor.AfterActionExecuted -= AfterAction;
            }
            executor.BeforeActionExecuted += BeforeAction;
            executor.AfterActionExecuted += AfterAction;
            _attachedExecutor = executor;
        }
        Emit(new { event_name = "run_started", seed = RunManager.Instance.DebugOnlyGetState()?.Rng.StringSeed });
    }

    private static void OnRoomEntered()
    {
        RunState? run = RunManager.Instance.DebugOnlyGetState();
        if (run == null || run.CurrentRoom == null) return;
        Player? player = LocalContext.GetMe(run);
        if (player == null) return;
        if (!_initialRunProbed)
        {
            _initialRunProbed = true;
            ProbeInitialRun(run, player, "first_room_entered");
        }
        else if (_runEnv != null)
            CompareRun(run, player, "room_entered");
        WatchCurrentEvent(run);
    }

    // Called after the game's reward set has been populated and attached to
    // the visible screen. This observes the result without selecting rewards.
    private static void AfterRewardsScreenShown(RewardsSet set, bool isTerminal, NRewardsScreen __result)
    {
        if (!isTerminal)
        {
            if (_runEnv == null) return;
            try
            {
                JsonNode? choice = JsonNode.Parse(_runEnv.ExportRunQueries())?["choice"];
                if (choice?["source"]?.GetValue<string>() != "rewards") return;
                _observedRewardsSet = set;
                _observedRewardsScreen = __result;
                _nestedRewards = true;
                Emit(new { event_name = "nested_rewards_shown", count = set.Rewards.Count });
                RunState? nestedRun = RunManager.Instance.DebugOnlyGetState();
                Player? nestedPlayer = nestedRun == null ? null : LocalContext.GetMe(nestedRun);
                if (nestedRun != null && nestedPlayer != null)
                    CompareRun(nestedRun, nestedPlayer, "nested_rewards_shown");
                if (_runEnv != null) CompareRewards(set, "nested_rewards_shown");
            }
            catch (Exception ex)
            {
                Emit(new { event_name = "nested_rewards_error", message = ex.ToString() });
                BlockRun("nested_rewards_observer_failed");
            }
            return;
        }
        if (set.Room is not CombatRoom) return;
        _rewardsObserved = true;
        _observedRewardsSet = set;
        _observedRewardsScreen = __result;
        _nestedRewards = false;
        Emit(new { event_name = "combat_rewards_shown", count = set.Rewards.Count,
            continuous = _combatContinuous, env_active = _env != null });
        if (!_combatContinuous || _env == null) return;
        _runEnv?.Dispose();
        _runEnv = _env;
        _env = null;
        _combat = null;
        _combatContinuous = false;
        RunState? run = RunManager.Instance.DebugOnlyGetState();
        Player? player = run == null ? null : LocalContext.GetMe(run);
        if (run == null || player == null)
        {
            BlockRun("reward_run_state_unavailable");
            return;
        }
        CompareRun(run, player, "combat_rewards_shown");
        if (_runEnv != null) CompareRewards(set, "combat_rewards_shown");
    }

    private static void BeforeLocalRewardSelected(Reward reward)
    {
        RewardsSet? set = _observedRewardsSet;
        if (set == null || _runEnv == null) return;
        int index = set.Rewards.IndexOf(reward);
        if (index < 0) return;
        _skippedCardReward = null;
        RunState? run = RunManager.Instance.DebugOnlyGetState();
        Player? player = run == null ? null : LocalContext.GetMe(run);
        if (run == null || player == null) return;
        CompareRun(run, player, "before_reward_selected");
        if (_runEnv == null) return;
        try
        {
            NativeEnv.Action action = new() { Kind = _nestedRewards ? 12 : 4,
                Index = index, Target = -1 };
            int sequence = ++_runSequence;
            Emit(new { event_name = "run_action", sequence,
                kind = _nestedRewards ? "answer_nested_reward" : "take_reward",
                index, reward_type = reward.GetType().Name, env_action = action });
            NativeEnv next = _runEnv.Predict(action);
            _runEnv.Dispose();
            _runEnv = next;
            int? decision = JsonNode.Parse(next.Inspect())?["decision"]?.GetValue<int>();
            if (decision == 7)
            {
                string? source = JsonNode.Parse(next.ExportRunQueries())?["choice"]?["source"]?.GetValue<string>();
                Emit(new { event_name = "run_choice_pending", sequence,
                    source = _nestedRewards ? "nested_reward" : "take_reward",
                    choice_source = source,
                    reward_type = reward.GetType().Name });
                if (source == "CardSelectCmd" && reward is CardReward cardReward)
                    _pendingRewardChoice = cardReward;
                else if (_nestedRewards && source == "rewards")
                    _runActionPending = true;
                else
                    BlockRun("choice_bridge_not_implemented");
                return;
            }
            _runActionPending = true;
            Emit(new { event_name = "run_env_transition", sequence, result = "ok", decision });
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "run_transition_error", kind = "take_reward", message = ex.ToString() });
            BlockRun("reward_transition_failed");
        }
    }

    private static void BeforeLocalPlayerChoice(Player player, uint choiceId, PlayerChoiceResult result)
    {
        CardReward? cardReward = _pendingRewardChoice;
        if (cardReward == null || _runEnv == null || !ReferenceEquals(cardReward.Player, player)) return;
        try
        {
            JsonNode? choice = JsonNode.Parse(_runEnv.ExportRunQueries())?["choice"];
            if (choice?["source"]?.GetValue<string>() != "CardSelectCmd" ||
                choice["prompt"]?.GetValue<string>() != "CardReward")
            {
                BlockRun("unexpected_reward_choice");
                return;
            }
            var screenField = typeof(CardReward).GetField("_currentlyShownScreen",
                BindingFlags.Instance | BindingFlags.NonPublic);
            var optionsField = typeof(NCardRewardSelectionScreen).GetField("_extraOptions",
                BindingFlags.Instance | BindingFlags.NonPublic);
            NCardRewardSelectionScreen? screen = screenField?.GetValue(cardReward) as NCardRewardSelectionScreen;
            var alternatives = screen == null ? null : optionsField?.GetValue(screen)
                as IReadOnlyList<MegaCrit.Sts2.Core.Entities.CardRewardAlternatives.CardRewardAlternative>;
            if (alternatives == null)
            {
                BlockRun("reward_choice_screen_unavailable");
                return;
            }
            JsonArray actualOptions = new(cardReward.Cards.Select(card => (JsonNode?)JsonValue.Create(
                "card:" + card.Id.Entry)).Concat(alternatives.Select(option => (JsonNode?)JsonValue.Create(
                "alternative:" + option.OptionId))).ToArray());
            JsonArray expectedOptions = new((choice["options"] as JsonArray ?? new JsonArray())
                .Select(option => (JsonNode?)JsonValue.Create(option?["kind"]?.GetValue<string>() == "card"
                    ? "card:" + option?["cards"]?[0]?["model_id"]?["entry"]?.GetValue<string>()
                    : "alternative:" + option?["key"]?.GetValue<string>())).ToArray());
            bool optionsMatch = JsonNode.DeepEquals(actualOptions, expectedOptions);
            Emit(new { event_name = "reward_choice_options_comparison", match = optionsMatch,
                game = actualOptions, env = expectedOptions });
            if (!optionsMatch)
            {
                BlockRun("reward_choice_options_mismatch");
                return;
            }
            int index = result.AsIndexOrNull() ?? -1;
            bool legal = (choice["legal_answers"] as JsonArray ?? new JsonArray())
                .Any(answer => answer?.GetValue<int>() == index);
            if (!legal)
            {
                BlockRun("reward_choice_answer_not_legal");
                return;
            }
            NativeEnv.Action action = new() { Kind = 12, Index = index, Target = -1 };
            bool skip = index >= cardReward.Cards.Count() &&
                alternatives[index - cardReward.Cards.Count()].OptionId == "Skip";
            int sequence = ++_runSequence;
            Emit(new { event_name = "run_action", sequence, kind = "answer_reward_choice",
                choice_id = choiceId, index, env_action = action });
            NativeEnv next = _runEnv.Predict(action);
            _runEnv.Dispose();
            _runEnv = next;
            int? decision = JsonNode.Parse(next.Inspect())?["decision"]?.GetValue<int>();
            Emit(new { event_name = "run_env_transition", sequence, result = "ok", decision });
            if (skip) _skippedCardReward = cardReward;
            string? nextSource = decision == 7
                ? JsonNode.Parse(next.ExportRunQueries())?["choice"]?["source"]?.GetValue<string>()
                : null;
            if (nextSource != "CardSelectCmd") _pendingRewardChoice = null;
            if (decision == 7 && nextSource != "CardSelectCmd" &&
                !(_nestedRewards && nextSource == "rewards"))
                BlockRun("choice_bridge_not_implemented");
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "reward_choice_error", message = ex.ToString() });
            BlockRun("reward_choice_transition_failed");
        }
    }

    private static void AfterLocalRewardSelected(Reward reward, Task<bool> __result)
    {
        if (_observedRewardsSet?.Rewards.Contains(reward) != true) return;
        _ = CompareRewardAfterTaskAsync(__result, reward);
    }

    private static async Task CompareRewardAfterTaskAsync(Task<bool> task, Reward reward)
    {
        try
        {
            bool selected = await task;
            if (!selected)
            {
                bool expectedSkip = ReferenceEquals(reward, _skippedCardReward);
                _skippedCardReward = null;
                if (expectedSkip && _runEnv != null)
                {
                    NGame? skippedHost = NGame.Instance;
                    if (skippedHost != null)
                        await skippedHost.ToSignal(skippedHost.GetTree(), SceneTree.SignalName.ProcessFrame);
                    RunState? skippedRun = RunManager.Instance.DebugOnlyGetState();
                    Player? skippedPlayer = skippedRun == null ? null : LocalContext.GetMe(skippedRun);
                    if (skippedRun != null && skippedPlayer != null)
                        CompareRun(skippedRun, skippedPlayer, "after_reward_choice_skip");
                    if (_runEnv != null && _observedRewardsSet != null)
                        CompareRewards(_observedRewardsSet, "after_reward_choice_skip");
                }
                Emit(new { event_name = "reward_selected", reward_type = reward.GetType().Name,
                    selected, run_env_active = _runEnv != null });
                if (!expectedSkip && _runEnv != null) BlockRun("reward_selection_not_completed");
                return;
            }
            if (_runEnv == null) return;
            _skippedCardReward = null;
            _pendingRewardChoice = null;
            NGame? host = NGame.Instance;
            if (host != null) await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
            RunState? run = RunManager.Instance.DebugOnlyGetState();
            Player? player = run == null ? null : LocalContext.GetMe(run);
            if (run != null && player != null)
                CompareRun(run, player, "after_reward_selected");
            if (_runEnv != null && _observedRewardsSet != null &&
                CanExportRewards(_runEnv))
                CompareRewards(_observedRewardsSet, "after_reward_selected");
            Emit(new { event_name = "reward_selected", reward_type = reward.GetType().Name,
                selected, run_env_active = _runEnv != null });
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "reward_selection_observer_error", message = ex.ToString() });
            if (_runEnv != null) BlockRun("reward_selection_observer_failed");
        }
    }

    private static void BeforeLocalRewardsSkipped()
    {
        RewardsSet? set = _observedRewardsSet;
        if (!_nestedRewards || set == null || _runEnv == null) return;
        RunState? run = RunManager.Instance.DebugOnlyGetState();
        Player? player = run == null ? null : LocalContext.GetMe(run);
        if (run == null || player == null) return;
        CompareRun(run, player, "before_nested_rewards_skip");
        if (_runEnv == null) return;
        CompareRewards(set, "before_nested_rewards_skip");
        if (_runEnv == null) return;
        try
        {
            JsonNode? choice = JsonNode.Parse(_runEnv.ExportRunQueries())?["choice"];
            int index = set.Rewards.Count;
            bool legal = choice?["source"]?.GetValue<string>() == "rewards" &&
                (choice["legal_answers"] as JsonArray ?? new JsonArray())
                    .Any(answer => answer?.GetValue<int>() == index);
            if (!legal)
            {
                BlockRun("nested_reward_proceed_not_legal");
                return;
            }
            NativeEnv.Action action = new() { Kind = 12, Index = index, Target = -1 };
            int sequence = ++_runSequence;
            Emit(new { event_name = "run_action", sequence, kind = "skip_nested_rewards",
                index, env_action = action });
            NativeEnv next = _runEnv.Predict(action);
            _runEnv.Dispose();
            _runEnv = next;
            Emit(new { event_name = "run_env_transition", sequence, result = "ok",
                decision = JsonNode.Parse(next.Inspect())?["decision"]?.DeepClone() });
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "nested_reward_skip_error", message = ex.ToString() });
            BlockRun("nested_reward_skip_failed");
        }
    }

    private static void AfterLocalRewardsSkipped()
    {
        if (_nestedRewards) _ = CompareNestedRewardsAfterExitAsync("skipped");
    }

    private static void AfterNestedRewardsCompleted()
    {
        RewardsSet? set = _observedRewardsSet;
        if (!_nestedRewards || set == null || _runEnv == null ||
            !set.AllRewardsSuccessfullySelected ||
            !RunManager.Instance.RewardsSetSynchronizer.IsRewardsSetCompleted(set)) return;
        try
        {
            CompareRewards(set, "nested_rewards_completed");
            if (_runEnv == null) return;
            JsonNode? choice = JsonNode.Parse(_runEnv.ExportRunQueries())?["choice"];
            int index = set.Rewards.Count;
            if (choice?["source"]?.GetValue<string>() != "rewards")
            {
                BlockRun("nested_rewards_completed_without_proceed_choice");
                return;
            }
            NativeEnv.Action action = new() { Kind = 12, Index = index, Target = -1 };
            int sequence = ++_runSequence;
            Emit(new { event_name = "run_action", sequence, kind = "complete_nested_rewards",
                index, env_action = action });
            NativeEnv next = _runEnv.Predict(action);
            _runEnv.Dispose();
            _runEnv = next;
            Emit(new { event_name = "run_env_transition", sequence, result = "ok",
                decision = JsonNode.Parse(next.Inspect())?["decision"]?.DeepClone() });
            _ = CompareNestedRewardsAfterExitAsync("completed");
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "nested_rewards_complete_error", message = ex.ToString() });
            BlockRun("nested_rewards_complete_failed");
        }
    }

    private static async Task CompareNestedRewardsAfterExitAsync(string outcome)
    {
        try
        {
            NGame? host = NGame.Instance;
            if (host != null) await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
            RunState? run = RunManager.Instance.DebugOnlyGetState();
            Player? player = run == null ? null : LocalContext.GetMe(run);
            if (run != null && player != null && _runEnv != null)
            {
                CompareRun(run, player, "after_nested_rewards_" + outcome);
                if (_runEnv != null && _observedEvent != null &&
                    JsonNode.Parse(_runEnv.Inspect())?["decision"]?.GetValue<int>() == 3)
                    CompareEventOptions(_observedEvent, "after_nested_rewards_" + outcome);
            }
            Emit(new { event_name = "nested_rewards_exited", outcome,
                run_env_active = _runEnv != null });
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "nested_rewards_skip_observer_error", message = ex.ToString() });
            if (_runEnv != null) BlockRun("nested_rewards_skip_observer_failed");
        }
        finally
        {
            _nestedRewards = false;
            _observedRewardsSet = null;
            _observedRewardsScreen = null;
        }
    }

    private static void BeforeTerminalRewardsProceed()
    {
        if (_observedRewardsSet == null || _runEnv == null) return;
        RunState? run = RunManager.Instance.DebugOnlyGetState();
        Player? player = run == null ? null : LocalContext.GetMe(run);
        if (run == null || player == null) return;
        CompareRun(run, player, "before_reward_proceed");
        if (_runEnv == null) return;
        try
        {
            NativeEnv.Action action = new() { Kind = 5, Index = -1, Target = -1 };
            int sequence = ++_runSequence;
            Emit(new { event_name = "run_action", sequence, kind = "proceed_from_rewards",
                env_action = action });
            NativeEnv next = _runEnv.Predict(action);
            _runEnv.Dispose();
            _runEnv = next;
            int? decision = JsonNode.Parse(next.Inspect())?["decision"]?.GetValue<int>();
            Emit(new { event_name = "run_env_transition", sequence, result = "ok", decision });
            if (decision == 7)
                BlockRun("choice_bridge_not_implemented");
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "run_transition_error", kind = "proceed_from_rewards",
                message = ex.ToString() });
            BlockRun("reward_proceed_transition_failed");
        }
    }

    private static void AfterTerminalRewardsProceed(Task __result)
    {
        if (_observedRewardsSet == null) return;
        _ = CompareTerminalRewardsProceedAsync(__result);
    }

    private static async Task CompareTerminalRewardsProceedAsync(Task task)
    {
        try
        {
            await task;
            NGame? host = NGame.Instance;
            if (host != null) await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
            RunState? run = RunManager.Instance.DebugOnlyGetState();
            Player? player = run == null ? null : LocalContext.GetMe(run);
            if (run != null && player != null && _runEnv != null)
                CompareRun(run, player, "after_reward_proceed");
            Emit(new { event_name = "reward_proceeded", run_env_active = _runEnv != null });
            _observedRewardsSet = null;
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "reward_proceed_observer_error", message = ex.ToString() });
            if (_runEnv != null) BlockRun("reward_proceed_observer_failed");
        }
    }

    private static void CompareRewards(RewardsSet set, string boundary)
    {
        if (_runEnv == null) return;
        try
        {
            JsonArray actual = new(set.Rewards.Select(reward => (JsonNode)new JsonObject
            {
                ["reward"] = OracleReward(reward), ["selected"] = reward.SuccessfullySelected
            }).ToArray());
            JsonNode? expected = JsonNode.Parse(_runEnv.ExportOracleRewards());
            List<string> paths = new();
            FindDifferences(actual, expected, "$", paths);
            Emit(new { event_name = "rewards_comparison", boundary, match = paths.Count == 0,
                difference_paths = paths, game = actual, env = expected });
            if (paths.Count > 0) BlockRun("rewards_mismatch:" + string.Join(',', paths));
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "rewards_comparison_error", message = ex.ToString() });
            BlockRun("rewards_comparison_failed");
        }
    }

    private static bool CanExportRewards(NativeEnv env)
    {
        int? decision = JsonNode.Parse(env.Inspect())?["decision"]?.GetValue<int>();
        return decision == 2 || (decision == 7 &&
            JsonNode.Parse(env.ExportRunQueries())?["choice"]?["source"]?.GetValue<string>() == "rewards");
    }

    private static JsonObject OracleReward(Reward reward)
    {
        switch (reward)
        {
            case GoldReward gold:
                return new JsonObject { ["type"] = "Gold", ["amount"] = gold.Amount };
            case PotionReward potion:
                return new JsonObject { ["type"] = "Potion", ["id"] = potion.Potion?.Id.Entry };
            case CardReward card:
                bool rerolled = (bool)(typeof(CardReward).GetField("_hasBeenRerolled",
                    BindingFlags.Instance | BindingFlags.NonPublic)?.GetValue(card) ??
                    throw new InvalidOperationException("CardReward reroll field unavailable"));
                return new JsonObject
                {
                    ["type"] = "Card",
                    ["cards"] = Array(card.Cards.Select(model => model.Id.Entry +
                        (model.CurrentUpgradeLevel > 0 ? "+" + model.CurrentUpgradeLevel : ""))),
                    ["can_skip"] = card.CanSkip,
                    ["can_reroll"] = card.CanReroll,
                    ["has_been_rerolled"] = rerolled
                };
            case RelicReward relic:
                return new JsonObject { ["type"] = "Relic", ["id"] = relic.Relic?.Id.Entry };
            case SpecialCardReward special:
                CardModel cardModel = (CardModel)(typeof(SpecialCardReward).GetField("_card",
                    BindingFlags.Instance | BindingFlags.NonPublic)?.GetValue(special) ??
                    throw new InvalidOperationException("SpecialCardReward card field unavailable"));
                return new JsonObject { ["type"] = "SpecialCard", ["card"] = cardModel.Id.Entry +
                    (cardModel.CurrentUpgradeLevel > 0 ? "+" + cardModel.CurrentUpgradeLevel : "") };
            case CardRemovalReward:
                return new JsonObject { ["type"] = "RemoveCard" };
            default:
                return new JsonObject { ["type"] = reward.GetType().Name };
        }
    }

    private static void ProbeInitialRun(RunState run, Player player, string boundary)
    {
        NativeEnv? candidate = null;
        try
        {
            candidate = new NativeEnv();
            candidate.ResetRun(run.Rng.StringSeed, player.Character.Id.Entry, (int)run.AscensionLevel);
            JsonObject actual = OracleRunState(run, player);
            JsonNode? expected = JsonNode.Parse(candidate.ExportOracleRunState());
            List<string> paths = new();
            FindDifferences(actual, expected, "$", paths);
            Emit(new { event_name = "run_comparison", boundary, match = paths.Count == 0,
                difference_paths = paths, game = actual, env = expected,
                decision = JsonNode.Parse(candidate.Inspect())?["decision"]?.DeepClone() });
            GD.Print($"[Sts2LiveDiff] RUN_COMPARE boundary={boundary} match={paths.Count == 0} " +
                $"paths={string.Join(',', paths)}");
            if (paths.Count == 0)
            {
                _runEnv = candidate;
                candidate = null;
            }
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "run_comparison_error", boundary, message = ex.ToString() });
        }
        finally { candidate?.Dispose(); }
    }

    private static void CompareRun(RunState run, Player player, string boundary)
    {
        if (_runEnv == null) return;
        try
        {
            JsonObject actual = OracleRunState(run, player);
            JsonNode? expected = JsonNode.Parse(_runEnv.ExportOracleRunState());
            List<string> paths = new();
            FindDifferences(actual, expected, "$", paths);
            Emit(new { event_name = "run_comparison", boundary, sequence = _runSequence,
                match = paths.Count == 0, difference_paths = paths, game = actual, env = expected,
                decision = JsonNode.Parse(_runEnv.Inspect())?["decision"]?.DeepClone() });
            if (paths.Count > 0) BlockRun("run_mismatch:" + string.Join(',', paths));
            else _runActionPending = false;
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "run_comparison_error", boundary, message = ex.ToString() });
            BlockRun("run_comparison_failed");
        }
    }

    private static void BlockRun(string reason)
    {
        _runEnv?.Dispose();
        _runEnv = null;
        _runActionPending = false;
        _pendingRewardChoice = null;
        _skippedCardReward = null;
        Emit(new { event_name = "run_env_blocked", reason });
    }

    private static void DetachEvent()
    {
        if (_observedEvent != null) _observedEvent.StateChanged -= OnEventStateChanged;
        foreach (EventOption option in ObservedOptions) option.BeforeChosen -= BeforeEventOptionChosen;
        ObservedOptions.Clear();
        _observedEvent = null;
    }

    private static void WatchCurrentEvent(RunState run)
    {
        if (run.CurrentRoom?.RoomType != RoomType.Event)
        {
            DetachEvent();
            return;
        }
        if (_runEnv == null) return;
        EventModel gameEvent = RunManager.Instance.EventSynchronizer.GetLocalEvent();
        if (!ReferenceEquals(_observedEvent, gameEvent))
        {
            DetachEvent();
            _observedEvent = gameEvent;
            gameEvent.StateChanged += OnEventStateChanged;
        }
        foreach (EventOption option in gameEvent.CurrentOptions)
            if (ObservedOptions.Add(option)) option.BeforeChosen += BeforeEventOptionChosen;
        CompareEventOptions(gameEvent, "event_options");
    }

    private static void CompareEventOptions(EventModel gameEvent, string boundary)
    {
        if (_runEnv == null) return;
        try
        {
            JsonNode? query = JsonNode.Parse(_runEnv.ExportRunQueries());
            JsonNode? expectedEvent = query?["event"];
            JsonArray actualOptions = Array(gameEvent.CurrentOptions.Select(option => option.TextKey));
            JsonArray expectedOptions = Array((expectedEvent?["options"] as JsonArray ?? new JsonArray())
                .Select(option => option?["text_key"]?.GetValue<string>() ?? ""));
            int gameRngCounter = gameEvent.Rng.ToSerializable().counter;
            int envRngCounter = _runEnv.EventRngCounter();
            bool match = expectedEvent?["entry"]?.GetValue<string>() == gameEvent.Id.Entry &&
                JsonNode.DeepEquals(actualOptions, expectedOptions) && gameRngCounter == envRngCounter;
            Emit(new { event_name = "event_options_comparison", boundary, match,
                game_event = gameEvent.Id.Entry, game_options = actualOptions,
                env_event = expectedEvent?["entry"]?.DeepClone(), env_options = expectedOptions,
                game_rng_counter = gameRngCounter, env_rng_counter = envRngCounter });
            if (!match) BlockRun("event_options_mismatch");
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "event_options_error", boundary, message = ex.ToString() });
            BlockRun("event_options_comparison_failed");
        }
    }

    private static Task BeforeEventOptionChosen(EventOption option)
    {
        if (_runEnv == null || _observedEvent == null) return Task.CompletedTask;
        RunState? run = RunManager.Instance.DebugOnlyGetState();
        Player? player = run == null ? null : LocalContext.GetMe(run);
        if (run == null || player == null) return Task.CompletedTask;
        int index = _observedEvent.CurrentOptions.ToList().FindIndex(candidate =>
            ReferenceEquals(candidate, option));
        if (index < 0)
        {
            BlockRun("event_option_identity_missing");
            return Task.CompletedTask;
        }
        CompareRun(run, player, "before_event_option");
        CompareEventOptions(_observedEvent, "before_event_option");
        if (_runEnv == null) return Task.CompletedTask;
        try
        {
            NativeEnv.Action action = new() { Kind = 6, Index = index, Target = -1 };
            int sequence = ++_runSequence;
            Emit(new { event_name = "run_action", sequence, kind = "event_option", index,
                option = option.TextKey, env_action = action });
            NativeEnv next = _runEnv.Predict(action);
            _runEnv.Dispose();
            _runEnv = next;
            if (JsonNode.Parse(next.Inspect())?["decision"]?.GetValue<int>() == 7)
            {
                string? source = JsonNode.Parse(next.ExportRunQueries())?["choice"]?["source"]?.GetValue<string>();
                Emit(new { event_name = "run_choice_pending", sequence,
                    source = "event_option", choice_source = source, option = option.TextKey });
                if (source == "rewards")
                {
                    _runActionPending = true;
                    return Task.CompletedTask;
                }
                BlockRun("choice_bridge_not_implemented");
                return Task.CompletedTask;
            }
            _runActionPending = true;
            Emit(new { event_name = "run_env_transition", sequence, result = "ok" });
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "run_transition_error", index, message = ex.ToString() });
            BlockRun("event_transition_failed");
        }
        return Task.CompletedTask;
    }

    private static void OnEventStateChanged(EventModel gameEvent)
    {
        if (!_runActionPending || !ReferenceEquals(_observedEvent, gameEvent)) return;
        _ = CompareEventAfterFrameAsync(gameEvent);
    }

    private static async Task CompareEventAfterFrameAsync(EventModel gameEvent)
    {
        NGame? host = NGame.Instance;
        if (host == null) return;
        await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
        RunState? run = RunManager.Instance.DebugOnlyGetState();
        Player? player = run == null ? null : LocalContext.GetMe(run);
        if (run == null || player == null || _runEnv == null ||
            !ReferenceEquals(_observedEvent, gameEvent)) return;
        CompareRun(run, player, "after_event_option");
        if (_runEnv != null) WatchCurrentEvent(run);
    }

    private static void OnTurnStarted(CombatState state)
    {
        // End-turn actions complete before the enemy phase. This later event is
        // the first comparable player-turn boundary for their successor.
        if (_env == null || !ReferenceEquals(_combat, state)) return;
        Player? player = LocalContext.GetMe(state);
        if (state.CurrentSide != CombatSide.Player || player?.PlayerCombatState?.Phase != PlayerTurnPhase.Play)
            return;
        Compare("turn_started", state, player);
    }

    private static void BeforeAction(GameAction action)
    {
        if (action is MoveToMapCoordAction mapMove)
        {
            BeforeMapMove(mapMove);
            return;
        }
        CombatState? state = CombatManager.Instance.DebugOnlyGetState();
        if (state == null || !CombatManager.Instance.IsInProgress)
        {
            Emit(new { event_name = "game_action", action = action.GetType().Name, combat = false });
            return;
        }
        Player? player = LocalContext.GetMe(state);
        if (player?.PlayerCombatState == null) return;
        if (state.CurrentSide != CombatSide.Player || player.PlayerCombatState.Phase != PlayerTurnPhase.Play ||
            action is not (PlayCardAction or EndPlayerTurnAction or UsePotionAction))
        {
            Emit(new { event_name = "game_action", action = action.GetType().Name,
                combat = true, phase = player.PlayerCombatState.Phase.ToString() });
            return;
        }
        try
        {
            if (!ReferenceEquals(_combat, state)) StartCombat(state, player);
            if (_env != null) Compare("before_action", state, player);
            int sequence = ++_sequence;
            if (!TryEncode(action, player, out NativeEnv.Action envAction, out string? description))
            {
                Emit(new { event_name = "action_unsupported", sequence, action = action.GetType().Name,
                    description, game = Capture(state, player) });
                Block("unsupported_game_action:" + action.GetType().Name);
                return;
            }
            Emit(new { event_name = "action", sequence, action = action.GetType().Name,
                description, env_action = envAction, game = Capture(state, player) });
            if (_env == null) return;
            NativeEnv next = _env.Predict(envAction);
            _env.Dispose();
            _env = next;
            if (JsonNode.Parse(next.Inspect())?["decision"]?.GetValue<int>() == 7)
            {
                Emit(new { event_name = "combat_choice_pending", sequence,
                    action = action.GetType().Name });
                Block("choice_bridge_not_implemented");
                return;
            }
            Emit(new { event_name = "env_transition", sequence, result = "ok" });
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "error", stage = "before_action", action = action.GetType().Name,
                message = ex.ToString() });
            Block("transition_or_capture_failed");
        }
    }

    private static void BeforeMapMove(MoveToMapCoordAction mapMove)
    {
        RunState? run = RunManager.Instance.DebugOnlyGetState();
        Player? player = run == null ? null : LocalContext.GetMe(run);
        if (run == null || player == null) return;
        NetMoveToMapCoordAction net = (NetMoveToMapCoordAction)mapMove.ToNetAction();
        MapCoord destination = net.destination;
        Emit(new { event_name = "game_map_move", destination = new[] { destination.col, destination.row },
            run_env_active = _runEnv != null });
        if (_runEnv == null) return;
        CompareRun(run, player, "before_map_move");
        if (_runEnv == null) return;
        try
        {
            NativeEnv.Action encoded = new() { Kind = 1, Index = -1, Target = -1,
                Col = destination.col, Row = destination.row };
            int sequence = ++_runSequence;
            Emit(new { event_name = "run_action", sequence, kind = "map_move", env_action = encoded });
            NativeEnv next = _runEnv.Predict(encoded);
            _runEnv.Dispose();
            _runEnv = next;
            if (JsonNode.Parse(next.Inspect())?["decision"]?.GetValue<int>() == 7)
            {
                Emit(new { event_name = "run_choice_pending", sequence, source = "map_move" });
                BlockRun("choice_bridge_not_implemented");
                return;
            }
            _runActionPending = true;
            Emit(new { event_name = "run_env_transition", sequence, result = "ok" });
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "run_transition_error", kind = "map_move", message = ex.ToString() });
            BlockRun("map_transition_failed");
        }
    }

    private static void AfterAction(GameAction action)
    {
        CombatState? state = CombatManager.Instance.DebugOnlyGetState();
        if (_env == null || state == null || !ReferenceEquals(state, _combat) ||
            action is EndPlayerTurnAction) return;
        Player? player = LocalContext.GetMe(state);
        if (player?.PlayerCombatState?.Phase != PlayerTurnPhase.Play) return;
        try
        {
            int? decision = JsonNode.Parse(_env.Inspect())?["decision"]?.GetValue<int>();
            if (decision != 1)
            {
                Emit(new { event_name = "combat_terminal_pending", sequence = _sequence,
                    decision, action = action.GetType().Name });
                return;
            }
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "error", stage = "after_action_decision", message = ex.ToString() });
            Block("after_action_decision_failed");
            return;
        }
        Compare("after_action", state, player);
    }

    private static void StartCombat(CombatState state, Player player)
    {
        _env?.Dispose();
        _env = null;
        _combatContinuous = false;
        _combat = state;
        _blocked = null;
        _sequence = 0;
        string? encounter = state.Encounter?.Id.Entry;
        Emit(new { event_name = "combat_started", encounter, game = Capture(state, player) });
        if (encounter == null) { Block("no_encounter"); return; }
        if (_runEnv != null)
        {
            _env = _runEnv;
            _runEnv = null;
            _combatContinuous = true;
            Emit(new { event_name = "env_continuous_combat", encounter,
                sequence = _runSequence, env = JsonNode.Parse(_env.Inspect()) });
            return;
        }
        bool independentProbe = System.Environment.GetEnvironmentVariable("STS2_LIVE_DIFF_AUTOPROBE") == "1" &&
            System.Environment.GetEnvironmentVariable("STS2_LIVE_DIFF_AUTOPROBE_NEOW") != "1";
        if (!independentProbe)
        {
            Block("run_lineage_unavailable");
            return;
        }
        NativeEnv? candidate = null;
        try
        {
            candidate = new NativeEnv();
            string result = candidate.ResetCombat(state.RunState.Rng.StringSeed,
                player.Character.Id.Entry, (int)state.RunState.AscensionLevel,
                encounter, state.RunState.CurrentActIndex);
            _env = candidate;
            candidate = null;
            Emit(new { event_name = "env_reset", encounter, env = JsonNode.Parse(result) });
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "error", stage = "env_reset", message = ex.ToString() });
            Block("env_reset_failed");
        }
        finally { candidate?.Dispose(); }
    }

    private static void Compare(string boundary, CombatState state, Player player)
    {
        if (_env == null) return;
        try
        {
            JsonNode? inspection = JsonNode.Parse(_env.Inspect());
            JsonObject? expected = inspection?["root"] as JsonObject;
            JsonObject actual = Root(state, player);
            if (expected == null)
            {
                Emit(new { event_name = "comparison", boundary, match = false,
                    reason = "env_not_at_combat_decision", game = Capture(state, player), env = inspection });
                Block("env_not_at_combat_decision");
                return;
            }
            string[] differences = actual.Select(kv => kv.Key)
                .Where(key => !JsonNode.DeepEquals(actual[key], expected[key]))
                .ToArray();
            JsonNode? expectedCombat = JsonNode.Parse(_env.ExportOracleCombatState());
            JsonObject actualCombat = OracleCombatState(state, player);
            List<string> combatDifferences = new();
            FindDifferences(actualCombat, expectedCombat, "$", combatDifferences);
            Emit(new { event_name = "comparison", boundary, sequence = _sequence,
                match = differences.Length == 0 && combatDifferences.Count == 0,
                difference_fields = differences, combat_difference_paths = combatDifferences,
                game = Capture(state, player), game_root = actual, env = inspection,
                game_combat = actualCombat, env_combat = expectedCombat });
            GD.Print($"[Sts2LiveDiff] COMPARE boundary={boundary} sequence={_sequence} " +
                $"match={differences.Length == 0 && combatDifferences.Count == 0} " +
                $"fields={string.Join(',', differences)} combat={string.Join(',', combatDifferences)}");
            if (differences.Length > 0) Block("root_mismatch:" + string.Join(',', differences));
            else if (combatDifferences.Count > 0) Block("combat_mismatch:" + string.Join(',', combatDifferences));
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "error", stage = "compare", boundary, message = ex.ToString() });
            Block("comparison_failed");
        }
    }

    private static bool TryEncode(GameAction action, Player player, out NativeEnv.Action encoded, out string? description)
    {
        encoded = new NativeEnv.Action { Index = -1, Target = -1 };
        description = null;
        if (action is PlayCardAction play)
        {
            CardModel? card = play.NetCombatCard.ToCardModelOrNull();
            int index = card == null ? -1 : player.PlayerCombatState!.Hand.Cards.ToList()
                .FindIndex(candidate => ReferenceEquals(candidate, card));
            if (index < 0) return false;
            encoded.Kind = 2;
            encoded.Index = index;
            encoded.Target = play.TargetId.HasValue ? checked((int)play.TargetId.Value) : -1;
            description = $"play:{card!.Id.Entry}:hand={index}:target={encoded.Target}";
            return true;
        }
        if (action is EndPlayerTurnAction)
        {
            encoded.Kind = 3;
            description = "end_turn";
            return true;
        }
        if (action is UsePotionAction potion && potion.WasEnqueuedInCombat)
        {
            encoded.Kind = 10;
            encoded.Index = checked((int)potion.PotionIndex);
            encoded.Target = potion.TargetId.HasValue ? checked((int)potion.TargetId.Value) : -1;
            description = $"use_potion:slot={encoded.Index}:target={encoded.Target}";
            return true;
        }
        return false;
    }

    private static JsonObject Root(CombatState state, Player player)
    {
        PlayerCombatState pcs = player.PlayerCombatState!;
        JsonObject root = new();
        AddPile(root, "hand", pcs.Hand);
        AddPile(root, "draw", pcs.DrawPile);
        AddPile(root, "discard", pcs.DiscardPile);
        AddPile(root, "exhaust", pcs.ExhaustPile);
        root["relics"] = Array(player.Relics.Select(relic => relic.Id.Entry + (relic.IsMelted ? ":1" : ":0")));
        root["potions"] = Array(player.PotionSlots.Select(potion => potion?.Id.Entry ?? ""));
        root["player_powers"] = Powers(player.Creature);
        JsonArray enemies = new();
        foreach (Creature enemy in state.Enemies)
        {
            enemies.Add(new JsonObject
            {
                ["id"] = enemy.CombatId.HasValue ? (int)enemy.CombatId.Value : -1,
                ["monster"] = enemy.Monster?.Id.Entry,
                ["hp"] = enemy.CurrentHp,
                ["max_hp"] = enemy.MaxHp,
                ["block"] = enemy.Block,
                ["powers"] = Powers(enemy)
            });
        }
        root["enemies"] = enemies;
        root["energy"] = pcs.Energy;
        root["hp"] = player.Creature.CurrentHp;
        root["max_hp"] = player.Creature.MaxHp;
        root["player_block"] = player.Creature.Block;
        return root;
    }

    private static void AddPile(JsonObject root, string key, CardPile pile)
    {
        root[key] = Array(pile.Cards.Select(card => card.Id.Entry));
        root[key + "_states"] = Array(pile.Cards.Select(card =>
            card.Id.Entry + "+" + card.CurrentUpgradeLevel + "/" +
            (card.EnergyCost.CostsX ? "1" : "0") + ":" +
            card.EnergyCost.GetWithModifiers(CostModifiers.Local) + "/" +
            (card.Enchantment?.Id.Entry ?? "-") + ":" + (card.Enchantment?.Amount ?? 0) + "/" +
            (card.Affliction?.Id.Entry ?? "-") + ":" + (card.Affliction?.Amount ?? 0)));
    }

    private static JsonArray Powers(Creature creature) =>
        Array(creature.Powers.Select(power => power.Id.Entry + ":" + power.Amount));

    private static JsonArray Array(IEnumerable<string> values) =>
        new(values.Select(value => (JsonNode?)JsonValue.Create(value)).ToArray());

    private static JsonArray OraclePowers(Creature creature) =>
        new(creature.Powers.Select(power => (JsonNode)new JsonArray(power.Id.Entry, power.Amount)).ToArray());

    private static JsonArray OraclePile(CardPile pile) =>
        Array(pile.Cards.Select(card => card.Id.Entry +
            (card.CurrentUpgradeLevel > 0 ? "+" + card.CurrentUpgradeLevel : "")));

    private static JsonArray OracleIntents(Creature enemy, IReadOnlyList<Creature> targets)
    {
        JsonArray intents = new();
        if (enemy.Monster?.NextMove == null) return intents;
        foreach (AbstractIntent intent in enemy.Monster.NextMove.Intents)
        {
            JsonObject item = new() { ["type"] = intent.IntentType.ToString() };
            if (intent is AttackIntent attack)
            {
                item["damage"] = attack.GetSingleDamage(targets, enemy);
                item["repeats"] = attack.Repeats;
            }
            intents.Add(item);
        }
        return intents;
    }

    private static JsonObject OracleCombatState(CombatState combat, Player player)
    {
        PlayerCombatState pcs = player.PlayerCombatState!;
        JsonArray enemies = new();
        foreach (Creature enemy in combat.Enemies)
            enemies.Add(new JsonObject
            {
                ["id"] = enemy.Monster!.Id.Entry,
                ["combat_id"] = (int)enemy.CombatId!.Value,
                ["hp"] = enemy.CurrentHp,
                ["max_hp"] = enemy.MaxHp,
                ["block"] = enemy.Block,
                ["powers"] = OraclePowers(enemy),
                ["next_move"] = enemy.Monster.NextMove?.Id,
                ["intents"] = OracleIntents(enemy, combat.PlayerCreatures),
                ["rng_counter"] = enemy.Monster.Rng.ToSerializable().counter
            });
        JsonObject rng = new();
        foreach (RunRngType kind in Enum.GetValues<RunRngType>())
            rng[kind.ToString()] = combat.RunState.Rng.GetRng(kind).ToSerializable().counter;
        return new JsonObject
        {
            ["round"] = combat.RoundNumber,
            ["turn"] = pcs.TurnNumber,
            ["test_subject_kills"] = combat.RunState.ExtraFields.TestSubjectKills,
            ["player"] = new JsonObject
            {
                ["hp"] = player.Creature.CurrentHp,
                ["max_hp"] = player.Creature.MaxHp,
                ["block"] = player.Creature.Block,
                ["energy"] = pcs.Energy,
                ["stars"] = pcs.Stars,
                ["powers"] = OraclePowers(player.Creature),
                ["hand"] = OraclePile(pcs.Hand),
                ["hand_costs"] = new JsonArray(pcs.Hand.Cards.Select(card =>
                    (JsonNode)JsonValue.Create(card.EnergyCost.GetWithModifiers(CostModifiers.All))!).ToArray()),
                ["hand_afflictions"] = new JsonArray(pcs.Hand.Cards.Select(card =>
                    (JsonNode?)card.Affliction?.Id.Entry).ToArray()),
                ["draw"] = OraclePile(pcs.DrawPile),
                ["discard"] = OraclePile(pcs.DiscardPile),
                ["exhaust"] = OraclePile(pcs.ExhaustPile),
                ["play"] = OraclePile(pcs.PlayPile),
                ["relics"] = new JsonArray(player.Relics.Select(relic =>
                {
                    JsonObject item = new() { ["id"] = relic.Id.Entry };
                    if (relic.ShowCounter) item["counter"] = relic.DisplayAmount;
                    return (JsonNode)item;
                }).ToArray()),
                ["potion_slots"] = new JsonArray(player.PotionSlots.Select(potion =>
                    (JsonNode?)potion?.Id.Entry).ToArray()),
                ["orb_capacity"] = pcs.OrbQueue.Capacity,
                ["orbs"] = Array(pcs.OrbQueue.Orbs.Select(orb => orb.Id.Entry)),
                ["orb_values"] = new JsonArray(pcs.OrbQueue.Orbs.Select(orb =>
                    (JsonNode)JsonValue.Create((int)orb.EvokeVal)!).ToArray()),
                ["pets"] = new JsonArray(pcs.Pets.Select(pet => (JsonNode)new JsonObject
                {
                    ["id"] = pet.Monster!.Id.Entry,
                    ["combat_id"] = (int)pet.CombatId!.Value,
                    ["hp"] = pet.CurrentHp,
                    ["max_hp"] = pet.MaxHp,
                    ["block"] = pet.Block,
                    ["powers"] = OraclePowers(pet),
                    ["next_move"] = pet.Monster.NextMove?.Id
                }).ToArray())
            },
            ["enemies"] = enemies,
            ["rng"] = rng
        };
    }

    private static JsonObject OracleRunState(RunState run, Player player)
    {
        MapCoord? coord = run.CurrentMapCoord;
        JsonObject rng = new();
        foreach (RunRngType kind in Enum.GetValues<RunRngType>())
            rng[kind.ToString()] = run.Rng.GetRng(kind).ToSerializable().counter;
        JsonObject playerRng = new();
        foreach (var pair in player.PlayerRng.ToSerializable().Rngs)
            playerRng[pair.Key.ToString()] = pair.Value.counter;
        return new JsonObject
        {
            ["act"] = run.CurrentActIndex,
            ["total_floor"] = run.TotalFloor,
            ["act_floor"] = run.ActFloor,
            ["coord"] = coord.HasValue ? new JsonArray(coord.Value.col, coord.Value.row) : null,
            ["room"] = run.CurrentRoom?.RoomType.ToString(),
            ["hp"] = player.Creature.CurrentHp,
            ["max_hp"] = player.Creature.MaxHp,
            ["gold"] = player.Gold,
            ["deck"] = Array(player.Deck.Cards.Select(card => card.Id.Entry +
                (card.CurrentUpgradeLevel > 0 ? "+" + card.CurrentUpgradeLevel : ""))),
            ["relics"] = new JsonArray(player.Relics.Select(relic =>
            {
                JsonObject item = new() { ["id"] = relic.Id.Entry };
                if (relic.ShowCounter) item["counter"] = relic.DisplayAmount;
                return (JsonNode)item;
            }).ToArray()),
            ["potions"] = new JsonArray(player.PotionSlots.Select(potion =>
                (JsonNode?)potion?.Id.Entry).ToArray()),
            ["rng"] = rng,
            ["player_rng"] = playerRng,
            ["odds"] = new JsonObject
            {
                ["card_rarity"] = player.PlayerOdds.CardRarity.CurrentValue,
                ["potion_reward"] = player.PlayerOdds.PotionReward.CurrentValue,
                ["unknown"] = new JsonObject
                {
                    ["Monster"] = run.Odds.UnknownMapPoint.MonsterOdds,
                    ["Elite"] = run.Odds.UnknownMapPoint.EliteOdds,
                    ["Treasure"] = run.Odds.UnknownMapPoint.TreasureOdds,
                    ["Shop"] = run.Odds.UnknownMapPoint.ShopOdds
                }
            }
        };
    }

    private static void FindDifferences(JsonNode? actual, JsonNode? expected, string path, List<string> paths)
    {
        if (paths.Count >= 32 || JsonNode.DeepEquals(actual, expected)) return;
        if (actual is JsonObject actualObject && expected is JsonObject expectedObject)
        {
            foreach (string key in actualObject.Select(pair => pair.Key).Union(expectedObject.Select(pair => pair.Key)))
                FindDifferences(actualObject[key], expectedObject[key], path + "." + key, paths);
            return;
        }
        if (actual is JsonArray actualArray && expected is JsonArray expectedArray)
        {
            for (int index = 0; index < Math.Max(actualArray.Count, expectedArray.Count); index++)
                FindDifferences(index < actualArray.Count ? actualArray[index] : null,
                    index < expectedArray.Count ? expectedArray[index] : null,
                    path + "[" + index + "]", paths);
            return;
        }
        paths.Add(path);
    }

    private static object Capture(CombatState state, Player player)
    {
        PlayerCombatState pcs = player.PlayerCombatState!;
        Dictionary<string, int> rng = new();
        foreach (RunRngType kind in Enum.GetValues<RunRngType>())
            rng[kind.ToString()] = state.RunState.Rng.GetRng(kind).ToSerializable().counter;
        return new
        {
            seed = state.RunState.Rng.StringSeed,
            character = player.Character.Id.Entry,
            ascension = (int)state.RunState.AscensionLevel,
            act = state.RunState.CurrentActIndex,
            encounter = state.Encounter?.Id.Entry,
            round = state.RoundNumber,
            turn = pcs.TurnNumber,
            phase = pcs.Phase.ToString(),
            hp = player.Creature.CurrentHp,
            max_hp = player.Creature.MaxHp,
            block = player.Creature.Block,
            energy = pcs.Energy,
            stars = pcs.Stars,
            hand = pcs.Hand.Cards.Select(card => card.Id.Entry + "+" + card.CurrentUpgradeLevel).ToArray(),
            draw = pcs.DrawPile.Cards.Select(card => card.Id.Entry + "+" + card.CurrentUpgradeLevel).ToArray(),
            discard = pcs.DiscardPile.Cards.Select(card => card.Id.Entry + "+" + card.CurrentUpgradeLevel).ToArray(),
            exhaust = pcs.ExhaustPile.Cards.Select(card => card.Id.Entry + "+" + card.CurrentUpgradeLevel).ToArray(),
            enemies = state.Enemies.Select(enemy => new { id = enemy.Monster?.Id.Entry,
                combat_id = enemy.CombatId, hp = enemy.CurrentHp, max_hp = enemy.MaxHp,
                block = enemy.Block, next_move = enemy.Monster?.NextMove?.Id,
                rng_counter = enemy.Monster?.Rng.ToSerializable().counter }).ToArray(),
            run_rng = rng
        };
    }

    private static void Block(string reason)
    {
        if (_blocked == reason) return;
        _blocked = reason;
        _env?.Dispose();
        _env = null;
        Emit(new { event_name = "env_blocked", reason });
        GD.Print($"[Sts2LiveDiff] BLOCKED reason={reason}");
    }

    private static string ResolveTracePath()
    {
        string? configured = System.Environment.GetEnvironmentVariable("STS2_LIVE_DIFF_PATH");
        if (!string.IsNullOrWhiteSpace(configured)) return Path.GetFullPath(configured);
        string root = System.Environment.GetEnvironmentVariable("XDG_DATA_HOME")
            ?? Path.Combine(System.Environment.GetFolderPath(System.Environment.SpecialFolder.UserProfile), ".local", "share");
        return Path.Combine(root, "SlayTheSpire2", "sts2-live-diff.jsonl");
    }

    private static void Emit(object value)
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(TracePath)!);
            File.AppendAllText(TracePath, JsonSerializer.Serialize(value, TraceJsonOptions) + "\n");
        }
        catch (Exception ex)
        {
            GD.PrintErr($"[Sts2LiveDiff] trace_write_failed {ex}");
        }
    }

    private static async Task AutoProbeAsync()
    {
        NGame? host = NGame.Instance;
        if (host == null)
        {
            Emit(new { event_name = "autoprobe_error", message = "NGame.Instance is null at mod initialization" });
            return;
        }
        try
        {
            await host.GameStartupComplete;
            string seedInput = System.Environment.GetEnvironmentVariable("STS2_LIVE_DIFF_SEED") ?? "LIVEDIFF001";
            string seed = SeedHelper.CanonicalizeSeed(seedInput);
            Emit(new { event_name = "autoprobe_seed", seed_input = seedInput, seed });
            CharacterModel character = ModelDb.AllCharacters.Single(model => model.Id.Entry == "IRONCLAD");
            EncounterModel encounter = ModelDb.All.OfType<EncounterModel>()
                .Single(model => model.Id.Entry == "CULTISTS_NORMAL");
            bool oracleStyle = System.Environment.GetEnvironmentVariable("STS2_LIVE_DIFF_AUTOPROBE_MODE") == "oracle";
            var unlocks = oracleStyle ? UnlockState.all : SaveManager.Instance.GenerateUnlockStateFromProgress();
            var actRng = new Rng(StringHelper.GetDeterministicHashCode(seed), "act_selection");
            ActModel[] acts = ModelDb.ActsByIndex
                .Select(candidates => actRng.NextItem(candidates.Where(act => act.IsUnlocked(unlocks)).ToList())!)
                .ToArray();
            Emit(new { event_name = "autoprobe_acts", acts = acts.Select(act => act.Id.Entry).ToArray(),
                number_of_runs = unlocks.NumberOfRuns });
            if (oracleStyle)
            {
                Player startingPlayer = Player.CreateForNewRun(character, unlocks, 1uL);
                RunState run = RunState.CreateForNewRun(new[] { startingPlayer },
                    acts.Select(act => act.ToMutable()).ToList(), System.Array.Empty<ModifierModel>(),
                    GameMode.Standard, 0, seed);
                Checkpoint("created", run);
                RunManager.Instance.SetUpNewSingleplayer(run, shouldSave: false);
                Checkpoint("setup", run);
                await PreloadManager.LoadRunAssets(run.Players.Select(player => player.Character));
                Checkpoint("run_assets", run);
                await PreloadManager.LoadActAssets(run.Acts[0]);
                Checkpoint("act_assets", run);
                await RunManager.Instance.FinalizeStartingRelics();
                Checkpoint("starting_relics", run);
                RunManager.Instance.Launch();
                Checkpoint("launch", run);
                if (System.Environment.GetEnvironmentVariable("STS2_LIVE_DIFF_AUTOPROBE_SAVE") == "1")
                    Emit(new { event_name = "autoprobe_launch_save", seed,
                        save = JsonNode.Parse(JsonSerializationUtility.ToJson(RunManager.Instance.ToSave(null))) });
                host.RootSceneContainer.SetCurrentScene(NRun.Create(run));
                Checkpoint("scene", run);
                await RunManager.Instance.SetActInternal(0);
                Checkpoint("act", run);
            }
            else
            {
                RunState run = await host.StartNewSingleplayerRun(character, shouldSave: false, acts,
                    System.Array.Empty<ModifierModel>(), seed, GameMode.Standard);
                Checkpoint("normal_start", run);
            }
            if (System.Environment.GetEnvironmentVariable("STS2_LIVE_DIFF_AUTOPROBE_NEOW") == "1")
            {
                RunState neowRun = RunManager.Instance.DebugOnlyGetState()!;
                EventModel neow = RunManager.Instance.EventSynchronizer.GetLocalEvent();
                if (neow.Id.Entry != "NEOW" || neow.CurrentOptions.Count == 0)
                    throw new InvalidOperationException("autoprobe expected Neow with options");
                int neowIndex = int.TryParse(System.Environment.GetEnvironmentVariable(
                    "STS2_LIVE_DIFF_AUTOPROBE_NEOW_INDEX"), out int configuredIndex) ? configuredIndex : 0;
                if (neowIndex < 0 || neowIndex >= neow.CurrentOptions.Count)
                    throw new InvalidOperationException("autoprobe Neow index out of range");
                Emit(new { event_name = "autoprobe_event_action", event_id = neow.Id.Entry,
                    index = neowIndex, option = neow.CurrentOptions[neowIndex].TextKey });
                RunManager.Instance.EventSynchronizer.ChooseLocalOption(neowIndex);
                Task pendingNeow = RunManager.Instance.EventSynchronizer.AwaitPendingOptionTasks();
                bool handledCustomRewards = false;
                for (int frame = 0; frame < 600 && !pendingNeow.IsCompleted; frame++)
                {
                    if (!handledCustomRewards && _nestedRewards &&
                        System.Environment.GetEnvironmentVariable(
                            "STS2_LIVE_DIFF_AUTOPROBE_NEOW_CUSTOM_REWARDS") == "1")
                    {
                        handledCustomRewards = true;
                        if (_observedRewardsScreen == null)
                            throw new InvalidOperationException("Neow custom reward screen unavailable");
                        bool chooseCard = System.Environment.GetEnvironmentVariable(
                            "STS2_LIVE_DIFF_AUTOPROBE_NEOW_CUSTOM_CARD") == "1";
                        bool selected;
                        if (chooseCard)
                        {
                            CardReward cardReward = _observedRewardsSet?.Rewards.OfType<CardReward>().FirstOrDefault()
                                ?? throw new InvalidOperationException("Neow custom card reward unavailable");
                            CardModel customCard = cardReward.Cards.First();
                            Task<bool> selection = RunManager.Instance.RewardsSetSynchronizer
                                .SelectLocalReward(cardReward);
                            await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                            NCardRewardSelectionScreen screen = typeof(CardReward).GetField("_currentlyShownScreen",
                                BindingFlags.Instance | BindingFlags.NonPublic)?.GetValue(cardReward)
                                as NCardRewardSelectionScreen ??
                                throw new InvalidOperationException("Neow card selection screen unavailable");
                            await Task.Delay(450);
                            NCardHolder holder = screen.GetCardHolder(customCard);
                            Emit(new { event_name = "autoprobe_custom_reward_action",
                                kind = "take_card", card = customCard.Id.Entry });
                            holder.EmitSignal(NCardHolder.SignalName.Pressed, holder);
                            selected = await selection;
                        }
                        else
                        {
                            PotionReward potion = _observedRewardsSet?.Rewards.OfType<PotionReward>()
                                .FirstOrDefault() ?? throw new InvalidOperationException("Neow potion reward unavailable");
                            Emit(new { event_name = "autoprobe_custom_reward_action",
                                kind = "take_potion", potion = potion.Potion?.Id.Entry });
                            selected = await RunManager.Instance.RewardsSetSynchronizer.SelectLocalReward(potion);
                        }
                        await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                        Emit(new { event_name = "autoprobe_custom_reward_result", selected,
                            run_env_active = _runEnv != null });
                        if (System.Environment.GetEnvironmentVariable(
                            "STS2_LIVE_DIFF_AUTOPROBE_NEOW_CUSTOM_ALL") == "1")
                        {
                            if (!chooseCard)
                                throw new InvalidOperationException("custom all-rewards probe requires card first");
                            PotionReward potion = _observedRewardsSet?.Rewards.OfType<PotionReward>()
                                .FirstOrDefault() ?? throw new InvalidOperationException("remaining potion unavailable");
                            Emit(new { event_name = "autoprobe_custom_reward_action",
                                kind = "take_potion", potion = potion.Potion?.Id.Entry });
                            bool potionSelected = await RunManager.Instance.RewardsSetSynchronizer
                                .SelectLocalReward(potion);
                            Emit(new { event_name = "autoprobe_custom_reward_result",
                                selected = potionSelected, run_env_active = _runEnv != null });
                        }
                        else
                        {
                            typeof(NRewardsScreen).GetMethod("OnProceedButtonPressed",
                                BindingFlags.Instance | BindingFlags.NonPublic)?.Invoke(
                                    _observedRewardsScreen, new object?[] { null });
                        }
                    }
                    await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                }
                if (!pendingNeow.IsCompleted)
                    throw new TimeoutException("autoprobe Neow choice still needs input after 600 frames");
                await pendingNeow;
                await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                Player? neowPlayer = LocalContext.GetMe(neowRun);
                if (_runEnv != null && neowPlayer != null)
                    CompareRun(neowRun, neowPlayer, "autoprobe_after_neow");
                Emit(new { event_name = "autoprobe_event_result", event_id = neow.Id.Entry,
                    finished = neow.IsFinished, run_env_active = _runEnv != null,
                    sequence = _runSequence });
                if (System.Environment.GetEnvironmentVariable("STS2_LIVE_DIFF_AUTOPROBE_MAP") == "1")
                {
                    MapPoint current = neowRun.CurrentMapPoint ??
                        throw new InvalidOperationException("autoprobe has no current map point");
                    MapPoint nextPoint = MapTravel.GetTravelablePointsFrom(neowRun, current)
                        .OrderBy(point => point.coord.col).First();
                    Player? movingPlayer = LocalContext.GetMe(neowRun);
                    if (movingPlayer == null) throw new InvalidOperationException("autoprobe has no local player");
                    Emit(new { event_name = "autoprobe_map_action",
                        destination = new[] { nextPoint.coord.col, nextPoint.coord.row } });
                    RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(
                        new MoveToMapCoordAction(movingPlayer, nextPoint.coord));
                    await RunManager.Instance.ActionExecutor.FinishedExecutingActions();
                    await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                    Emit(new { event_name = "autoprobe_map_result", run_env_active = _runEnv != null,
                        room = neowRun.CurrentRoom?.RoomType.ToString(),
                        coord = neowRun.CurrentMapCoord.HasValue
                            ? new[] { neowRun.CurrentMapCoord.Value.col, neowRun.CurrentMapCoord.Value.row }
                            : null });
                    if (System.Environment.GetEnvironmentVariable("STS2_LIVE_DIFF_AUTOPROBE_MAP_COMBAT") == "1")
                    {
                        CombatState? mapCombat = null;
                        Player? mapPlayer = null;
                        for (int frame = 0; frame < 600; frame++)
                        {
                            await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                            mapCombat = CombatManager.Instance.DebugOnlyGetState();
                            mapPlayer = mapCombat == null ? null : LocalContext.GetMe(mapCombat);
                            if (mapCombat?.CurrentSide == CombatSide.Player &&
                                mapPlayer?.PlayerCombatState?.Phase == PlayerTurnPhase.Play &&
                                !RunManager.Instance.ActionExecutor.IsRunning) break;
                        }
                        if (mapCombat == null || mapPlayer?.PlayerCombatState?.Phase != PlayerTurnPhase.Play)
                            throw new InvalidOperationException("map combat did not reach player play phase");
                        CardModel mapCard = mapPlayer.PlayerCombatState.Hand.Cards.First(model =>
                            model.Id.Entry == "STRIKE_IRONCLAD");
                        Creature mapTarget = mapCombat.Enemies.First();
                        Emit(new { event_name = "autoprobe_map_combat_action", card = mapCard.Id.Entry,
                            target = mapTarget.CombatId });
                        RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(
                            new PlayCardAction(mapCard, mapTarget));
                        await RunManager.Instance.ActionExecutor.FinishedExecutingActions();
                        await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                        if (System.Environment.GetEnvironmentVariable(
                            "STS2_LIVE_DIFF_AUTOPROBE_MAP_COMBAT_END_TURN") == "1")
                        {
                            RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(
                                new EndPlayerTurnAction(mapPlayer, mapPlayer.PlayerCombatState.TurnNumber));
                            await RunManager.Instance.ActionExecutor.FinishedExecutingActions();
                            for (int frame = 0; frame < 600; frame++)
                            {
                                await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                                if (mapPlayer.PlayerCombatState.Phase == PlayerTurnPhase.Play &&
                                    mapPlayer.PlayerCombatState.TurnNumber > 1 &&
                                    !RunManager.Instance.ActionExecutor.IsRunning) break;
                            }
                        }
                        if (System.Environment.GetEnvironmentVariable(
                            "STS2_LIVE_DIFF_AUTOPROBE_MAP_COMBAT_FINISH") == "1")
                        {
                            int policyActions = 0;
                            int idleFrames = 0;
                            while (policyActions < 100 && idleFrames < 6000 && !_rewardsObserved)
                            {
                                CombatState? currentCombat = CombatManager.Instance.DebugOnlyGetState();
                                if (currentCombat == null || !CombatManager.Instance.IsInProgress)
                                {
                                    await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                                    idleFrames++;
                                    continue;
                                }
                                PlayerCombatState? pcs = mapPlayer.PlayerCombatState;
                                if (pcs == null || currentCombat.CurrentSide != CombatSide.Player ||
                                    pcs.Phase != PlayerTurnPhase.Play || RunManager.Instance.ActionExecutor.IsRunning)
                                {
                                    await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                                    idleFrames++;
                                    continue;
                                }
                                idleFrames = 0;
                                int step = policyActions++;
                                Creature? enemy = currentCombat.Enemies.FirstOrDefault(creature => creature.IsAlive);
                                CardModel? strike = enemy == null ? null : pcs.Hand.Cards.FirstOrDefault(model =>
                                    model.Id.Entry == "STRIKE_IRONCLAD" &&
                                    model.EnergyCost.GetWithModifiers(CostModifiers.All) <= pcs.Energy);
                                CardModel? defend = pcs.Hand.Cards.FirstOrDefault(model =>
                                    model.Id.Entry == "DEFEND_IRONCLAD" &&
                                    model.EnergyCost.GetWithModifiers(CostModifiers.All) <= pcs.Energy);
                                if (strike != null && enemy != null)
                                {
                                    Emit(new { event_name = "autoprobe_combat_policy", step,
                                        kind = "strike", target = enemy.CombatId });
                                    RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(
                                        new PlayCardAction(strike, enemy));
                                }
                                else if (defend != null)
                                {
                                    Emit(new { event_name = "autoprobe_combat_policy", step, kind = "defend" });
                                    RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(
                                        new PlayCardAction(defend, null));
                                }
                                else
                                {
                                    Emit(new { event_name = "autoprobe_combat_policy", step, kind = "end_turn" });
                                    RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(
                                        new EndPlayerTurnAction(mapPlayer, pcs.TurnNumber));
                                }
                                await RunManager.Instance.ActionExecutor.FinishedExecutingActions();
                                await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                            }
                            for (int frame = 0; frame < 600 && !_rewardsObserved; frame++)
                                await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                            Emit(new { event_name = "autoprobe_reward_result", rewards_observed = _rewardsObserved,
                                run_env_active = _runEnv != null, combat_env_active = _env != null,
                                blocked = _blocked, combat_actions = _sequence,
                                policy_actions = policyActions, idle_frames = idleFrames,
                                combat_in_progress = CombatManager.Instance.IsInProgress,
                                player_hp = mapPlayer.Creature.CurrentHp });
                            if (_rewardsObserved && System.Environment.GetEnvironmentVariable(
                                "STS2_LIVE_DIFF_AUTOPROBE_TAKE_GOLD") == "1")
                            {
                                GoldReward? gold = _observedRewardsSet?.Rewards.OfType<GoldReward>().FirstOrDefault();
                                if (gold == null)
                                    throw new InvalidOperationException("combat reward set has no gold reward");
                                bool selected = await RunManager.Instance.RewardsSetSynchronizer.SelectLocalReward(gold);
                                for (int frame = 0; frame < 3; frame++)
                                    await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                                Emit(new { event_name = "autoprobe_reward_take_result", selected,
                                    amount = gold.Amount, player_gold = mapPlayer.Gold,
                                    run_env_active = _runEnv != null, blocked = _blocked });
                            }
                            if (_rewardsObserved && System.Environment.GetEnvironmentVariable(
                                "STS2_LIVE_DIFF_AUTOPROBE_TAKE_CARD") == "1")
                            {
                                CardReward? cardReward = _observedRewardsSet?.Rewards
                                    .OfType<CardReward>().FirstOrDefault();
                                if (cardReward == null)
                                    throw new InvalidOperationException("combat reward set has no card reward");
                                int cardIndex = int.TryParse(System.Environment.GetEnvironmentVariable(
                                    "STS2_LIVE_DIFF_AUTOPROBE_CARD_INDEX"), out int configuredCardIndex)
                                    ? configuredCardIndex : 0;
                                CardModel chosen = cardReward.Cards.ElementAt(cardIndex);
                                Task<bool> selection = RunManager.Instance.RewardsSetSynchronizer
                                    .SelectLocalReward(cardReward);
                                await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                                var screenField = typeof(CardReward).GetField("_currentlyShownScreen",
                                    BindingFlags.Instance | BindingFlags.NonPublic);
                                NCardRewardSelectionScreen? screen = screenField?.GetValue(cardReward)
                                    as NCardRewardSelectionScreen;
                                if (screen == null)
                                    throw new InvalidOperationException("card reward selection screen not shown");
                                await Task.Delay(450);
                                bool skipCard = System.Environment.GetEnvironmentVariable(
                                    "STS2_LIVE_DIFF_AUTOPROBE_SKIP_CARD") == "1";
                                if (skipCard)
                                {
                                    Emit(new { event_name = "autoprobe_card_reward_action", kind = "skip" });
                                    typeof(NCardRewardSelectionScreen).GetMethod("OnAlternateRewardSelected",
                                        BindingFlags.Instance | BindingFlags.NonPublic)?.Invoke(screen, new object[] { 0 });
                                }
                                else
                                {
                                    NCardHolder holder = screen.GetCardHolder(chosen);
                                    Emit(new { event_name = "autoprobe_card_reward_action", kind = "card",
                                        card = chosen.Id.Entry });
                                    holder.EmitSignal(NCardHolder.SignalName.Pressed, holder);
                                }
                                bool selected = await selection;
                                for (int frame = 0; frame < 3; frame++)
                                    await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                                Emit(new { event_name = "autoprobe_card_reward_result", selected,
                                    card = skipCard ? null : chosen.Id.Entry, run_env_active = _runEnv != null,
                                    blocked = _blocked });
                            }
                            if (_rewardsObserved && System.Environment.GetEnvironmentVariable(
                                "STS2_LIVE_DIFF_AUTOPROBE_REWARD_PROCEED") == "1")
                            {
                                await RunManager.Instance.ProceedFromTerminalRewardsScreen();
                                for (int frame = 0; frame < 3; frame++)
                                    await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                                Emit(new { event_name = "autoprobe_reward_proceed_result",
                                    run_env_active = _runEnv != null, blocked = _blocked });
                                if (System.Environment.GetEnvironmentVariable(
                                    "STS2_LIVE_DIFF_AUTOPROBE_NEXT_MAP") == "1")
                                {
                                    MapPoint from = neowRun.CurrentMapPoint ??
                                        throw new InvalidOperationException("no map point after combat rewards");
                                    MapPoint next = MapTravel.GetTravelablePointsFrom(neowRun, from)
                                        .OrderBy(point => point.coord.col).First();
                                    Emit(new { event_name = "autoprobe_next_map_action",
                                        destination = new[] { next.coord.col, next.coord.row } });
                                    RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(
                                        new MoveToMapCoordAction(mapPlayer, next.coord));
                                    await RunManager.Instance.ActionExecutor.FinishedExecutingActions();
                                    await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                                    Emit(new { event_name = "autoprobe_next_map_result",
                                        run_env_active = _runEnv != null, blocked = _blocked,
                                        room = neowRun.CurrentRoom?.RoomType.ToString(),
                                        coord = neowRun.CurrentMapCoord.HasValue
                                            ? new[] { neowRun.CurrentMapCoord.Value.col,
                                                neowRun.CurrentMapCoord.Value.row }
                                            : null });
                                    if (neowRun.CurrentRoom?.RoomType == RoomType.Event &&
                                        System.Environment.GetEnvironmentVariable(
                                            "STS2_LIVE_DIFF_AUTOPROBE_NEXT_EVENT_OPTION") == "1")
                                    {
                                        EventModel nextEvent = RunManager.Instance.EventSynchronizer.GetLocalEvent();
                                        int optionIndex = int.TryParse(System.Environment.GetEnvironmentVariable(
                                            "STS2_LIVE_DIFF_AUTOPROBE_NEXT_EVENT_INDEX"), out int configuredEventIndex)
                                            ? configuredEventIndex
                                            : nextEvent.CurrentOptions.ToList().FindIndex(option => !option.IsLocked);
                                        if (optionIndex < 0 || optionIndex >= nextEvent.CurrentOptions.Count ||
                                            nextEvent.CurrentOptions[optionIndex].IsLocked)
                                            throw new InvalidOperationException("next event probe has no usable option");
                                        Emit(new { event_name = "autoprobe_next_event_action",
                                            event_id = nextEvent.Id.Entry, index = optionIndex,
                                            option = nextEvent.CurrentOptions[optionIndex].TextKey });
                                        RunManager.Instance.EventSynchronizer.ChooseLocalOption(optionIndex);
                                        Task pendingEvent = RunManager.Instance.EventSynchronizer
                                            .AwaitPendingOptionTasks();
                                        for (int frame = 0; frame < 600 && !pendingEvent.IsCompleted; frame++)
                                            await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                                        if (!pendingEvent.IsCompleted)
                                            throw new TimeoutException("next event option still needs input after 600 frames");
                                        await pendingEvent;
                                        await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                                        if (_runEnv != null)
                                            CompareRun(neowRun, mapPlayer, "autoprobe_after_next_event");
                                        Emit(new { event_name = "autoprobe_next_event_result",
                                            event_id = nextEvent.Id.Entry, finished = nextEvent.IsFinished,
                                            run_env_active = _runEnv != null, blocked = _blocked });
                                        if (nextEvent.IsFinished &&
                                            System.Environment.GetEnvironmentVariable(
                                                "STS2_LIVE_DIFF_AUTOPROBE_AFTER_EVENT_MAP") == "1")
                                        {
                                            MapPoint afterEvent = neowRun.CurrentMapPoint ??
                                                throw new InvalidOperationException("no map point after event");
                                            MapPoint following = MapTravel.GetTravelablePointsFrom(neowRun, afterEvent)
                                                .OrderBy(point => point.coord.col).First();
                                            Emit(new { event_name = "autoprobe_after_event_map_action",
                                                destination = new[] { following.coord.col,
                                                    following.coord.row } });
                                            RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(
                                                new MoveToMapCoordAction(mapPlayer, following.coord));
                                            await RunManager.Instance.ActionExecutor.FinishedExecutingActions();
                                            await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                                            Emit(new { event_name = "autoprobe_after_event_map_result",
                                                run_env_active = _runEnv != null, blocked = _blocked,
                                                room = neowRun.CurrentRoom?.RoomType.ToString(),
                                                coord = neowRun.CurrentMapCoord.HasValue
                                                    ? new[] { neowRun.CurrentMapCoord.Value.col,
                                                        neowRun.CurrentMapCoord.Value.row }
                                                    : null });
                                        }
                                    }
                                }
                            }
                        }
                        Emit(new { event_name = "autoprobe_map_combat_result", env_active = _env != null,
                            blocked = _blocked, actions = _sequence,
                            turn = mapPlayer.PlayerCombatState.TurnNumber });
                    }
                }
                return;
            }
            await RunManager.Instance.EnterMapCoordDebug(new MapCoord(0, 0), RoomType.Monster,
                MapPointType.Monster, encounter.ToMutable(), showTransition: false);
            Checkpoint("combat_entered", RunManager.Instance.DebugOnlyGetState()!);
            CombatState? combat = null;
            Player? player = null;
            for (int frame = 0; frame < 600; frame++)
            {
                await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                combat = CombatManager.Instance.DebugOnlyGetState();
                player = combat == null ? null : LocalContext.GetMe(combat);
                if (combat?.CurrentSide == CombatSide.Player &&
                    player?.PlayerCombatState?.Phase == PlayerTurnPhase.Play &&
                    !RunManager.Instance.ActionExecutor.IsRunning) break;
            }
            if (combat == null || player?.PlayerCombatState?.Phase != PlayerTurnPhase.Play)
                throw new InvalidOperationException("autoprobe did not reach player play phase");
            CardModel card = player.PlayerCombatState.Hand.Cards.First(model =>
                model.Id.Entry == "STRIKE_IRONCLAD");
            Creature target = combat.Enemies.First();
            Emit(new { event_name = "autoprobe_action", kind = "play", card = card.Id.Entry,
                target = target.CombatId });
            RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(new PlayCardAction(card, target));
            await RunManager.Instance.ActionExecutor.FinishedExecutingActions();
            await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
            if (System.Environment.GetEnvironmentVariable("STS2_LIVE_DIFF_AUTOPROBE_DEFEND") == "1")
            {
                CardModel? defend = player.PlayerCombatState.Hand.Cards.FirstOrDefault(model =>
                    model.Id.Entry == "DEFEND_IRONCLAD");
                if (defend == null)
                    Emit(new { event_name = "autoprobe_skip", kind = "defend", reason = "not_in_hand" });
                else
                {
                    Emit(new { event_name = "autoprobe_action", kind = "play", card = defend.Id.Entry,
                        target = (uint?)null });
                    RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(new PlayCardAction(defend, null));
                    await RunManager.Instance.ActionExecutor.FinishedExecutingActions();
                    await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                }
            }
            if (System.Environment.GetEnvironmentVariable("STS2_LIVE_DIFF_AUTOPROBE_END_TURN") == "1")
            {
                Emit(new { event_name = "autoprobe_action", kind = "end_turn", turn = player.PlayerCombatState.TurnNumber });
                RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(
                    new EndPlayerTurnAction(player, player.PlayerCombatState.TurnNumber));
                await RunManager.Instance.ActionExecutor.FinishedExecutingActions();
                for (int frame = 0; frame < 600; frame++)
                {
                    await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                    if (player.PlayerCombatState.Phase == PlayerTurnPhase.Play &&
                        player.PlayerCombatState.TurnNumber > 1 && !RunManager.Instance.ActionExecutor.IsRunning)
                        break;
                }
                Emit(new { event_name = "autoprobe_turn_result", turn = player.PlayerCombatState.TurnNumber,
                    phase = player.PlayerCombatState.Phase.ToString(), env_active = _env != null, blocked = _blocked });
            }
            Emit(new { event_name = "autoprobe_done", seed, env_active = _env != null,
                blocked = _blocked, actions = _sequence });
        }
        catch (Exception ex)
        {
            Emit(new { event_name = "autoprobe_error", message = ex.ToString() });
            GD.PrintErr($"[Sts2LiveDiff] AUTOPROBE_ERROR {ex}");
        }
        finally
        {
            host.GetTree().Quit();
        }
    }

    private static void Checkpoint(string stage, RunState state)
    {
        Emit(new { event_name = "autoprobe_checkpoint", stage,
            seed = state.Rng.StringSeed,
            up_front = state.Rng.GetRng(RunRngType.UpFront).ToSerializable().counter,
            shuffle = state.Rng.GetRng(RunRngType.Shuffle).ToSerializable().counter,
            act = state.CurrentActIndex,
            coord = state.CurrentMapCoord.HasValue
                ? new[] { state.CurrentMapCoord.Value.col, state.CurrentMapCoord.Value.row }
                : null });
    }
}
