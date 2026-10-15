# 共享遗物批次 13：Unsettling Lamp

## 目标

移植最后一个尚无 C++ 模型头文件的共享遗物 `UnsettlingLamp`，实现其战斗中首次由铁甲战士卡牌施加减益时翻倍的规则，包括临时力量牌的内部属性修正防止二次翻倍。

## 工作项

- 新增 `UnsettlingLamp` 遗物模型，按 v0.111.0 源码实现战斗开始、减益倍率、出牌完成和战斗结束状态重置。
- 补齐所有 `ITemporaryPower` 的底层力量映射，确保其内部应用的力量不会被再次翻倍。
- 添加直接行为测试：一般减益与同卡后续效果、不同卡来源、临时力量牌、Artifact 目标以及出牌后停止触发。
- 更新 README 与路线图中的共享遗物覆盖率。
- 运行 `scripts/check.sh` 并记录原始验收结果。

## 验收标准

- `UnsettlingLamp` 通过生成器接入 dispatch，整体共享遗物模型头文件达到 118/118。
- 新行为测试通过，且无临时力量牌内部效果二次翻倍。
- `scripts/check.sh` 全部通过。

## 涉及文件

- `game/Models/Relics/UnsettlingLamp.h`
- `tests/test_relics.cpp`
- 生成的 dispatch 文件及生成器输出
- `README.md`
- `docs/roadmap.md`
- `docs/reports/2026-09-25-common-relics-13.md`
