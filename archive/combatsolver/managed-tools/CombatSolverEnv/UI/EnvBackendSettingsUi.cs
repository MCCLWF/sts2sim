using Godot;

namespace CombatSolver;

internal static class EnvBackendSettingsUi
{
    internal static void Append(VBoxContainer page)
    {
        GridContainer grid = new() { Columns = 2, SizeFlagsHorizontal = Control.SizeFlags.ExpandFill };
        grid.AddThemeConstantOverride("h_separation", SolverUiTokens.Spacing.Lg);
        grid.AddThemeConstantOverride("v_separation", SolverUiTokens.Spacing.Sm);

        OptionButton mode = new() { CustomMinimumSize = new Vector2(260, 32), FocusMode = Control.FocusModeEnum.None };
        mode.AddItem(SolverText.Get("Native（原生搜索）"), (int)EnvBackendMode.Native);
        mode.AddItem(SolverText.Get("Env（状态不支持时回退 Native）"), (int)EnvBackendMode.Env);
        mode.AddItem(SolverText.Get("同时核对 Env / Native"), (int)EnvBackendMode.Both);
        mode.Selected = mode.GetItemIndex((int)EnvBackendSettings.Mode);

        CheckButton compare = new() { ButtonPressed = EnvBackendSettings.Compare, FocusMode = Control.FocusModeEnum.None };
        mode.ItemSelected += index =>
        {
            EnvBackendSettings.Update((EnvBackendMode)mode.GetItemId((int)index), compare.ButtonPressed);
            Entry.Logger.Info("[CombatSolverEnv] BACKEND_SETTINGS_APPLIED next_combat_boundary=true unported_falls_back_native=true");
        };
        compare.Toggled += enabled => EnvBackendSettings.Update(EnvBackendSettings.Mode, enabled);
        AddRow(grid, SolverText.Get("搜索后端"), mode,
            SolverText.Get($"Env 当前门禁：{EnvBackendSettings.EnvExecutionUnavailableReason}；完整适配完成前会明确拒绝并由 Native 继续。Both 显示两种路径的检查结果。"));
        AddRow(grid, SolverText.Get("显示 Env / Native 对比"), compare,
            SolverText.Get("记录节点、转移和耗时对比；核对本身会增加 Native 测量时间。"));
        AddSection(page, SolverText.Get("Env 搜索后端"),
            SolverText.Get("设置会持久化。当前 live 根仍在逐字段冻结，发生未移植、奖励订阅或未知字段时会显示原因并安全回退 Native。"), grid);
    }

    private static void AddRow(GridContainer grid, string title, Control control, string tooltip)
    {
        Label label = SolverUiTokens.CreateLabel(title, SolverUiTokens.Type.Body, SolverUiTokens.Palette.TextPrimary);
        label.TooltipText = tooltip;
        control.TooltipText = tooltip;
        grid.AddChild(label);
        grid.AddChild(control);
    }

    private static void AddSection(VBoxContainer page, string title, string description, Control content)
    {
        VBoxContainer section = new() { SizeFlagsHorizontal = Control.SizeFlags.ExpandFill };
        section.AddThemeConstantOverride("separation", SolverUiTokens.Spacing.Sm);
        section.AddChild(SolverUiTokens.CreateLabel(title, SolverUiTokens.Type.Body, SolverUiTokens.Palette.TextPrimary));
        Label hint = SolverUiTokens.CreateLabel(description, SolverUiTokens.Type.Caption, SolverUiTokens.Palette.TextMuted);
        hint.AutowrapMode = TextServer.AutowrapMode.WordSmart;
        section.AddChild(hint);
        section.AddChild(content);
        page.AddChild(section);
    }
}
