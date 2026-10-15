# 通用遗物批次 17：Golden Compass 专属地图报告

计划：[`../plans/2026-09-26-common-relics-17-golden-compass.md`](../plans/2026-09-26-common-relics-17-golden-compass.md)。本批建立在未知地图房间类型 Hook 已可分发的基础上。

## 实现

- `ActMap` 现可用于可覆写成员签名；Hook 按游戏顺序串接普通与后置地图变换。
- `RunManager::GenerateMap` 在生成标准地图后执行两类变换。`AfterMapGenerated` 会改动传入的引用型地图，当前继续由 run hook guard 受控拦截。
- 新增 Golden Compass 模型，使用 relic POD `extra[0]` 保存 `GoldenPathAct + 1`。拾取后立即按当前幕重建地图。
- 新增单人 Golden Path 地图：古代人起点、16 个固定房间点、中心单路径、首领终点；对应幕的未知地图点仅保留 Event。
- 离开获得遗物的幕后，地图和未知点筛选恢复标准行为。
- SpoilsMap 的普通/后置地图覆写继续显式标记为未移植；Fur Coat 等未实现覆写会由标准 dispatch 受控报告。

## 验收

- `python3 tools/gen/gen_members.py && python3 tools/gen/gen_hook_header.py && python3 tools/port/refresh_not_ported.py && python3 tools/gen/gen_dispatch.py`：通过；AbstractModel 172 个成员，Hook.h 140 个 dispatcher，6 个等待未支持类型，683 个模型接入分发。
- `cmake --build build -j2`：通过。
- `ctest --test-dir build --output-on-failure`：8/8 通过。新增单测逐项核验固定房间类型、坐标、父子边、16 层路径、起终点、当前幕事件过滤和换幕后恢复。
- Golden Compass 定向 L4：种子 `M4GOLDEN001`–`004` × 策略 `1,2`，不带 Neow、500 HP、强制 `GOLDEN_COMPASS`：**8/8 局、1273/1273 决策一致**，0 `Unported`、0 分歧。
- `./scripts/check.sh`：通过。ctest 8/8；冒烟无崩溃/卡死（93 death/7 unported，99 death/1 unported，3 boss/97 death）；L2 **200/200**；L3 两组 **40/40**（784/784、827/827 状态）；L4 两组 **40/40**（1958/1958、2131/2131 决策），均 0 分歧、0 `Unported` 停止。
- Oracle 构建保留既存 `OracleRun.cs(234,84)` CS8604 可空性警告：0 错误。

## 限制

这是单人 Golden Compass 路径。`AfterMapGenerated` 的地图对象引用写入、SpoilsMap 的任务地图，以及 Fur Coat 的后置标记仍未移植；持有这些覆写者时按现有 guard/dispatch 规则受控停止。
