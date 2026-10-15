# 通用遗物批次 14：休息点选项

## 目标

移植通用遗物 Girya 与 Shovel 的休息点路径，覆盖选项生成、可用性、选择后的状态变化，以及 Girya 后续战斗中的力量效果。

## 范围

- 在单人 `RestSite::Generate` 中按 hook listener 顺序接入 Girya 的 Lift 与 Shovel 的 Dig 选项；执行选择时沿现有遗物袋抽取与获得命令。
- `Girya.AfterRoomEntered`、`DisplayAmount`、`ShowCounter` 与举重计数。
- 其余尚未移植的休息点选项保留 `Unported`，多人选项不纳入单人模拟器。

## 验收

- 单测验证 Girya 最多举重三次、第三次后不再提供选项、下一场战斗获得等量力量。
- 单测验证 Shovel 提供挖掘，选择后从当前遗物袋获得一个遗物。
- 构建、`ctest`、`git diff --check` 与完整 `scripts/check.sh` 通过。
