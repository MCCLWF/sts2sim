# 通用 Env：生成卡牌命令的空结果路径核对

日期：2026-09-27。状态：对照完成；单卡包装器已显式保持原版空索引失败语义。

## 目标

按 v0.111.0 原版 `Commands/CardPileCmd.cs` 核对 `AddGeneratedCardToCombat` 与 `AddGeneratedCardsToCombat` 在战斗未进行、没有 CombatState 时的空结果路径。仅当 C++ 与原版语义不一致时修正对应基础命令。

## 范围

- 对照原版 `../Slay the Spire 2/src/Core/Commands/CardPileCmd.cs` 和模拟器 `game/Commands/CardPileCmd.cpp`、`.h`。
- 若需修复，只能改 `game/Commands/CardPileCmd.cpp`、`.h`，并记录本计划与报告。
- 不改卡牌、生成文件、RunManager、Oracle 或 CMake；不生成、不构建、不测试、不提交。
- 不改变有效战斗路径中的 Hook、历史记录和 RNG 顺序。

## 对照判据

1. 批量方法在空输入、战斗未进行或 CombatState 缺失时均返回空集合。
2. 单卡方法对空批量结果必须明确失败；不得因 C++ release 下 unchecked `operator[]` 而返回默认槽值。
3. 报告注明原版行号、空结果路径和调用影响范围。
