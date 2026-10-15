# 通用遗物批次 16：未知地图点房间类型

## 目标

把 `ModifyUnknownMapPointRoomTypes` 从缺少 C++ 集合类型的未移植钩子接入标准模型分发，并将 Juzu Bracelet 从硬编码分支迁移到遗物模型回调。

## 工作项

- 用可平凡复制的 `RoomTypeSet` 位集合表示游戏的 `IReadOnlySet<RoomType>`，接入成员与 Hook 生成器。
- 按源码的 run hook listener 顺序分发房间类型集合变换。
- 实现 Juzu Bracelet 移除 `Monster` 房间类型，保留其他候选项。
- 未移植的其他覆盖者继续经模型分发显式 `Unported`。
- 增加集合变换和未知地图点抽取测试，更新路线图与 README。

## 验收

- 生成脚本可重复运行，构建与 `ctest` 通过。
- 持有 Juzu Bracelet 时未知地图点不再抽到普通战斗房。
- 强制持有 Juzu 的整局 L4 与 v0.111.0 oracle 零分歧，并跑完整 `scripts/check.sh`。

## 涉及文件

- `game/Models/signature_types.h`
- `game/Models/AbstractModel.def`
- `game/Models/Relics/JuzuBracelet.h`
- `game/Hooks/Hook.h`、`game/Hooks/Hook.cpp`
- `game/Odds/UnknownMapPointOdds.cpp`
- `tests/test_relics.cpp`
- `tools/gen/gen_members.py`、`tools/gen/gen_hook_header.py`
- 生成的 dispatch、路线图、README 与执行报告
