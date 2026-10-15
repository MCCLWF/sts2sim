#if NOSL_PANEL_TEST
// Opt-in integration driver compiled only by panel_smoke.sh, never shipped.
// Uses the real menu and game action queues in an isolated save profile.
using System.Diagnostics;
using System.Text.Json;
using Godot;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Nodes;
using MegaCrit.Sts2.Core.Nodes.CommonUi;
using MegaCrit.Sts2.Core.Nodes.GodotExtensions;
using MegaCrit.Sts2.Core.Nodes.Rooms;
using MegaCrit.Sts2.Core.Nodes.Rewards;
using MegaCrit.Sts2.Core.Nodes.Screens.CardSelection;
using MegaCrit.Sts2.Core.Nodes.Screens.Capstones;
using MegaCrit.Sts2.Core.Nodes.Screens.CharacterSelect;
using MegaCrit.Sts2.Core.Nodes.Screens.MainMenu;
using MegaCrit.Sts2.Core.Nodes.Screens.Overlays;
using MegaCrit.Sts2.Core.Nodes.Screens;
using MegaCrit.Sts2.Core.Nodes.TopBar;
using MegaCrit.Sts2.Core.Rewards;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Saves;

namespace NoslPilot;

public static partial class Entry
{
    private static async Task SmokeWait(Func<bool> predicate, string message, double seconds = 90)
    {
        Stopwatch timer = Stopwatch.StartNew();
        while (!predicate())
        {
            if (timer.Elapsed.TotalSeconds > seconds) throw new Exception("Timed out: " + message + " / " + GetPanelSnapshot());
            await _host.ToSignal(_host.GetTree(), SceneTree.SignalName.ProcessFrame);
        }
    }

    private static async Task SmokeDelay(double seconds)
    {
        Stopwatch timer = Stopwatch.StartNew();
        await SmokeWait(() => timer.Elapsed.TotalSeconds >= seconds, "delay", seconds + 5);
    }

    private static void SmokeKey(Key key)
    {
        Input.ParseInputEvent(new InputEventKey { Keycode = key, Pressed = true });
        Input.ParseInputEvent(new InputEventKey { Keycode = key, Pressed = false });
    }

    private static async Task SmokeNewRun(string character)
    {
        await SmokeWait(() => _host.MainMenu != null, "main menu");
        await SmokeDelay(2);
        _host.MainMenu!.GetNode<NMainMenuTextButton>("MainMenuTextButtons/SingleplayerButton").ForceClick();
        await SmokeDelay(1);
        if (_host.MainMenu.SubmenuStack.Peek() is NSingleplayerSubmenu single)
            single.GetNode<NSubmenuButton>("StandardButton").ForceClick();
        await SmokeWait(() => _host.MainMenu.SubmenuStack.Peek() is NCharacterSelectScreen, "character selection");
        NCharacterSelectScreen screen = (NCharacterSelectScreen)_host.MainMenu.SubmenuStack.Peek()!;
        NCharacterSelectButton button = screen.GetNode<Control>("CharSelectButtons/ButtonContainer")
            .GetChildren().OfType<NCharacterSelectButton>().Single(b => b.Character.Id.Entry == character);
        if (button.IsLocked) throw new Exception("Test profile has locked " + character);
        button.Select(); // Character buttons select on press, unlike menu buttons' release.
        await SmokeDelay(2);
        _host.DebugSeedOverride = "NR5EVAL000067";
        screen.GetNode<NConfirmButton>("ConfirmButton").ForceClick();
        await SmokeWait(() => VisibleRun()?.CurrentRoom != null, "normal new run");
    }

    private static async Task SmokeShot(string directory, string name)
    {
        await SmokeDelay(0.5);
        using Image shot = _host.GetViewport().GetTexture().GetImage();
        Error result = shot.SavePng(Path.Combine(directory, name + ".png"));
        if (result != Error.Ok) throw new Exception("Screenshot: " + result);
    }

    private static async Task PanelSmokeAsync()
    {
        string directory = Env("NOSL_PANEL_TEST_OUT", OS.GetUserDataDir());
        Directory.CreateDirectory(directory);
        List<string> checks = new();
        void Check(bool pass, string name)
        {
            if (!pass) throw new Exception("Failed: " + name);
            checks.Add(name);
            GD.Print("[PanelSmoke] PASS " + name);
        }
        try
        {
            await SmokeDelay(3);
            Check(_panel != null && !_sessionActive, "panel on normal startup, no run created");
            PanelContainer box = _panel!.GetNode<PanelContainer>("Panel");
            Control header = box.GetNode<Control>("Layout/Header");
            Button fold = header.GetChildren().OfType<Button>().Last();
            if (Env("NOSL_PANEL_TEST_MODE", "full") == "restore")
            {
                Check(Preferences.Collapsed && Preferences.X > 100, "position and collapse restored after process restart");
                await SmokeShot(directory, "restored");
            }
            else
            {
                await SmokeShot(directory, "menu");
                Vector2 before = box.Position;
                Vector2 start = header.GlobalPosition + new Vector2(60, 14);
                _host.GetViewport().PushInput(new InputEventMouseMotion { Position = start, GlobalPosition = start }, true);
                await SmokeDelay(0.1);
                _host.GetViewport().PushInput(new InputEventMouseButton { Position = start, GlobalPosition = start, ButtonIndex = MouseButton.Left, Pressed = true }, true);
                await SmokeDelay(0.1);
                Vector2 end = start + new Vector2(160, 50);
                _host.GetViewport().PushInput(new InputEventMouseMotion { Position = end, GlobalPosition = end, Relative = end - start, ButtonMask = MouseButtonMask.Left }, true);
                await SmokeDelay(0.2);
                _host.GetViewport().PushInput(new InputEventMouseButton { Position = end, GlobalPosition = end, ButtonIndex = MouseButton.Left, Pressed = false }, true);
                Check(box.Position.DistanceTo(before) > 100, "header mouse drag moves panel");
                float expanded = box.Size.Y;
                fold.EmitSignal(Button.SignalName.Pressed);
                await SmokeDelay(0.3);
                Check(Preferences.Collapsed && box.Size.Y < expanded / 2, "collapse shrinks panel");
                await SmokeShot(directory, "collapsed");
                fold.EmitSignal(Button.SignalName.Pressed);
                await SmokeNewRun("SILENT");
                if (Env("NOSL_PANEL_TEST_MODE", "full") == "missing-model")
                {
                    await SmokeWait(() => _panelStatus == "运行出错" && !_sessionActive, "missing model error");
                    Check(_taken == 0 && _interactiveSelector == null && _actor == null, "missing model releases control without actions");
                    await SmokeShot(directory, "missing-model");
                }
                else
                {
                    await SmokeWait(() => _taken >= 12 && IsReady("Combat", false), "automatic live inference");
                    SmokeKey(Key.F8);
                    await SmokeWait(() => _atPause, "F8 pauses at action boundary");
                    int count = _taken;
                    await SmokeDelay(1);
                    Check(_taken == count && _interactiveSelector == null, "paused has no actions or selector");
                    await SmokeShot(directory, "paused");
                    int round = CombatManager.Instance.DebugOnlyGetState()!.RoundNumber;
                    NCombatRoom.Instance!.Ui.EndTurnButton.ForceClick();
                    await SmokeWait(() => CombatManager.Instance.DebugOnlyGetState()!.RoundNumber > round && IsReady("Combat", false), "manual turn while paused");
                    Check(_taken == count, "manual game input works while paused");
                    SmokeKey(Key.F8);
                    await SmokeWait(() => _taken > count + 4, "resume uses current observation");
                    UiHelperFindAll<NTopBarPauseButton>(_host).Single().ForceClick();
                    await SmokeWait(() => NCapstoneContainer.Instance!.InUse, "open game pause menu");
                    await SmokeDelay(1);
                    count = _taken;
                    await SmokeDelay(1);
                    Check(_taken == count && _sessionActive, "game menu suspends automatic decisions");
                    NCapstoneContainer.Instance!.Close();
                    await SmokeWait(() => _taken > count, "closing game menu resumes");
                    await SmokeShot(directory, "running");
                    SmokeKey(Key.F9);
                    await SmokeWait(() => !_sessionActive, "F9 hands over");
                    count = _taken;
                    await SmokeDelay(1);
                    Check(_taken == count && _actor == null && _interactiveSelector == null, "handover releases native actor and prevents auto-restart");
                    StartFromPanel();
                    await SmokeWait(() => _sessionActive && _taken >= 3, "restart after handover");
                    await SmokeWait(() => NOverlayStack.Instance?.Peek() is NRewardsScreen rewards &&
                        UiHelperFindAll<NRewardButton>(rewards).Any(b => b.Reward is CardReward && b.IsEnabled), "card reward for manual handover");
                    TogglePause();
                    await SmokeWait(() => _atPause, "pause at rewards");
                    NRewardsScreen rewardsScreen = (NRewardsScreen)NOverlayStack.Instance!.Peek()!;
                    UiHelperFindAll<NRewardButton>(rewardsScreen).First(b => b.Reward is CardReward && b.IsEnabled).ForceClick();
                    await SmokeWait(() => NOverlayStack.Instance?.Peek() is NCardRewardSelectionScreen, "human opens card reward");
                    HandOver();
                    await SmokeWait(() => !_sessionActive, "handover at card reward");
                    await SmokeDelay(0.6);
                    StartFromPanel();
                    await SmokeWait(() => _taken > 0 && NOverlayStack.Instance?.Peek() is not NCardRewardSelectionScreen,
                        "reattach to human-opened card reward");
                    Check(_sessionActive && _fatal == null, "restart reads human-opened card reward");
                    TogglePause();
                    await SmokeWait(() => _atPause, "pause before normal menu exit");
                    await _host.ReturnToMainMenu();
                    await SmokeWait(() => !_sessionActive, "run exit cancels driver");
                    Check(_actor == null && _interactiveSelector == null, "run exit cleans native resources");
                    SaveManager.Instance.DeleteCurrentRun(); // isolated test profile only
                    await SmokeNewRun("SILENT");
                    await SmokeWait(() => _sessionActive && _taken >= 3, "second normal run automatically attaches");
                    Check(true, "new run auto-attaches");
                    HandOver();
                    await SmokeWait(() => !_sessionActive, "second run handover");
                    await _host.ReturnToMainMenu();
                    SaveManager.Instance.DeleteCurrentRun();
                    await SmokeNewRun("IRONCLAD");
                    await SmokeDelay(2);
                    Check(!_sessionActive && !GetPanelSnapshot().Eligible, "non-Silent run is untouched");
                    await _host.ReturnToMainMenu();
                    SaveManager.Instance.DeleteCurrentRun();
                }
                if (!Preferences.Collapsed) fold.EmitSignal(Button.SignalName.Pressed);
            }
            File.WriteAllText(Path.Combine(directory, "result.json"), JsonSerializer.Serialize(new { passed = true, checks, snapshot = GetPanelSnapshot() }, new JsonSerializerOptions { WriteIndented = true }));
            _host.GetTree().Quit(0);
        }
        catch (Exception ex)
        {
            GD.PrintErr("[PanelSmoke] " + ex);
            File.WriteAllText(Path.Combine(directory, "result.json"), JsonSerializer.Serialize(new { passed = false, checks, error = ex.ToString(), snapshot = GetPanelSnapshot() }, new JsonSerializerOptions { WriteIndented = true }));
            _host.GetTree().Quit(1);
        }
    }
}
#endif
