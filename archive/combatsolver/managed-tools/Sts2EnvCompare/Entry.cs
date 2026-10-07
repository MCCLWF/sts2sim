using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Diagnostics;
using Godot;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Context;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.GameActions;
using MegaCrit.Sts2.Core.Modding;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Nodes;
using MegaCrit.Sts2.Core.Runs;

namespace Sts2EnvCompare;

// The game and CombatSolver own the live battle. This mod captures only a
// main-thread, immutable description and passes scalars to a separate C++ Env.
[ModInitializer(nameof(Initialize))]
public static class Entry
{
    // Optional derived-solver diagnostics. The companion assembly has no
    // dependency on native search internals and leaves this provider unset.
    internal static Func<string?>? NativeBranchWarning { get; set; }
    private static (CombatState State, int Turn)? _lastTurn;
    private static readonly int Iterations = ReadPositive("STS2ENV_COMPARE_ITERATIONS", 100);
    private static readonly int EnvBeamWidth = ReadPositive("STS2ENV_COMPARE_BEAM_WIDTH", 8);
    private static readonly int RolloutDepth = ReadPositive("STS2ENV_COMPARE_DEPTH", 12);
    // The standalone comparison project has no CombatSolver settings assembly,
    // so it keeps the environment-variable fallback. The derived Mod wires
    // this provider to its persisted Native/Env/Both selector at startup.
    internal static Func<string?>? BackendModeProvider { get; set; }
    internal static Func<CombatState, Player, string?>? EnvExecutionGate { get; set; }
    internal static Func<CombatState, Player, EnvLiveSnapshotResult>? EnvLiveSnapshotProvider { get; set; }

    private static string CompareMode
        => BackendModeProvider?.Invoke() ?? ReadMode();

    public static void Initialize()
    {
        NativeEnv.Configure();
        CombatManager.Instance.TurnStarted += OnTurnStarted;
        GD.Print($"[Sts2EnvCompare] READY backend=sts2sim_reference_beam native=CombatSolver " +
                 $"mode={CompareMode} node_budget={Iterations} beam_width={EnvBeamWidth} depth={RolloutDepth}");
    }

    private static int ReadPositive(string name, int fallback)
        => int.TryParse(System.Environment.GetEnvironmentVariable(name), out int value) && value > 0 ? value : fallback;

    private static string ReadMode()
    {
        string mode = System.Environment.GetEnvironmentVariable("STS2ENV_COMPARE_MODE")?.ToLowerInvariant() ?? "both";
        return mode is "env" or "native" or "both" ? mode : "both";
    }

    private static void OnTurnStarted(CombatState state)
    {
        string mode = CompareMode;
        if (mode == "native")
            return;
        if (state.CurrentSide != CombatSide.Player || NGame.Instance == null)
            return;
        Player? player = LocalContext.GetMe(state);
        if (player?.Character.Id.Entry != "IRONCLAD" || player.PlayerCombatState == null)
            return;
        int turn = player.PlayerCombatState.TurnNumber;
        if (_lastTurn is { } previous && ReferenceEquals(previous.State, state) && previous.Turn == turn)
            return;
        _lastTurn = (state, turn);
        _ = CompareAtStableRootAsync(state, turn, mode);
    }

    private static async Task CompareAtStableRootAsync(CombatState state, int turn, string mode)
    {
        bool nativeSuspended = false;
        EnvSearchRun? envSearch = null;
        try
        {
            NGame? host = NGame.Instance;
            if (host == null)
                return;
            for (int frame = 0; frame < 3; frame++)
                await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
            await RunManager.Instance.ActionExecutor.FinishedExecutingActions();
            Player? player = LocalContext.GetMe(state);
            if (player?.PlayerCombatState?.Phase != PlayerTurnPhase.Play
                || player.PlayerCombatState.TurnNumber != turn
                || state.Encounter == null || !CombatManager.Instance.IsInProgress)
                return;

            LiveRoot live = LiveRoot.Capture(state, player);
            string seed = state.RunState.Rng.StringSeed;
            string encounter = state.Encounter.Id.Entry;
            int ascension = (int)state.RunState.AscensionLevel;
            int actIndex = state.RunState.CurrentActIndex;
            if (mode == "native")
                return;
            EnvLiveSnapshotResult? liveSnapshot = null;
            if (EnvLiveSnapshotProvider is { } captureLive)
            {
                liveSnapshot = captureLive(state, player);
                if (liveSnapshot.Value.FailureReason is { } captureFailure)
                {
                    GD.Print($"[Sts2EnvCompare] ENV_MODE_UNAVAILABLE turn={turn} " +
                        $"reason={captureFailure} root_source=live_capture env_imported=false env_used=false native_search=continues");
                    ComparisonOverlay.ShowUnavailable(host, $"战士双搜索  {encounter}  回合 {turn}\n" +
                        $"Env 未使用：{captureFailure}\n当前由 Native 继续搜索和执行。");
                    return;
                }
                if (string.IsNullOrWhiteSpace(liveSnapshot.Value.Snapshot))
                {
                    const string reason = "live_snapshot_provider_returned_empty";
                    GD.Print($"[Sts2EnvCompare] ENV_MODE_UNAVAILABLE turn={turn} reason={reason} " +
                        "root_source=live_capture env_imported=false env_used=false native_search=continues");
                    ComparisonOverlay.ShowUnavailable(host, $"战士双搜索  {encounter}  回合 {turn}\n" +
                        $"Env 未使用：{reason}\n当前由 Native 继续搜索和执行。");
                    return;
                }
            }
            if (mode == "env")
            {
                if (EnvExecutionGate?.Invoke(state, player) is { } unavailable)
                {
                    GD.Print($"[Sts2EnvCompare] ENV_MODE_UNAVAILABLE turn={turn} reason={unavailable} env_used=false native_search=continues");
                    ComparisonOverlay.ShowUnavailable(host, $"战士双搜索  {encounter}  回合 {turn}\nEnv 未使用：{unavailable}\n当前由 Native 继续搜索和执行。");
                    return;
                }
                if (!CombatSolverBridge.SetDisabled(true))
                {
                    GD.Print($"[Sts2EnvCompare] ENV_MODE_UNAVAILABLE turn={turn} " +
                             "reason=CombatSolver_control_unavailable env_search=skipped");
                    ComparisonOverlay.Show(host, $"战士双搜索  {encounter}  回合 {turn}\n" +
                        "Env 模式不可用：无法安全暂停原生搜索");
                    return;
                }
                nativeSuspended = true;
            }
            ComparisonOverlay.Show(host, $"战士双搜索  {encounter}  回合 {turn}\nEnv Beam 搜索中；CombatSolver 搜索中");
            Task<EnvSearchRun> envTask = Task.Run(() => liveSnapshot is { Snapshot: { } snapshot }
                ? NativeEnv.SolveImportedSnapshot(snapshot, Iterations, EnvBeamWidth, RolloutDepth, live,
                    mode == "env")
                : NativeEnv.SolveIfRootMatches(seed, encounter, ascension, actIndex, Iterations,
                    EnvBeamWidth, RolloutDepth, live, mode == "env"));
            envSearch = await envTask;
            if (envSearch.ErrorReason is { } failed)
            {
                GD.Print($"[Sts2EnvCompare] ENV_UNAVAILABLE turn={turn} reason={failed} env_used=false native_search=continues");
                ComparisonOverlay.ShowUnavailable(host, $"战士双搜索  {encounter}  回合 {turn}\n" +
                    $"Env 未使用：{failed}\n当前由 Native 继续搜索和执行。");
                return;
            }
            if (envSearch.UnportedReason is { } unported)
            {
                GD.Print($"[Sts2EnvCompare] ENV_UNPORTED turn={turn} encounter={encounter} " +
                         $"reason={unported} env_search=skipped comparison=skipped");
                ComparisonOverlay.Show(host, $"战士双搜索  {encounter}  回合 {turn}\n" +
                    $"Env 未使用：未移植内容（{unported}）");
                return;
            }
            if (!envSearch.RootMatches)
            {
                bool unchangedAtSkip = player.PlayerCombatState.Phase == PlayerTurnPhase.Play
                                       && player.PlayerCombatState.TurnNumber == turn
                                       && live.SameAs(LiveRoot.Capture(state, player));
                string mismatchFields = string.Join(",", envSearch.RootMismatchFields);
                GD.Print($"[Sts2EnvCompare] ENV_ROOT_CHECK turn={turn} encounter={encounter} " +
                         $"structural_root_match=false mismatch_fields={mismatchFields} " +
                         $"live_unchanged={unchangedAtSkip} env_search=skipped");
                ComparisonOverlay.Show(host, $"战士双搜索  {encounter}  回合 {turn}\n" +
                    $"Env 未搜索：根结构字段不同（{mismatchFields}）");
                return;
            }

            using JsonDocument document = JsonDocument.Parse(envSearch.ResultJson
                ?? throw new InvalidOperationException("Env root matched but returned no search result"));
            JsonElement result = document.RootElement;
            string[] resultMismatchFields = live.DifferenceFields(result.GetProperty("root"));
            bool structuralMatch = resultMismatchFields.Length == 0;
            bool liveUnchanged = player.PlayerCombatState.Phase == PlayerTurnPhase.Play
                                 && player.PlayerCombatState.TurnNumber == turn
                                 && live.SameAs(LiveRoot.Capture(state, player));
            double envMs = result.GetProperty("elapsed_ms").GetDouble();
            SearchAction envAction = LiveRoot.ReadAction(result, live);
            EnvPlanAction[] envPlan = NativeEnv.ReadPlan(result);
            string action = envAction.ToString();
            GD.Print($"[Sts2EnvCompare] ENV turn={turn} encounter={encounter} " +
                     $"root_source={(liveSnapshot.HasValue ? "live_snapshot_import" : "seed_snapshot_import")} " +
                     $"structural_root_match={structuralMatch} " +
                     $"root_mismatch_fields={string.Join(",", resultMismatchFields)} live_unchanged={liveUnchanged} " +
                     $"algorithm=beam node_budget={Iterations} beam_width={EnvBeamWidth} depth={RolloutDepth} " +
                     $"elapsed_ms={envMs:F3} plan_actions={envPlan.Length} action={action}");
            ComparisonOverlay.Show(host, $"战士双搜索  {encounter}  回合 {turn}\n" +
                $"Env Beam: {envMs:F1} ms / {Iterations} nodes / width {EnvBeamWidth} / {action}\n" +
                $"根结构: {(structuralMatch ? "一致" : "不同")}，实机状态: {(liveUnchanged ? "稳定" : "已变化")}");
            if (!structuralMatch || !liveUnchanged)
            {
                GD.Print("[Sts2EnvCompare] COMPARISON_SKIPPED reason=root_or_live_mismatch");
                return;
            }

            if (mode == "env")
            {
                int deployed = await DeployEnvPlanAsync(state, turn, envSearch, envPlan);
                if (deployed > 0)
                    await RunManager.Instance.ActionExecutor.FinishedExecutingActions();
                GD.Print($"[Sts2EnvCompare] ENV_ONLY turn={turn} encounter={encounter} " +
                         $"native_wait=skipped env_deployed_actions={deployed}");
                ComparisonOverlay.Show(host, $"战士双搜索  {encounter}  回合 {turn}\n" +
                    $"Env Beam / {Iterations} nodes / width {EnvBeamWidth} / {action}\n" +
                    (deployed > 0 ? $"Env 已执行 {deployed} 个动作，原生搜索恢复" : "Env 动作未执行，原生搜索恢复"));
                return;
            }

            // CombatSolver's result is internal. Read only its completed metrics;
            // never mutate its route or use reflection to deploy an action.
            Stopwatch wait = Stopwatch.StartNew();
            while (wait.Elapsed < TimeSpan.FromMinutes(3))
            {
                await host.ToSignal(host.GetTree(), SceneTree.SignalName.ProcessFrame);
                ComparisonOverlay.RefreshBranchWarning();
                if (!CombatManager.Instance.IsInProgress || player.PlayerCombatState.TurnNumber != turn)
                    return;
                if (!CombatSolverMetrics.TryReadCompleted(turn, out double nativeMs, out long expanded,
                                                          out SearchAction nativeAction))
                    continue;
                bool actionAgree = envAction == nativeAction;
                string nativePlanReason = "native_plan_unavailable";
                bool nativePlanRead = CombatSolverMetrics.TryReadPlan(turn, out NativePlanAction[] nativePlan);
                bool nativePlanChecked = nativePlanRead && (liveSnapshot is { Snapshot: { } nativePlanRoot }
                    ? NativeEnv.VerifyNativePlanSnapshot(nativePlanRoot, live, nativePlan, out nativePlanReason)
                    : NativeEnv.VerifyNativePlan(seed, encounter, ascension, actIndex, live, nativePlan, out nativePlanReason));
                GD.Print($"[Sts2EnvCompare] TWO_SOLVERS turn={turn} encounter={encounter} " +
                         $"env_ms={envMs:F3} native_ms={nativeMs:F3} " +
                         $"env_nodes={Iterations} env_beam_width={EnvBeamWidth} native_expanded={expanded} " +
                         $"env_action={envAction} native_action={nativeAction} action_agree={actionAgree} " +
                         $"native_plan_actions={nativePlan.Length} native_plan_env_check={nativePlanChecked} " +
                         $"native_plan_reason={nativePlanReason} " +
                         "root_check=structural_only algorithms=env_beam_vs_native_beam budgets=not_equivalent " +
                         $"native_branch_verification_enabled={NativeBranchWarning != null}");
                ComparisonOverlay.Show(host, $"战士双搜索  {encounter}  回合 {turn}\n" +
                    $"Env Beam: {envMs:F1} ms / {Iterations} nodes / width {EnvBeamWidth} / {action}\n" +
                    $"CombatSolver: {nativeMs:F1} ms / {expanded} nodes / {nativeAction}\n" +
                    $"首动作: {(actionAgree ? "一致" : "不同")}\n" +
                    "根仅结构核对；两者都是 Beam 搜索但策略细节与预算不同");
                return;
            }
            GD.Print($"[Sts2EnvCompare] NATIVE_TIMEOUT turn={turn} elapsed_wait_ms=180000");
        }
        catch (Exception error)
        {
            GD.PushError($"[Sts2EnvCompare] FAILED {error}");
        }
        finally
        {
            envSearch?.Dispose();
            if (nativeSuspended)
                CombatSolverBridge.SetDisabled(false);
        }
    }

    private static async Task<int> DeployEnvPlanAsync(CombatState state, int turn,
                                                       EnvSearchRun run, EnvPlanAction[] plan)
    {
        if (run.Session == null || plan.Length == 0)
            return 0;
        int deployed = 0;
        foreach (EnvPlanAction action in plan)
        {
            Player? player = LocalContext.GetMe(state);
            if (player?.PlayerCombatState == null || player.PlayerCombatState.TurnNumber != turn
                || player.PlayerCombatState.Phase != PlayerTurnPhase.Play)
                break;
            LiveRoot live = LiveRoot.Capture(state, player);
            using JsonDocument current = JsonDocument.Parse(run.Session.InspectJson());
            string[] mismatch = live.DifferenceFields(current.RootElement.GetProperty("root"));
            if (mismatch.Length > 0)
            {
                GD.Print($"[Sts2EnvCompare] ENV_DEPLOY_STOP reason=state_mismatch fields={string.Join(",", mismatch)}");
                break;
            }
            EnvAction envAction = new(action.Kind, action.Index, action.Target, action.Col, action.Row);
            NativeEnv.Session successor;
            try
            {
                successor = run.Session.Predict(envAction);
            }
            catch (Exception error)
            {
                GD.Print($"[Sts2EnvCompare] ENV_DEPLOY_STOP reason=prediction_failed error={error.Message}");
                break;
            }
            bool executed = false;
            if (action.Kind == 2) // Action::PlayCard
            {
                if (action.Index < 0 || action.Index >= player.PlayerCombatState.Hand.Cards.Count)
                {
                    successor.Dispose();
                    break;
                }
                CardModel card = player.PlayerCombatState.Hand.Cards[action.Index];
                MegaCrit.Sts2.Core.Entities.Creatures.Creature? target = action.Target >= 0
                    ? state.GetCreature((uint)action.Target)
                    : null;
                executed = card.TryManualPlay(target);
            }
            else if (action.Kind == 3) // Action::EndTurn
            {
                CombatManager.Instance.OnEndedTurnLocally();
                RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(
                    new EndPlayerTurnAction(player, turn));
                executed = true;
            }
            if (!executed)
            {
                successor.Dispose();
                break;
            }
            await RunManager.Instance.ActionExecutor.FinishedExecutingActions();
            run.Session.Dispose();
            run.Session = successor;
            deployed++;
            if (action.Kind == 3)
                break;
        }
        return deployed;
    }
}

internal static class ComparisonOverlay
{
    private static NGame? _host;
    private static Label? _label;
    private static string _message = "";
    private static string? _lastWarning;

    public static void ShowUnavailable(NGame host, string message)
    {
        Show(host, message);
        if (_label!.Text.Contains(message, StringComparison.Ordinal))
            GD.Print($"[Sts2EnvCompare] ENV_UNAVAILABLE_WARNING shown=true text={message}");
    }

    public static void Show(NGame host, string message)
    {
        if (!ReferenceEquals(_host, host) || !GodotObject.IsInstanceValid(_label))
        {
            CanvasLayer layer = new() { Layer = 90, Name = "Sts2EnvCompareOverlay" };
            Label label = new() { Name = "Sts2EnvCompareText", Position = new Vector2(20, 540) };
            label.AddThemeColorOverride("font_color", Colors.White);
            label.AddThemeColorOverride("font_shadow_color", Colors.Black);
            label.AddThemeConstantOverride("shadow_offset_x", 2);
            label.AddThemeConstantOverride("shadow_offset_y", 2);
            layer.AddChild(label);
            host.AddChild(layer);
            _host = host;
            _label = label;
            _lastWarning = null;
        }
        _message = message;
        RefreshBranchWarning();
    }

    internal static void RefreshBranchWarning()
    {
        if (!GodotObject.IsInstanceValid(_label))
            return;
        string? warning = Entry.NativeBranchWarning?.Invoke();
        _label!.Text = warning is null ? _message : $"{_message}\n{warning}";
        if (warning != null && warning != _lastWarning)
            GD.Print($"[Sts2EnvCompare] ENV_BRANCH_WARNING shown=true text={warning}");
        _lastWarning = warning;
    }
}

internal sealed record EnemyRoot(uint Id, string Monster, int Hp, int MaxHp, int Block, string[] Powers)
{
    public bool SameAs(EnemyRoot other)
        => Id == other.Id && Monster == other.Monster && Hp == other.Hp && MaxHp == other.MaxHp
           && Block == other.Block && Powers.SequenceEqual(other.Powers);
}

internal sealed record SearchAction(string Kind, string CardId = "", uint? TargetId = null)
{
    public override string ToString() => Kind == "PlayCard" ? $"{Kind}:{CardId}:target={TargetId?.ToString() ?? "none"}" : Kind;
}

public readonly record struct EnvAction(int Kind, int Index = -1, int Target = -1, int Col = 0, int Row = 0);

internal readonly record struct EnvLiveSnapshotResult(string? Snapshot, string? FailureReason)
{
    internal static EnvLiveSnapshotResult Failed(string reason) => new(null, reason);
    internal static EnvLiveSnapshotResult Ready(string snapshot) => new(snapshot, null);
}

public sealed class EnvPredictionUnportedException(string message) : InvalidOperationException(message);

internal sealed class EnvSearchRun : IDisposable
{
    public bool RootMatches { get; }
    public string[] RootMismatchFields { get; }
    public string? ResultJson { get; }
    public string? UnportedReason { get; }
    public string? ErrorReason { get; }
    public NativeEnv.Session? Session { get; set; }

    public EnvSearchRun(bool rootMatches, string[] rootMismatchFields, string? resultJson,
                        string? unportedReason = null, NativeEnv.Session? session = null, string? errorReason = null)
    {
        RootMatches = rootMatches;
        RootMismatchFields = rootMismatchFields;
        ResultJson = resultJson;
        UnportedReason = unportedReason;
        ErrorReason = errorReason;
        Session = session;
    }

    public void Dispose()
    {
        Session?.Dispose();
        Session = null;
    }
}

internal readonly record struct EnvPlanAction(int Kind, int Index, int Target, int Col, int Row);

internal readonly record struct NativePlanAction(string Kind, string CardId, int CardOccurrence,
                                                 uint? TargetCombatId, string PotionId, int PotionSlot,
                                                 string CardStateKey, int CardStateOccurrence,
                                                 bool HasChoices);

internal sealed record LiveRoot(string[] Hand, string[] Draw, string[] Discard, string[] Exhaust,
                                string[] HandStates, string[] DrawStates, string[] DiscardStates, string[] ExhaustStates,
                                string[] Relics, string[] Potions, string[] PlayerPowers,
                                EnemyRoot[] Enemies, int Energy, int Hp, int MaxHp, int PlayerBlock)
{
    public bool SameAs(LiveRoot other)
        => Energy == other.Energy && Hp == other.Hp && MaxHp == other.MaxHp && PlayerBlock == other.PlayerBlock
           && Hand.SequenceEqual(other.Hand) && Draw.SequenceEqual(other.Draw)
           && Discard.SequenceEqual(other.Discard) && Exhaust.SequenceEqual(other.Exhaust)
           && HandStates.SequenceEqual(other.HandStates) && DrawStates.SequenceEqual(other.DrawStates)
           && DiscardStates.SequenceEqual(other.DiscardStates) && ExhaustStates.SequenceEqual(other.ExhaustStates)
           && Relics.SequenceEqual(other.Relics) && Potions.SequenceEqual(other.Potions)
           && PlayerPowers.SequenceEqual(other.PlayerPowers)
           && Enemies.Length == other.Enemies.Length
           && Enemies.Zip(other.Enemies).All(pair => pair.First.SameAs(pair.Second));

    public static LiveRoot Capture(CombatState state, Player player)
        => new(player.PlayerCombatState!.Hand.Cards.Select(card => card.Id.Entry).ToArray(),
               player.PlayerCombatState.DrawPile.Cards.Select(card => card.Id.Entry).ToArray(),
               player.PlayerCombatState.DiscardPile.Cards.Select(card => card.Id.Entry).ToArray(),
               player.PlayerCombatState.ExhaustPile.Cards.Select(card => card.Id.Entry).ToArray(),
               player.PlayerCombatState.Hand.Cards.Select(CardStateKey).ToArray(),
               player.PlayerCombatState.DrawPile.Cards.Select(CardStateKey).ToArray(),
               player.PlayerCombatState.DiscardPile.Cards.Select(CardStateKey).ToArray(),
               player.PlayerCombatState.ExhaustPile.Cards.Select(CardStateKey).ToArray(),
               player.Relics.Select(relic => $"{relic.Id.Entry}:{(relic.IsMelted ? 1 : 0)}").ToArray(),
               player.PotionSlots.Select(potion => potion?.Id.Entry ?? "").ToArray(),
               PowerKeys(player.Creature),
               state.Enemies.Select(enemy => new EnemyRoot(enemy.CombatId ?? 0,
                   enemy.Monster!.Id.Entry, enemy.CurrentHp, enemy.MaxHp, enemy.Block,
                   PowerKeys(enemy))).ToArray(),
               player.PlayerCombatState.Energy, player.Creature.CurrentHp, player.Creature.MaxHp,
               player.Creature.Block);

    internal static string CardStateKey(CardModel card)
        => $"{card.Id.Entry}+{card.CurrentUpgradeLevel}/" +
           $"{(card.EnergyCost.CostsX ? 1 : 0)}:{card.EnergyCost.GetWithModifiers(CostModifiers.Local)}/" +
           $"{card.Enchantment?.Id.Entry ?? "-"}:{card.Enchantment?.Amount ?? 0}/" +
           $"{card.Affliction?.Id.Entry ?? "-"}:{card.Affliction?.Amount ?? 0}";

    private static string[] PowerKeys(MegaCrit.Sts2.Core.Entities.Creatures.Creature creature)
        => creature.Powers.Select(power => $"{power.Id.Entry}:{power.Amount}").ToArray();

    private static string[] Strings(JsonElement array)
        => array.EnumerateArray().Select(item => item.GetString() ?? "").ToArray();

    public string[] DifferenceFields(JsonElement root)
    {
        List<string> differences = [];
        if (root.GetProperty("energy").GetInt32() != Energy) differences.Add("energy");
        if (root.GetProperty("hp").GetInt32() != Hp) differences.Add("player_hp");
        if (root.GetProperty("max_hp").GetInt32() != MaxHp) differences.Add("player_max_hp");
        if (root.GetProperty("player_block").GetInt32() != PlayerBlock) differences.Add("player_block");
        AddIfDifferent(differences, "hand", Hand, Strings(root.GetProperty("hand")));
        AddIfDifferent(differences, "draw", Draw, Strings(root.GetProperty("draw")));
        AddIfDifferent(differences, "discard", Discard, Strings(root.GetProperty("discard")));
        AddIfDifferent(differences, "exhaust", Exhaust, Strings(root.GetProperty("exhaust")));
        AddIfDifferent(differences, "hand_states", HandStates, Strings(root.GetProperty("hand_states")));
        AddIfDifferent(differences, "draw_states", DrawStates, Strings(root.GetProperty("draw_states")));
        AddIfDifferent(differences, "discard_states", DiscardStates, Strings(root.GetProperty("discard_states")));
        AddIfDifferent(differences, "exhaust_states", ExhaustStates, Strings(root.GetProperty("exhaust_states")));
        AddIfDifferent(differences, "relics", Relics, Strings(root.GetProperty("relics")));
        AddIfDifferent(differences, "potions", Potions, Strings(root.GetProperty("potions")));
        AddIfDifferent(differences, "player_powers", PlayerPowers, Strings(root.GetProperty("player_powers")));
        JsonElement.ArrayEnumerator enemies = root.GetProperty("enemies").EnumerateArray();
        EnemyRoot[] other = enemies.Select(enemy => new EnemyRoot(enemy.GetProperty("id").GetUInt32(),
            enemy.GetProperty("monster").GetString() ?? "", enemy.GetProperty("hp").GetInt32(),
            enemy.GetProperty("max_hp").GetInt32(), enemy.GetProperty("block").GetInt32(),
            Strings(enemy.GetProperty("powers")))).ToArray();
        if (Enemies.Length != other.Length || !Enemies.Zip(other).All(pair => pair.First.SameAs(pair.Second)))
            differences.Add("enemies");
        return differences.ToArray();
    }

    private static void AddIfDifferent(List<string> differences, string field, string[] left, string[] right)
    {
        if (!left.SequenceEqual(right)) differences.Add(field);
    }

    public static SearchAction ReadAction(JsonElement result, LiveRoot root)
    {
        int kind = result.GetProperty("action_kind").GetInt32();
        int index = result.GetProperty("action_index").GetInt32();
        int target = result.GetProperty("action_target").GetInt32();
        if (kind == 2 && index >= 0 && index < root.Hand.Length)
            return new SearchAction("PlayCard", root.Hand[index], target >= 0 ? (uint)target : null);
        return new SearchAction(kind == 3 ? "EndTurn" : $"kind={kind}:index={index}:target={target}");
    }
}

internal static class CombatSolverBridge
{
    private static readonly Type? Controller = typeof(Entry).Assembly.GetType("CombatSolver.SolverController")
        ?? Type.GetType("CombatSolver.SolverController, CombatSolver");
    private static readonly MethodInfo? SetDisabledMethod = Controller?.GetMethods(
            BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic)
        .FirstOrDefault(method => method.Name == "SetSolverDisabled"
            && method.GetParameters() is { Length: 2 } parameters
            && parameters.All(parameter => parameter.ParameterType == typeof(bool)));

    public static bool SetDisabled(bool disabled)
    {
        if (SetDisabledMethod == null)
            return false;
        try
        {
            SetDisabledMethod.Invoke(null, [disabled, false]);
            return true;
        }
        catch (Exception error)
        {
            GD.PushError($"[Sts2EnvCompare] COMBAT_SOLVER_CONTROL_FAILED disabled={disabled} error={error}");
            return false;
        }
    }
}

internal static class CombatSolverMetrics
{
    private static readonly Type? Controller = typeof(Entry).Assembly.GetType("CombatSolver.SolverController")
        ?? Type.GetType("CombatSolver.SolverController, CombatSolver");
    private static readonly PropertyInfo? CurrentResult = Controller?.GetProperty("CurrentResultForBugReport",
        BindingFlags.Static | BindingFlags.NonPublic);

    public static bool TryReadCompleted(int turn, out double elapsedMs, out long expanded, out SearchAction firstAction)
    {
        elapsedMs = 0;
        expanded = 0;
        firstAction = new SearchAction("Unknown");
        object? result = CurrentResult?.GetValue(null);
        if (result == null)
            return false;
        Type type = result.GetType();
        if ((int?)type.GetProperty("StartTurnNumber")?.GetValue(result) != turn
            || type.GetProperty("ResultScope")?.GetValue(result)?.ToString() != "SearchCompletion"
            || (bool?)type.GetProperty("WasRestoredFromCache")?.GetValue(result) == true)
            return false;
        elapsedMs = ((TimeSpan?)type.GetProperty("TotalSearchElapsed")?.GetValue(result))?.TotalMilliseconds ?? 0;
        expanded = (long?)type.GetProperty("TotalExpandedNodes")?.GetValue(result) ?? 0;
        object? plan = type.GetProperty("BestNode")?.GetValue(result);
        System.Collections.IEnumerable? actions = plan?.GetType().GetProperty("Actions")?.GetValue(plan)
                                                   as System.Collections.IEnumerable;
        if (actions != null)
        {
            foreach (object action in actions)
            {
                Type actionType = action.GetType();
                if ((int?)actionType.GetProperty("Turn")?.GetValue(action) != turn)
                    continue;
                string kind = actionType.GetProperty("Kind")?.GetValue(action)?.ToString() ?? "Unknown";
                firstAction = kind == "PlayCard"
                    ? new SearchAction(kind,
                        actionType.GetProperty("CardId")?.GetValue(action)?.ToString() ?? "",
                        (uint?)actionType.GetProperty("TargetCombatId")?.GetValue(action))
                    : new SearchAction(kind);
                break;
            }
        }
        return elapsedMs > 0;
    }

    public static bool TryReadPlan(int turn, out NativePlanAction[] plan)
    {
        plan = [];
        object? result = CurrentResult?.GetValue(null);
        if (result == null)
            return false;
        Type type = result.GetType();
        if ((int?)type.GetProperty("StartTurnNumber")?.GetValue(result) != turn
            || type.GetProperty("ResultScope")?.GetValue(result)?.ToString() != "SearchCompletion")
            return false;
        object? bestNode = type.GetProperty("BestNode")?.GetValue(result);
        System.Collections.IEnumerable? actions = bestNode?.GetType().GetProperty("Actions")?.GetValue(bestNode)
                                                   as System.Collections.IEnumerable;
        if (actions == null)
            return false;
        List<NativePlanAction> selected = [];
        foreach (object action in actions)
        {
            Type actionType = action.GetType();
            if ((int?)actionType.GetProperty("Turn")?.GetValue(action) != turn)
                continue;
            string kind = actionType.GetProperty("Kind")?.GetValue(action)?.ToString() ?? "Unknown";
            if (kind is not ("PlayCard" or "UsePotion" or "EndTurn"))
                continue;
            selected.Add(new NativePlanAction(
                kind,
                actionType.GetProperty("CardId")?.GetValue(action)?.ToString() ?? "",
                (int?)actionType.GetProperty("CardOccurrence")?.GetValue(action) ?? 0,
                (uint?)actionType.GetProperty("TargetCombatId")?.GetValue(action),
                actionType.GetProperty("PotionId")?.GetValue(action)?.ToString() ?? "",
                (int?)actionType.GetProperty("PotionSlot")?.GetValue(action) ?? -1,
                actionType.GetProperty("CardStateKey")?.GetValue(action)?.ToString() ?? "",
                (int?)actionType.GetProperty("CardStateOccurrence")?.GetValue(action) ?? 0,
                actionType.GetProperty("Choice")?.GetValue(action) != null
                    || HasItems(actionType.GetProperty("NestedChoices")?.GetValue(action))
                    || HasItems(actionType.GetProperty("TurnStartChoices")?.GetValue(action))));
        }
        plan = selected.ToArray();
        return plan.Length > 0;
    }

    private static bool HasItems(object? value)
        => value is System.Collections.IEnumerable items && items.Cast<object>().Any();
}

internal static class NativeEnv
{
    [StructLayout(LayoutKind.Sequential)]
    private struct CAction
    {
        public int Kind;
        public int Index;
        public int Target;
        public int Col;
        public int Row;
    }

    private const string LibraryName = "sts2env_c";
    private static IntPtr _library;
    private static readonly object ConfigurationGate = new();

    public static void Configure()
    {
        lock (ConfigurationGate)
        {
            if (_library != IntPtr.Zero) return;
            string directory = Path.GetDirectoryName(typeof(NativeEnv).Assembly.Location)
                               ?? throw new InvalidOperationException("Mod assembly has no location");
            IntPtr library = NativeLibrary.Load(Path.Combine(directory, "libsts2env_c.so"));
            try
            {
                NativeLibrary.SetDllImportResolver(typeof(NativeEnv).Assembly, (name, assembly, _) =>
                    name == LibraryName ? library : IntPtr.Zero);
                _library = library;
            }
            catch { NativeLibrary.Free(library); throw; }
        }
    }

    public static EnvSearchRun SolveIfRootMatches(string seed, string encounter, int ascension, int actIndex,
                                                  int nodeBudget, int beamWidth, int depth, LiveRoot live,
                                                  bool retainSession = false)
    {
        IntPtr handle = Create();
        if (handle == IntPtr.Zero)
            throw new InvalidOperationException("sts2_env_create failed");
        try
        {
            int status = InspectNamed(handle, seed, "IRONCLAD", ascension, encounter, actIndex);
            if (status == 1)
                return new EnvSearchRun(false, [], null, ReadError(handle));
            if (status != 0)
                throw new InvalidOperationException(ReadError(handle));
            string inspectJson = Marshal.PtrToStringUTF8(LastResult(handle))
                                 ?? throw new InvalidOperationException("Env returned an empty root snapshot");
            status = ValidateRoot(handle);
            if (status == 1)
                return new EnvSearchRun(false, [], null, ReadError(handle));
            if (status != 0)
                throw new InvalidOperationException(ReadError(handle));
            using JsonDocument inspect = JsonDocument.Parse(inspectJson);
            string[] mismatchFields = live.DifferenceFields(inspect.RootElement.GetProperty("root"));
            if (mismatchFields.Length > 0)
                return new EnvSearchRun(false, mismatchFields, null);

            status = SolveCurrent(handle, nodeBudget, beamWidth, depth);
            if (status == 1)
                return new EnvSearchRun(false, [], null, ReadError(handle));
            if (status != 0)
                throw new InvalidOperationException(ReadError(handle));
            string resultJson = Marshal.PtrToStringUTF8(LastResult(handle))
                                ?? throw new InvalidOperationException("Env returned an empty result");
            VerifyFuturePredictionBoundary(seed, encounter, ascension, actIndex, resultJson);
            Session? session = null;
            if (retainSession)
            {
                session = new Session(handle);
                handle = IntPtr.Zero;
            }
            return new EnvSearchRun(true, [], resultJson, session: session);
        }
        finally
        {
            Destroy(handle);
        }
    }

    /// <summary>
    /// Run the Env Beam implementation from an already assembled live root.
    /// The worker imports an owned snapshot; no seed is consulted for gameplay
    /// state. This remains a reference-policy diagnostic, and cannot authorize
    /// execution of CombatSolver's same-policy Env backend.
    /// </summary>
    public static EnvSearchRun SolveImportedSnapshot(string snapshotJson,
                                                     int nodeBudget, int beamWidth, int depth,
                                                     LiveRoot live, bool retainSession = false)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(snapshotJson);
        Session? session = null;
        try
        {
            session = ImportCombatSnapshot(snapshotJson);
            using JsonDocument inspect = JsonDocument.Parse(session.InspectJson());
            if (inspect.RootElement.GetProperty("decision").GetInt32() != 1)
                return new EnvSearchRun(false, [], null, errorReason: "live_snapshot_not_combat");
            string[] mismatchFields = live.DifferenceFields(inspect.RootElement.GetProperty("root"));
            if (mismatchFields.Length > 0)
                return new EnvSearchRun(false, mismatchFields, null);

            string resultJson = session.SolveBeamResult(nodeBudget, beamWidth, depth);
            VerifyFuturePredictionBoundary(session, resultJson);
            if (retainSession)
            {
                Session owned = session;
                session = null;
                return new EnvSearchRun(true, [], resultJson, session: owned);
            }
            return new EnvSearchRun(true, [], resultJson);
        }
        catch (EnvPredictionUnportedException error)
        {
            return new EnvSearchRun(false, [], null, error.Message);
        }
        catch (InvalidOperationException error) when (error is not ObjectDisposedException)
        {
            return new EnvSearchRun(false, [], null, errorReason: error.Message);
        }
        finally
        {
            session?.Dispose();
        }
    }

    public static EnvPlanAction[] ReadPlan(JsonElement result)
    {
        if (!result.TryGetProperty("plan", out JsonElement plan) || plan.ValueKind != JsonValueKind.Array)
        {
            return [new EnvPlanAction(result.GetProperty("action_kind").GetInt32(),
                                      result.GetProperty("action_index").GetInt32(),
                                      result.GetProperty("action_target").GetInt32(), 0, 0)];
        }
        List<EnvPlanAction> actions = [];
        foreach (JsonElement item in plan.EnumerateArray())
        {
            actions.Add(new EnvPlanAction(item.GetProperty("action_kind").GetInt32(),
                                          item.GetProperty("action_index").GetInt32(),
                                          item.GetProperty("action_target").GetInt32(),
                                          item.TryGetProperty("action_col", out JsonElement col) ? col.GetInt32() : 0,
                                          item.TryGetProperty("action_row", out JsonElement row) ? row.GetInt32() : 0));
        }
        return actions.ToArray();
    }

    /// <summary>
    /// Replays the native CombatSolver plan through Env without deploying it. This keeps the
    /// native policy as the source of the route while using Env as a fail-closed transition
    /// oracle for every action that the plan actually selected.
    /// </summary>
    public static bool VerifyNativePlan(string seed, string encounter, int ascension, int actIndex,
                                        LiveRoot live, NativePlanAction[] plan, out string reason)
        => VerifyNativePlanFrom(() => OpenCombat(seed, encounter, ascension, actIndex), live, plan, out reason);

    internal static bool VerifyNativePlanSnapshot(string snapshot, LiveRoot live,
        NativePlanAction[] plan, out string reason)
        => VerifyNativePlanFrom(() => ImportCombatSnapshot(snapshot), live, plan, out reason);

    private static bool VerifyNativePlanFrom(Func<Session> open, LiveRoot live,
        NativePlanAction[] plan, out string reason)
    {
        reason = "ok";
        if (plan.Length == 0)
        {
            reason = "empty_plan";
            return false;
        }
        Session? current = null;
        try
        {
            current = open();
            using JsonDocument root = JsonDocument.Parse(current.InspectJson());
            string[] mismatch = live.DifferenceFields(root.RootElement.GetProperty("root"));
            if (mismatch.Length > 0)
            {
                reason = $"root_mismatch:{string.Join(",", mismatch)}";
                return false;
            }
            foreach (NativePlanAction planned in plan)
            {
                using JsonDocument snapshot = JsonDocument.Parse(current.InspectJson());
                JsonElement envRoot = snapshot.RootElement.GetProperty("root");
                EnvAction action = NativePlanToEnvAction(planned, envRoot);
                ulong before = current.StateHash;
                Session next = current.Predict(action);
                if (current.StateHash != before)
                {
                    next.Dispose();
                    reason = "source_mutated";
                    return false;
                }
                current.Dispose();
                current = next;
            }
            return true;
        }
        catch (EnvPredictionUnportedException error)
        {
            reason = $"unported:{error.Message}";
            return false;
        }
        catch (Exception error)
        {
            reason = $"error:{error.Message}";
            return false;
        }
        finally
        {
            current?.Dispose();
        }
    }

    internal static EnvAction NativePlanToEnvAction(NativePlanAction planned, JsonElement root)
    {
        if (planned.HasChoices)
            throw new InvalidOperationException("native plan choices require explicit Env answer mapping");
        if (planned.Kind == "EndTurn")
            return new EnvAction(3);
        if (planned.Kind == "UsePotion")
        {
            JsonElement potions = root.GetProperty("potions");
            if (planned.PotionSlot < 0 || planned.PotionSlot >= potions.GetArrayLength()
                || potions[planned.PotionSlot].GetString() != planned.PotionId)
                throw new InvalidOperationException("native plan potion slot mismatch");
            return new EnvAction(10, planned.PotionSlot, planned.TargetCombatId is { } target ? (int)target : -1);
        }
        if (planned.Kind != "PlayCard")
            throw new InvalidOperationException($"unsupported native plan action {planned.Kind}");
        JsonElement hand = root.GetProperty("hand");
        JsonElement states = root.GetProperty("hand_states");
        string? expectedState = string.IsNullOrEmpty(planned.CardStateKey)
            ? null : NativeCardStateSummary(planned.CardStateKey);
        int selectedOccurrence = expectedState == null ? planned.CardOccurrence : planned.CardStateOccurrence;
        int occurrence = 0;
        for (int index = 0; index < hand.GetArrayLength(); index++)
        {
            if (!string.Equals(hand[index].GetString(), planned.CardId, StringComparison.Ordinal))
                continue;
            if (expectedState != null && states[index].GetString() != expectedState)
                continue;
            if (occurrence++ == selectedOccurrence)
                return new EnvAction(2, index,
                    planned.TargetCombatId is { } target ? (int)target : -1);
        }
        throw new InvalidOperationException($"native plan card missing {planned.CardId}#state{selectedOccurrence}");
    }

    // This is the same structural subset used by LiveRoot. It preserves the native
    // preference for CardStateKey/CardStateOccurrence without claiming full identity.
    private static string NativeCardStateSummary(string key)
    {
        string[] fields = key.Split('|');
        if (fields.Length < 13 || !fields[1].StartsWith("energy=", StringComparison.Ordinal))
            throw new InvalidOperationException("unsupported native card state key schema");
        string[] energy = fields[1][7..].Split(':');
        if (energy.Length != 2 || !bool.TryParse(energy[0], out bool costsX))
            throw new InvalidOperationException("invalid native card energy state");
        string[] enchantment = fields[10].Split(':');
        string enchantmentSummary = fields[10] == "-" ? "-:0"
            : enchantment.Length >= 2 ? $"{enchantment[0]}:{enchantment[1]}"
            : throw new InvalidOperationException("invalid native enchantment state");
        string affliction = fields[11].StartsWith(':') ? $"-{fields[11]}" : fields[11];
        return $"{fields[0]}/{(costsX ? 1 : 0)}:{energy[1]}/{enchantmentSummary}/{affliction}";
    }

    private static void VerifyFuturePredictionBoundary(string seed, string encounter, int ascension,
                                                       int actIndex, string resultJson)
    {
        using JsonDocument result = JsonDocument.Parse(resultJson);
        JsonElement root = result.RootElement;
        EnvAction action = new(root.GetProperty("action_kind").GetInt32(),
                               root.GetProperty("action_index").GetInt32(),
                               root.GetProperty("action_target").GetInt32());
        using Session source = OpenCombat(seed, encounter, ascension, actIndex);
        ulong before = source.StateHash;
        using Session successor = source.Predict(action);
        _ = successor.StateHash;
        if (source.StateHash != before)
            throw new InvalidOperationException("Env future prediction changed its source");
    }

    private static void VerifyFuturePredictionBoundary(Session source, string resultJson)
    {
        using JsonDocument result = JsonDocument.Parse(resultJson);
        JsonElement root = result.RootElement;
        EnvAction action = new(root.GetProperty("action_kind").GetInt32(),
                               root.GetProperty("action_index").GetInt32(),
                               root.GetProperty("action_target").GetInt32());
        ulong before = source.StateHash;
        using Session successor = source.Predict(action);
        _ = successor.StateHash;
        if (source.StateHash != before)
            throw new InvalidOperationException("Env future prediction changed its imported source");
    }

    /// <summary>
    /// Opens a persistent Env branch for future prediction. Each <see cref="Session.Predict"/>
    /// returns a new owned branch and leaves its source session unchanged.
    /// </summary>
    public static Session OpenCombat(string seed, string encounter, int ascension = 0, int actIndex = 0)
    {
        IntPtr handle = Create();
        if (handle == IntPtr.Zero)
            throw new InvalidOperationException("sts2_env_create failed");
        try
        {
            int status = InspectNamed(handle, seed, "IRONCLAD", ascension, encounter, actIndex);
            CheckStatus(handle, status);
            CheckStatus(handle, ValidateRoot(handle));
            return new Session(handle);
        }
        catch
        {
            Destroy(handle);
            throw;
        }
    }

    private static void CheckStatus(IntPtr handle, int status)
    {
        if (status == 0)
            return;
        string message = ReadError(handle);
        if (status == 1)
            throw new EnvPredictionUnportedException(message);
        throw new InvalidOperationException(message);
    }

    public static Session ImportCombatSnapshot(string json)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(json);
        byte[] payload = System.Text.Encoding.UTF8.GetBytes(json);
        if (payload.Length > 4 * 1024 * 1024) throw new InvalidOperationException("Env snapshot exceeds 4 MiB");
        IntPtr diagnostic = Create();
        if (diagnostic == IntPtr.Zero) throw new InvalidOperationException("sts2_env_create failed");
        IntPtr successor = IntPtr.Zero;
        try
        {
            CheckStatus(diagnostic, ImportSnapshot(diagnostic, payload, (nuint)payload.Length, out successor));
            if (successor == IntPtr.Zero) throw new InvalidOperationException("Env import returned no owned successor");
            Session session = new(successor);
            successor = IntPtr.Zero;
            return session;
        }
        finally
        {
            if (successor != IntPtr.Zero) Destroy(successor);
            Destroy(diagnostic);
        }
    }

    internal static string ExportBlankTemplateJson(Session source)
    {
        ArgumentNullException.ThrowIfNull(source);
        return source.ExportBlankTemplateJson();
    }

    public sealed class Session : IDisposable
    {
        private IntPtr _handle;
        // A native handle owns mutable diagnostic buffers even for pure prediction.
        // Parallel siblings must serialize access to their shared source handle.
        private readonly object _gate = new();

        internal Session(IntPtr handle) => _handle = handle;

        public string InspectJson()
        {
            lock (_gate)
            {
                IntPtr handle = RequireHandle();
                CheckStatus(handle, InspectCurrent(handle));
                return Marshal.PtrToStringUTF8(LastResult(handle))
                       ?? throw new InvalidOperationException("Env returned an empty inspection");
            }
        }

        public ulong StateHash
        {
            get
            {
                using JsonDocument document = JsonDocument.Parse(InspectJson());
                return document.RootElement.GetProperty("state_hash").GetUInt64();
            }
        }

        public string ExportSnapshotJson()
        {
            lock (_gate)
            {
                IntPtr handle = RequireHandle();
                CheckStatus(handle, ExportSnapshot(handle));
                return Marshal.PtrToStringUTF8(LastResult(handle))
                       ?? throw new InvalidOperationException("Env returned an empty snapshot");
            }
        }

        internal string ExportMonsterMachinesJson()
        {
            lock (_gate)
            {
                IntPtr handle = RequireHandle();
                CheckStatus(handle, ExportMonsterMachines(handle));
                return Marshal.PtrToStringUTF8(LastResult(handle))
                    ?? throw new InvalidOperationException("Env returned an empty monster layout");
            }
        }

        internal string ExportBlankTemplateJson()
        {
            lock (_gate)
            {
                IntPtr handle = RequireHandle();
                CheckStatus(handle, ExportBlankTemplate(handle));
                return Marshal.PtrToStringUTF8(LastResult(handle))
                    ?? throw new InvalidOperationException("Env returned an empty blank template");
            }
        }

        public Session Predict(EnvAction action)
        {
            lock (_gate)
            {
                IntPtr source = RequireHandle();
                CAction selected = new() { Kind = action.Kind, Index = action.Index, Target = action.Target,
                                           Col = action.Col, Row = action.Row };
                int status = TryTransition(source, selected, out IntPtr successor);
                if (status != 0)
                {
                    if (successor != IntPtr.Zero)
                        Destroy(successor);
                    CheckStatus(source, status);
                }
                if (successor == IntPtr.Zero)
                    throw new InvalidOperationException("Env successful prediction returned no successor");
                return new Session(successor);
            }
        }

        public Session Clone()
        {
            lock (_gate)
            {
                IntPtr source = RequireHandle();
                IntPtr copy = CloneHandle(source);
                if (copy == IntPtr.Zero)
                    throw new InvalidOperationException("Env clone failed");
                return new Session(copy);
            }
        }

        internal string SolveBeamResult(int nodeBudget, int beamWidth, int depth)
        {
            if (nodeBudget < 1 || beamWidth < 1 || depth < 1)
                throw new ArgumentOutOfRangeException(nameof(nodeBudget), "Env Beam budget must be positive");
            lock (_gate)
            {
                IntPtr handle = RequireHandle();
                CheckStatus(handle, SolveCurrent(handle, nodeBudget, beamWidth, depth));
                return Marshal.PtrToStringUTF8(LastResult(handle))
                    ?? throw new InvalidOperationException("Env returned an empty Beam result");
            }
        }

        public void Dispose()
        {
            lock (_gate)
            {
                IntPtr handle = Interlocked.Exchange(ref _handle, IntPtr.Zero);
                if (handle != IntPtr.Zero)
                    Destroy(handle);
            }
            GC.SuppressFinalize(this);
        }

        ~Session() => Dispose();

        private IntPtr RequireHandle()
            => _handle == IntPtr.Zero
                ? throw new ObjectDisposedException(nameof(Session))
                : _handle;
    }

    private static string ReadError(IntPtr handle)
        => Marshal.PtrToStringUTF8(LastError(handle)) ?? "Env operation failed";

    [DllImport(LibraryName, EntryPoint = "sts2_env_create", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr Create();
    [DllImport(LibraryName, EntryPoint = "sts2_env_clone", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr CloneHandle(IntPtr handle);
    [DllImport(LibraryName, EntryPoint = "sts2_env_destroy", CallingConvention = CallingConvention.Cdecl)]
    private static extern void Destroy(IntPtr handle);
    [DllImport(LibraryName, EntryPoint = "sts2_env_last_error", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr LastError(IntPtr handle);
    [DllImport(LibraryName, EntryPoint = "sts2_env_last_result", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr LastResult(IntPtr handle);
    [DllImport(LibraryName, EntryPoint = "sts2_env_inspect_combat_named", CallingConvention = CallingConvention.Cdecl)]
    private static extern int InspectNamed(IntPtr handle,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string seed,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string character,
        int ascension,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string encounter,
        int actIndex);
    [DllImport(LibraryName, EntryPoint = "sts2_env_validate_combat_root", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ValidateRoot(IntPtr handle);
    [DllImport(LibraryName, EntryPoint = "sts2_env_inspect_current", CallingConvention = CallingConvention.Cdecl)]
    private static extern int InspectCurrent(IntPtr handle);
    [DllImport(LibraryName, EntryPoint = "sts2_env_export_snapshot", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ExportSnapshot(IntPtr handle);
    [DllImport(LibraryName, EntryPoint = "sts2_env_export_blank_template", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ExportBlankTemplate(IntPtr handle);
    [DllImport(LibraryName, EntryPoint = "sts2_env_export_monster_machines", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ExportMonsterMachines(IntPtr handle);
    [DllImport(LibraryName, EntryPoint = "sts2_env_import_snapshot", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ImportSnapshot(IntPtr diagnostic, [In] byte[] payload, nuint length, out IntPtr successor);
    [DllImport(LibraryName, EntryPoint = "sts2_env_try_transition", CallingConvention = CallingConvention.Cdecl)]
    private static extern int TryTransition(IntPtr source, CAction action, out IntPtr successor);
    [DllImport(LibraryName, EntryPoint = "sts2_env_solve_beam", CallingConvention = CallingConvention.Cdecl)]
    private static extern int SolveCurrent(IntPtr handle, int nodeBudget, int beamWidth, int depth);
}
