# 通用遗物批次 16：未知地图点房间类型报告

计划：[`../plans/2026-09-26-common-relics-16-unknown-map-types.md`](../plans/2026-09-26-common-relics-16-unknown-map-types.md)。当前路线为 M4；共享遗物模型头文件已全覆盖，但成员行为仍按模型逐项移植。

## 实现

- 以可平凡复制的 `RoomTypeSet` 位集合承载 `IReadOnlySet<RoomType>`，并将类型映射加入成员与 Hook 生成器。
- `Hook::ModifyUnknownMapPointRoomTypes` 按 run hook listener 顺序组合所有已移植模型的变换。
- Juzu Bracelet 通过自身模型回调排除未知地图点的普通战斗候选类型；`UnknownMapPointOdds` 不再依赖遗物 ID 的专用分支。
- Lantern Key 与 Golden Compass 的该回调仍未移植；标准 dispatch 会对它们继续抛出 `Unported`。

## 验收

- `python3 tools/gen/gen_members.py && python3 tools/gen/gen_hook_header.py && python3 tools/gen/gen_dispatch.py`：通过；AbstractModel 170 个成员，Hook.h 138 个 dispatcher，9 个等待未支持类型；682 个模型接入分发。
- `cmake --build build -j2`：通过。
- `ctest --test-dir build --output-on-failure`：8/8 通过。新增遗物测试确认 Juzu 仅移除 Monster，保留 Event/Shop，并通过实际未知地图赔率入口抽取 64 次均未得到 Monster。
- Juzu 定向 L4：种子 `M4JUZU001,002,003,005,006,007,008` × 策略 `1,2`，不带 Neow：**14/14 局、730/730 决策一致**，0 `Unported`、0 分歧。
- 较宽的探索组还含 `M4JUZU004`：16 局中 15 局完全一致；另 1 局在第 61/66 个决策处遇到既有 `Relics.HandDrill.AfterBlockBroken` 未移植点。它发生在后续战斗，与地图房间筛选无关；该种子未纳入最终定向组。
- `./scripts/check.sh`：通过。ctest 8/8；冒烟无崩溃/卡死（各组结果：93 death/7 unported，99 death/1 unported，3 boss/97 death）；L2 **200/200**；L3 两组 **40/40**（784/784、827/827 状态）；L4 两组 **40/40**（1958/1958、2131/2131 决策），均 0 分歧、0 `Unported` 停止。
- Oracle 构建保留既存 `OracleRun.cs(234,84)` CS8604 可空性警告：0 错误。

## 限制

本批完成 Juzu Bracelet 的标准 Hook 路径，不代表所有未知地图房间筛选覆盖者都已移植。Golden Compass 的地图替换与取遗物后重建地图、以及 Lantern Key 的事件替换仍是独立内容缺口。
