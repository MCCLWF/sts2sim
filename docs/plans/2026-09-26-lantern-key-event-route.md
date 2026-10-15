# M4 内容批次：Lantern Key 事件路线

## 目标

接入通用 `ModifyNextEvent` Hook，实现 Lantern Key 在 Glory 幕（act index 2）把未知房间改为事件，并将事件替换为 War Historian Repy。

## 工作项

- 为 EventModel 句柄生成 AbstractModel 与 Hook 的事件替换签名。
- `RunManager::PullNextEvent` 对基础事件执行源码相同的覆写顺序，并用最终事件 id 更新已访问记录。
- 添加 Lantern Key 卡模型：不可升级、不可打出、幕三限制未知点为事件并替换下一个随机事件。
- 用行为测试直接进入幕三未知点，检查地图候选、最终事件 id 和访问记录；更新路线图和 README。

## 验收

- Lantern Key 在前两幕保持原事件/房间候选；在第三幕把事件替换为 War Historian Repy 且未知点仅有 Event。
- 生成器、构建、`ctest` 与完整 `scripts/check.sh` 通过。当前 Oracle L4 仅运行到第一幕 Boss，因此不覆盖这条第三幕路线。
- War Historian Repy 自身的选项回调仍显式标记 `Unported`，不静默近似。

## 涉及文件

- `tools/gen/gen_members.py`、生成 AbstractModel/Hook 签名
- `game/Models/Cards/LanternKey.h`
- `game/Hooks/Hook.h`、`game/Hooks/Hook.cpp`
- `game/Runs/RunManager.cpp`
- `tests/test_relics.cpp`
- README、路线图与执行报告
