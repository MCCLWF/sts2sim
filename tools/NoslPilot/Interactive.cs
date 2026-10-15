using System.Diagnostics;
using System.Text.Json;
using System.Text.Json.Nodes;
using Godot;
using HarmonyLib;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Events.Custom.CrystalSphereEvent;
using MegaCrit.Sts2.Core.Nodes;
using MegaCrit.Sts2.Core.Nodes.Screens.Capstones;
using MegaCrit.Sts2.Core.Multiplayer.Game;
using MegaCrit.Sts2.Core.Runs;

namespace NoslPilot;

public static partial class Entry
{
    private static bool _interactive, _sessionActive, _pauseRequested, _stopRequested, _atPause;
    private static IDisposable? _interactiveSelector;
    private static RunState? _seenRun, _suppressedRun;
    private static bool _hooksInstalled;
    private static PilotPanel? _panel;
    private static string _panelStatus = "等待猎手对局", _panelDetail = "进入猎手单人对局后自动接管。";
    private static string _lastAction = "尚无动作";
    private static double _lastMs;
    private static int _lastRows;
    private static string _lastDecision = "等待决策", _lastRoute = "等待模型加载";
    private static double _nextDecisionAt;
    internal static PanelPreferences Preferences { get; private set; } = new();
    internal static string PreferencesPath => Path.Combine(OS.GetUserDataDir(), "nosl-pilot-panel.json");
    internal static string ModelPath => Path.Combine(NativeRuntime.Directory, "model", "policy.bundle");

    private static async Task InitializeInteractiveAsync()
    {
        try
        {
            _host = NGame.Instance ?? throw new InvalidOperationException("游戏尚未初始化");
            await _host.GameStartupComplete;
            _interactive = true;
            try
            {
                if (File.Exists(PreferencesPath)) Preferences = JsonSerializer.Deserialize<PanelPreferences>(File.ReadAllText(PreferencesPath)) ?? new();
            }
            catch (Exception ex) { GD.PrintErr("[NoslPilot] panel preferences: " + ex.Message); }
            Preferences.Delay = Math.Clamp(double.IsFinite(Preferences.Delay) ? Preferences.Delay : 0.15, 0, 1);
            if (!float.IsFinite(Preferences.X)) Preferences.X = 24;
            if (!float.IsFinite(Preferences.Y)) Preferences.Y = 96;
            InstallChoiceHooks();
            _panel = new PilotPanel();
            _host.AddChild(_panel);
            GD.Print("[NoslPilot] R5 panel ready; native CPU inference; auto=" + Preferences.AutoStart);
#if NOSL_PANEL_TEST
            if (Env("NOSL_PANEL_TEST", "0") == "1") _ = PanelSmokeAsync();
#endif
        }
        catch (Exception ex) { GD.PrintErr("[NoslPilot] initialization failed: " + ex); }
    }

    private static void InstallChoiceHooks()
    {
        if (_hooksInstalled) return;
        Harmony harmony = new("sts2sim.nosl-pilot");
        harmony.Patch(AccessTools.Method(typeof(CardSelectCmd), nameof(CardSelectCmd.FromChooseABundleScreen)),
            prefix: new HarmonyMethod(typeof(Entry), nameof(ChooseBundle)));
        harmony.Patch(AccessTools.Method(typeof(CrystalSphereMinigame), nameof(CrystalSphereMinigame.PlayMinigame)),
            prefix: new HarmonyMethod(typeof(Entry), nameof(PlayCrystalSphere)));
        _hooksInstalled = true;
    }

    internal static void SavePanelPreferences()
    {
        try { File.WriteAllText(PreferencesPath, JsonSerializer.Serialize(Preferences, new JsonSerializerOptions { WriteIndented = true })); }
        catch (Exception ex) { GD.PrintErr("[NoslPilot] cannot save preferences: " + ex.Message); }
    }

    private static RunState? VisibleRun() => NRun.Instance != null && !RunManager.Instance.IsCleaningUp
        ? RunManager.Instance.DebugOnlyGetState() : null;

    private static bool Eligible(RunState? run) => run != null && run.Players.Count == 1
        && RunManager.Instance.NetService.Type == NetGameType.Singleplayer
        && run.Players[0].Character.Id.Entry == "SILENT" && !run.IsGameOver;

    private static bool CurrentSessionIsValid() => ReferenceEquals(VisibleRun(), _run);

    internal static void TickPanel()
    {
        RunState? current = VisibleRun();
        if (!ReferenceEquals(current, _seenRun))
        {
            _seenRun = current;
            _suppressedRun = null;
            if (!_sessionActive)
            {
                _panelStatus = current == null ? "等待猎手对局" : Eligible(current) ? "准备接管" : "当前对局不支持";
                _panelDetail = current == null ? "进入猎手单人对局后自动接管。" : "支持静默猎手单人对局；R5 以 A10 训练。";
            }
        }
        if (!_sessionActive && Preferences.AutoStart && Eligible(current) && current!.CurrentRoom != null
            && !ReferenceEquals(current, _suppressedRun) && NCapstoneContainer.Instance?.InUse != true)
            StartFromPanel();
    }

    internal static void StartFromPanel()
    {
        if (_sessionActive)
        {
            _pauseRequested = false;
            return;
        }
        RunState? current = VisibleRun();
        if (!Eligible(current))
        {
            _panelStatus = "等待猎手对局";
            _panelDetail = "请进入静默猎手单人对局后开始。";
            return;
        }
        if (RunManager.Instance.ActionExecutor.IsRunning)
        {
            _panelStatus = "等待当前动作";
            _panelDetail = "请先完成当前选牌或动作，再开始接管。";
            return;
        }
        _sessionActive = true;  // set before starting an async driver; never start two drivers
        _ = RunInteractiveAsync(current!);
    }

    internal static void TogglePause()
    {
        if (!_sessionActive) { StartFromPanel(); return; }
        if (_stopRequested) return;
        _pauseRequested = !_pauseRequested;
        _panelStatus = _pauseRequested ? "正在暂停…" : "继续运行…";
        _panelDetail = _pauseRequested ? "当前动作及其选牌完成后暂停。" : "读取当前游戏状态并继续。";
    }

    internal static void HandOver()
    {
        _suppressedRun = VisibleRun();
        if (!_sessionActive)
        {
            _panelStatus = "手动模式";
            _panelDetail = "本局已交还；点击开始可再次接管。";
            return;
        }
        _stopRequested = true;
        _panelStatus = "正在交还…";
        _panelDetail = "完成当前动作后交还操作。";
    }

    private static async Task RunInteractiveAsync(RunState run)
    {
        string outcome = "stopped", reason = "";
        _run = run;
        _player = run.Players[0];
        _direct = true;
        _pauseRequested = _stopRequested = _atPause = _handedOver = false;
        _taken = 0;
        _fatal = null;
        _pendingPurchase = null;
        _activeCardReward = null;
        _chestOpened = _treasureResolved = false;
        _chestRoom = null;
        _lastAction = "尚无动作";
        _lastMs = 0;
        _lastRows = 0;
        _lastDecision = "等待决策";
        _lastRoute = "R5 · CPU 原生推理";
        _nextDecisionAt = 0;
        NextMs.Clear();
        Clock.Restart();
        try
        {
            _log = new StreamWriter(Path.Combine(OS.GetUserDataDir(), "nosl-pilot.jsonl"), append: true) { AutoFlush = true };
            _actor = new NativeActor(ModelPath, 1, 1, true);
            _panelStatus = "自动运行";
            _panelDetail = "R5 已加载 · 直接读取游戏公开状态";
            Emit(new JsonObject { ["event"] = "interactive_begin", ["model"] = "R5", ["mode"] = "direct", ["weights"] = ModelPath,
                ["floor"] = run.TotalFloor, ["ascension"] = run.AscensionLevel });
            outcome = await PlayDirectAsync();
            _panelStatus = outcome == "victory" ? "对局胜利" : "对局结束";
            _panelDetail = outcome == "victory" ? "游戏已确认胜利。" : "游戏已确认死亡；可以正常开始下一局。";
        }
        catch (PilotStop stop)
        {
            reason = stop.Message;
            _panelStatus = _stopRequested ? "手动模式" : CurrentSessionIsValid() ? "已停止" : "等待猎手对局";
            _panelDetail = _stopRequested ? "本局已交还；点击开始可再次接管。" : reason;
        }
        catch (Exception ex)
        {
            reason = ex.ToString();
            _panelStatus = "运行出错";
            _panelDetail = ex.Message;
            GD.PrintErr("[NoslPilot] " + ex);
        }
        finally
        {
            ReleaseInteractiveSelector();
            _actor?.Dispose();
            _actor = null;
            _suppressedRun = run;  // no automatic retries after an error, death or manual hand-over
            Emit(new JsonObject { ["event"] = "interactive_end", ["outcome"] = outcome, ["reason"] = reason,
                ["actions"] = _taken, ["floor"] = run.TotalFloor, ["wall_s"] = Clock.Elapsed.TotalSeconds });
            _log?.Dispose();
            _log = null;
            _sessionActive = false;
            _atPause = false;
            _pauseRequested = _stopRequested = false;
            GD.Print($"[NoslPilot] interactive end: {outcome}, {reason}");
        }
    }

    private static void ReleaseInteractiveSelector()
    {
        _interactiveSelector?.Dispose();
        _interactiveSelector = null;
    }

    private static async Task<bool> HoldInteractiveAsync()
    {
        bool waited = false;
        // Shop actions may request a nested selection after Execute returns. Finish that action before pausing.
        if (_pendingPurchase != null)
        {
            while (!_pendingPurchase.IsCompleted) await Frame();
            await _pendingPurchase;
            _pendingPurchase = null;
        }
        while (true)
        {
            if (!CurrentSessionIsValid()) throw new PilotStop("对局已关闭或切换");
            if (_stopRequested) throw new PilotStop("已交还手动操作");
            bool menu = NCapstoneContainer.Instance?.InUse == true;
            if (!_pauseRequested && !menu)
            {
                // A human may still be choosing cards from an action played while paused.
                if (_atPause && RunManager.Instance.ActionExecutor.IsRunning)
                {
                    _panelStatus = "等待手动动作完成";
                    await Frame();
                    continue;
                }
                if (_interactiveSelector == null) _interactiveSelector = CardSelectCmd.UseSelector(new Selector(), localOnly: true);
                if (_atPause)
                {
                    _panelStatus = "自动运行";
                    _panelDetail = "R5 已加载 · 直接读取游戏公开状态";
                }
                _atPause = false;
                if (Clock.Elapsed.TotalSeconds >= _nextDecisionAt)
                {
                    _nextDecisionAt = Clock.Elapsed.TotalSeconds + Preferences.Delay;
                    return waited;
                }
                await Frame();
                continue;
            }
            ReleaseInteractiveSelector();
            _atPause = true;
            _panelStatus = menu ? "游戏菜单已打开" : "已暂停";
            _panelDetail = menu ? "关闭菜单后继续；也可在面板暂停。" : "现在可以手动操作；点击继续或按 F8 接管。";
            waited = true;
            await Frame();
        }
    }

    private static void RecordPanelAction(JsonObject row)
    {
        if ((string?)row["event"] != "action" || row["action"] is not JsonObject action) return;
        _lastMs = (double?)row["ms"] ?? 0;
        _lastRows = (int?)row["rows"] ?? 0;
        string decision = (string?)row["decision"] ?? "";
        _lastDecision = decision switch
        {
            "Combat" => "战斗", "Map" => "选路", "Choice" => "选牌 / 嵌套选择", "Rewards" => "奖励",
            "Shop" => "商店", "RestSite" => "休息点", "Treasure" => "宝箱", "Event" => "事件", _ => decision,
        };
        _lastRoute = _lastRows <= 1 ? "筛选后唯一动作 · 无需网络推理"
            : MegaCrit.Sts2.Core.Combat.CombatManager.Instance.IsInProgress ? "战斗策略网络" : "局外价值网络 + 构筑规则";
        string kind = (string?)action["kind"] ?? "";
        int index = (int?)action["index"] ?? 0;
        _lastAction = kind switch
        {
            "PlayCard" => index < _player.PlayerCombatState!.Hand.Cards.Count
                ? "出牌 · " + _player.PlayerCombatState.Hand.Cards[index].Title : "出牌",
            "EndTurn" => "结束回合", "MapMove" => "选择路线", "TakeReward" => "领取奖励",
            "Proceed" => "继续前进", "EventOption" => "事件选项 " + (index + 1),
            "RestOption" => "休息点选项 " + (index + 1), "ShopBuy" => "商店购买",
            "TreasurePick" => "领取遗物", "UsePotion" => "使用药水", "DiscardPotion" => "丢弃药水",
            "Answer" => "完成选择", _ => kind,
        };
    }

    internal static PanelSnapshot GetPanelSnapshot()
    {
        RunState? run = VisibleRun();
        bool menu = _sessionActive && NCapstoneContainer.Instance?.InUse == true;
        return new(menu ? "游戏菜单已打开" : _panelStatus, menu ? "关闭菜单后继续；也可在面板暂停。" : _panelDetail,
            _lastDecision, _lastRoute, _lastAction, _lastMs, _lastRows,
            NextMs.Count == 0 ? 0 : NextMs.Average(), NextMs.Count, _taken,
            _sessionActive, _pauseRequested, _atPause, Eligible(run), _stopRequested);
    }
}

internal sealed class PanelPreferences
{
    public bool AutoStart { get; set; } = true;
    public bool Collapsed { get; set; }
    public float X { get; set; } = 24;
    public float Y { get; set; } = 96;
    public double Delay { get; set; } = 0.15;
}

internal sealed record PanelSnapshot(string Status, string Detail, string Decision, string Route, string Action, double Ms, int Rows,
    double AverageMs, int Inferences, int Count,
    bool Active, bool PauseRequested, bool Paused, bool Eligible, bool Stopping);
