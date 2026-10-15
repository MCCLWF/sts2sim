# M3 动作面补齐报告

分支：`feat/m3-action-surface`  
日期：2026-09-25

## 完成内容

- `Action::UsePotion` 和 `Action::DiscardPotion` 已进入 `Env::LegalActions()` 与执行分派。
- 药水动作按 `PotionUsage`、`CanUseOrRemovePotions`、`TargetType` 生成：战斗内枚举敌人/盟友/玩家目标，非战斗只开放 `AnyTime` 药水；战斗专用药水仍可在非战斗阶段丢弃。
- 执行路径复用 `PotionModel::OnUseWrapper` 与 `PotionCmd::Discard`，保留移除、Hook 和中途 `NeedChoice` 重放语义。
- C ABI 增加 `sts2_env_action_mask`；Python ctypes 增加 `Env.action_mask()`。mask 为 512 槽，按 `LegalActions()` 的稳定顺序置位。

## 验证

- `test_env`：药水采购、非战斗使用、槽位移除、mask 活跃槽数。
- `python/test_env.py`：C ABI mask 长度和活跃数。
- 本批次后会运行完整 `scripts/check.sh`，作为合入门禁。

## 后续性能项

- `Clone()` 的设计性能目标尚未通过基准实测；当前 `GameState` 仍约 138 KiB，列入 M5 性能回归。
- mask 使用稳定动作列表槽位，尚未提供可跨状态全局可逆的动作编码。
