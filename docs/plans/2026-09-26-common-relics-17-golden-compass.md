# 通用遗物批次 17：Golden Compass 专属地图

## 目标

按 v0.111.0 实现 Golden Compass：拾取后记录当前幕并重建地图；当前幕使用直线路径专属地图，未知点只生成事件；切幕后恢复常规地图。

## 工作项

- 为 `ActMap` 接入模型成员和 Hook 签名生成器。
- 实现 `ModifyGeneratedMap` 与 `ModifyGeneratedMapLate` 的 run listener 分发，并在 `RunManager::GenerateMap` 按游戏顺序调用。
- 在 POD 遗物额外状态中保存 `GoldenPathAct`，接通拾取后重建地图。
- 按 `GoldenPathActMap` 源码构建单人专属地图：16 个固定房间点、唯一中间路径、古代人起点与首领终点。
- 通过地图结构单测和强制持有 Golden Compass 的 Oracle L4 对拍验证；更新路线图与 README。

## 验收

- 拾取遗物后地图房间、点位坐标、父子边和未知点候选与源码一致。
- 当前幕地图被替换；新幕重建时恢复普通生成地图，未知点筛选也恢复。
- 生成器、构建、`ctest`、定向完整 L4 与完整 `scripts/check.sh` 通过。

## 涉及文件

- `engine/map.h`、`engine/map.cpp`
- `game/Models/signature_types.h`、`game/Models/AbstractModel.def`
- `game/Hooks/Hook.h`、`game/Hooks/Hook.cpp`
- `game/Runs/RunManager.cpp`
- `game/Models/Relics/GoldenCompass.h`
- `tests/test_relics.cpp`
- `tools/gen/gen_members.py`、生成 dispatch、README、路线图与报告
