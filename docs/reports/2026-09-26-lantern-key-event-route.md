# M4 内容批次：Lantern Key 与 War Historian Repy 路线

计划：[`../plans/2026-09-26-lantern-key-event-route.md`](../plans/2026-09-26-lantern-key-event-route.md)。本批接通 `ModifyNextEvent` 通用分发，并补 Lantern Key 在第三幕的未知点事件路线。

## 实现

- 为 `EventModel` 增加生成器类型映射，生成 `AbstractModel::ModifyNextEvent` 与 Hook 声明/分发。
- `RunManager::PullNextEvent` 按随机事件选择、运行时模型覆写、记录最终事件 ID 的顺序执行。
- Lantern Key 不可升级；在 act index 2 将未知点候选固定为 Event，并把下一个事件替换为 War Historian Repy。前两幕不修改房间候选或事件。
- War Historian Repy 的 `GenerateInitialOptions` 仍显式 `Unported`，进入该事件后会受控停止。

## 验收

- 生成器：通过；AbstractModel 175 个成员，Hook.h 142 个 dispatcher，5 个等待未支持类型，685 个模型接入分发。
- `cmake --build build -j2`：通过。
- Lantern Key 定向行为测试：通过。测试检查前两幕不变、第三幕事件替换和事件房间过滤；实际进入第三幕未知点后确认房间成为 Event、`room_event` 为 War Historian Repy、最终事件 ID 被记入已访问列表，并在选项生成处受控抛出 `Unported`。
- `ctest --test-dir build --output-on-failure`：8/8 通过。
- `./scripts/check.sh`：通过。冒烟无崩溃/卡死（94 death/6 unported，100 death，3 boss/97 death）；L2 **200/200**；L3 两组 **40/40**（784/784、827/827）；L4 两组 **40/40**（1958/1958、2131/2131），均 0 分歧、0 `Unported` 停止。
- Oracle L4 当前只运行到第一幕 Boss，本批第三幕路线不在 L4 覆盖范围内；以直接进入地图点的集成测试验证运行时路径。
- Oracle 构建保留既存 `OracleRun.cs(234,84)` CS8604 可空性警告：0 错误。

## 限制

Repy 事件本身的选项生成与后续分支仍未移植。本批只接通 Lantern Key 触发路线与最终事件选择，M4 继续进行中。
