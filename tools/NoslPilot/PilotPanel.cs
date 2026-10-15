using Godot;

namespace NoslPilot;

// Native Godot controls: no web view, Python process or external UI assets.
internal sealed partial class PilotPanel : CanvasLayer
{
    private PanelContainer _box = null!;
    private VBoxContainer _body = null!;
    private Label _status = null!, _detail = null!, _decision = null!, _route = null!, _action = null!, _timing = null!, _counts = null!;
    private Button _toggle = null!, _compactToggle = null!, _handOver = null!, _fold = null!;
    private Control _header = null!;
    private bool _dragging;
    private double _refresh;
    private static readonly Color Text = new("e9eee9"), Muted = new("9aa9a3"), Accent = new("92dab2");

    public override void _Ready()
    {
        Name = "NoslPilotPanel";
        Layer = 110;
        ProcessMode = ProcessModeEnum.Always;
        _box = new PanelContainer { Name = "Panel", Position = new Vector2(Entry.Preferences.X, Entry.Preferences.Y),
            CustomMinimumSize = new Vector2(350, 0), MouseFilter = Control.MouseFilterEnum.Stop };
        _box.AddThemeStyleboxOverride("panel", new StyleBoxFlat
        {
            BgColor = new Color("151e1cf5"), BorderColor = new Color("435f50"),
            BorderWidthLeft = 1, BorderWidthRight = 1, BorderWidthTop = 1, BorderWidthBottom = 1,
            CornerRadiusTopLeft = 12, CornerRadiusTopRight = 12, CornerRadiusBottomLeft = 12, CornerRadiusBottomRight = 12,
            ContentMarginLeft = 16, ContentMarginRight = 16, ContentMarginTop = 12, ContentMarginBottom = 14,
            ShadowColor = new Color(0, 0, 0, 0.35f), ShadowSize = 8,
        });
        AddChild(_box);
        VBoxContainer root = new() { Name = "Layout" };
        root.AddThemeConstantOverride("separation", 10);
        _box.AddChild(root);
        HBoxContainer header = new() { Name = "Header", MouseFilter = Control.MouseFilterEnum.Stop, MouseDefaultCursorShape = Control.CursorShape.Move };
        _header = header;
        root.AddChild(header);
        Label title = MakeLabel("R5  猎手助手", 18, Accent);
        title.SizeFlagsHorizontal = Control.SizeFlags.ExpandFill;
        header.AddChild(title);
        _compactToggle = MakeButton("▶", () => Entry.TogglePause());
        _compactToggle.TooltipText = "开始 / 暂停 / 继续（F8）";
        header.AddChild(_compactToggle);
        _fold = MakeButton("−", ToggleFold);
        _fold.TooltipText = "收起 / 展开";
        header.AddChild(_fold);
        header.GuiInput += HeaderInput;

        _body = new VBoxContainer { Name = "Body" };
        _body.AddThemeConstantOverride("separation", 9);
        root.AddChild(_body);
        _status = AddLabel(_body, "等待猎手对局", 20, Text);
        AddLabel(_body, "R5  ·  CPU 原生推理  ·  离线", 13, Muted);
        _detail = AddLabel(_body, "进入猎手单人对局后自动接管。", 13, Muted, true);
        _detail.CustomMinimumSize = new Vector2(310, 36);
        _detail.MaxLinesVisible = 2;
        _detail.TextOverrunBehavior = TextServer.OverrunBehavior.TrimEllipsis;
        _body.AddChild(new HSeparator());
        _decision = AddLabel(_body, "当前决策：等待", 14, Text);
        _route = AddLabel(_body, "等待模型加载", 13, Muted, true);
        _route.CustomMinimumSize = new Vector2(310, 18);
        _action = AddLabel(_body, "尚无动作", 16, Accent, true);
        _timing = AddLabel(_body, "最近 — ms    平均 — ms", 14, Text);
        _counts = AddLabel(_body, "候选动作 —    推理 0 次", 13, Muted);
        _body.AddChild(new HSeparator());
        HBoxContainer buttons = new();
        buttons.AddThemeConstantOverride("separation", 8);
        _body.AddChild(buttons);
        _toggle = MakeButton("开始  F8", () => Entry.TogglePause(), true);
        _toggle.SizeFlagsHorizontal = Control.SizeFlags.ExpandFill;
        buttons.AddChild(_toggle);
        _handOver = MakeButton("交还手动  F9", () => Entry.HandOver());
        _handOver.SizeFlagsHorizontal = Control.SizeFlags.ExpandFill;
        buttons.AddChild(_handOver);
        CheckButton auto = new() { Name = "AutoStart", Text = "进入猎手对局后自动接管", ButtonPressed = Entry.Preferences.AutoStart,
            FocusMode = Control.FocusModeEnum.None };
        auto.AddThemeFontSizeOverride("font_size", 14);
        auto.AddThemeColorOverride("font_color", Text);
        auto.Toggled += value => { Entry.Preferences.AutoStart = value; Entry.SavePanelPreferences(); };
        _body.AddChild(auto);
        HBoxContainer pace = new();
        _body.AddChild(pace);
        Label paceLabel = AddLabel(pace, $"动作间隔 {Entry.Preferences.Delay:0.00}s", 13, Muted);
        HSlider delay = new() { Name = "DecisionDelay", MinValue = 0, MaxValue = 1, Step = 0.05,
            Value = Entry.Preferences.Delay, SizeFlagsHorizontal = Control.SizeFlags.ExpandFill, CustomMinimumSize = new Vector2(110, 24),
            FocusMode = Control.FocusModeEnum.None };
        pace.AddChild(delay);
        delay.ValueChanged += value => { Entry.Preferences.Delay = value; paceLabel.Text = $"动作间隔 {value:0.00}s"; Entry.SavePanelPreferences(); };
        AddLabel(_body, "拖动标题栏移动 · 收起后仍可控制", 12, Muted);
        ApplyFold();
        ClampPosition();
    }

    private static Label MakeLabel(string text, int size, Color color, bool wrap = false)
    {
        Label label = new() { Text = text, MouseFilter = Control.MouseFilterEnum.Ignore };
        label.AddThemeFontSizeOverride("font_size", size);
        label.AddThemeColorOverride("font_color", color);
        if (wrap)
        {
            label.AutowrapMode = TextServer.AutowrapMode.WordSmart;
            label.CustomMinimumSize = new Vector2(310, 0);
        }
        return label;
    }

    private static Label AddLabel(Node parent, string text, int size, Color color, bool wrap = false)
    {
        Label label = MakeLabel(text, size, color, wrap);
        parent.AddChild(label);
        return label;
    }

    private static Button MakeButton(string text, Action pressed, bool primary = false)
    {
        Button button = new() { Text = text, FocusMode = Control.FocusModeEnum.None, MouseDefaultCursorShape = Control.CursorShape.PointingHand,
            CustomMinimumSize = new Vector2(30, 32) };
        button.AddThemeFontSizeOverride("font_size", 14);
        button.AddThemeColorOverride("font_color", Text);
        button.AddThemeColorOverride("font_hover_color", Colors.White);
        button.AddThemeColorOverride("font_pressed_color", Colors.White);
        foreach (string state in new[] { "normal", "hover", "pressed", "disabled" })
        {
            Color bg = state == "disabled" ? new("202724") : primary ? new("315e47") : new("293c33");
            if (state == "hover") bg = bg.Lightened(0.14f);
            button.AddThemeStyleboxOverride(state, new StyleBoxFlat { BgColor = bg,
                CornerRadiusTopLeft = 6, CornerRadiusTopRight = 6, CornerRadiusBottomLeft = 6, CornerRadiusBottomRight = 6,
                ContentMarginLeft = 9, ContentMarginRight = 9, ContentMarginTop = 5, ContentMarginBottom = 5 });
        }
        button.Pressed += pressed;
        return button;
    }

    public override void _Process(double delta)
    {
        _refresh += delta;
        if (_refresh < 0.15) return;
        _refresh = 0;
        Entry.TickPanel();
        PanelSnapshot state = Entry.GetPanelSnapshot();
        _status.Text = state.Status;
        _status.Modulate = state.Status == "运行出错" || state.Status == "已停止" ? new Color("ffb096") : Colors.White;
        _detail.Text = state.Detail;
        _detail.TooltipText = state.Detail;
        _decision.Text = "当前决策：" + state.Decision;
        _route.Text = state.Route;
        _action.Text = state.Action;
        _timing.Text = $"最近 {state.Ms:0.0} ms    平均 {state.AverageMs:0.0} ms";
        _counts.Text = $"筛后候选 {state.Rows}    推理 {state.Inferences} 次    动作 {state.Count}";
        _toggle.Text = !state.Active ? "开始  F8" : state.PauseRequested ? "继续  F8" : "暂停  F8";
        _compactToggle.Text = state.Active && !state.PauseRequested ? "Ⅱ" : "▶";
        _compactToggle.TooltipText = state.Status + " · " + _toggle.Text;
        _toggle.Disabled = _compactToggle.Disabled = state.Stopping && state.Active || !state.Active && !state.Eligible;
        _handOver.Disabled = !state.Active;
        ClampPosition();
    }

    public override void _Input(InputEvent input)
    {
        if (_dragging && input is InputEventMouseMotion motion)
        {
            _box.Position += motion.Relative;
            ClampPosition();
            GetViewport().SetInputAsHandled();
        }
        if (input is InputEventKey { Pressed: true, Echo: false } key)
        {
            if (key.Keycode == Key.F8) { Entry.TogglePause(); GetViewport().SetInputAsHandled(); }
            else if (key.Keycode == Key.F9) { Entry.HandOver(); GetViewport().SetInputAsHandled(); }
        }
        if (_dragging && input is InputEventMouseButton { ButtonIndex: MouseButton.Left, Pressed: false })
        {
            _dragging = false;
            RememberPosition();
            GetViewport().SetInputAsHandled();
        }
    }

    private void HeaderInput(InputEvent input)
    {
        if (input is not InputEventMouseButton { ButtonIndex: MouseButton.Left, Pressed: true }) return;
        _dragging = true;
        _header.AcceptEvent();
    }

    private void ToggleFold()
    {
        Entry.Preferences.Collapsed = !Entry.Preferences.Collapsed;
        ApplyFold();
        Entry.SavePanelPreferences();
    }

    private void ApplyFold()
    {
        _body.Visible = !Entry.Preferences.Collapsed;
        _fold.Text = Entry.Preferences.Collapsed ? "+" : "−";
        _box.ResetSize();
    }

    private void ClampPosition()
    {
        Vector2 viewport = _box.GetViewportRect().Size;
        _box.Position = new Vector2(Math.Clamp(_box.Position.X, 0, Math.Max(0, viewport.X - _box.Size.X)),
            Math.Clamp(_box.Position.Y, 0, Math.Max(0, viewport.Y - _box.Size.Y)));
    }

    private void RememberPosition()
    {
        Entry.Preferences.X = _box.Position.X;
        Entry.Preferences.Y = _box.Position.Y;
        Entry.SavePanelPreferences();
    }
}
