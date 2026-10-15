# 生成卡牌命令空结果路径对照报告

日期：2026-09-27。状态：实现与针对性合同测试已通过；未做独立原版非法调用轨迹对拍。

## 依据

- 原版 v0.111.0：`../Slay the Spire 2/src/Core/Commands/CardPileCmd.cs`。
- 模拟器：`game/Commands/CardPileCmd.cpp`、`game/Commands/CardPileCmd.h`。

## 对照结果

| 行为 | 原版 | C++ | 结论 |
|---|---|---|---|
| 单卡生成方法 | 原版 267–270 行把单卡包装成集合，再对 `AddGeneratedCardsToCombat(...)` 返回值取 `[0]`；空集合索引会失败 | C++ 先对批量返回值取 `[0]`；`StaticVec::operator[]` 只用 `assert(i < size_)` 检查，release 编译会移除该检查 | 不一致，已在取 `[0]` 前检查 `empty()` 并抛 `GameError` |
| 空输入 | 原版 283–287 行返回空数组 | C++ `CardPileCmd.cpp:102–103` 返回空 `CardPileAddResults` | 一致 |
| 战斗未进行 | 原版 288–291 行返回空数组 | C++ `CardPileCmd.cpp:104–105` 在 `!IsInProgress()` 时返回空结果 | 一致 |
| CombatState 缺失 | 原版 300–304 行返回空数组 | C++ `CardPileCmd.cpp:110–111` 返回空结果 | 一致 |

批量方法的空集合条件本身与原版一致。差异在 C++ 容器的访问语义：`engine/containers.h:35–40` 中 `StaticVec::operator[]` 仅有调试断言；debug 下断言会终止，release 下断言移除后本路径会读空容器的默认槽，不能保留原版空索引失败语义。单卡包装器现在保存批量返回值，在为空时显式抛 `GameError`，然后再读取 `[0]`。它不返回默认 `CardPileAddResult`，其默认值会将失败变成正常返回。

## 影响范围

- 批量 API 在上述条件下不记录生成历史、不调用生成 Hook，也不抽 RNG；两端均在这些操作前返回。
- 单卡 API 在这些状态下明确抛 `GameError`，对应原版对空结果 `[0]` 的失败；有效进行中的战斗照常记录每张卡的生成历史、加入目标牌堆并调用 `AfterCardGeneratedForCombat`。空检查发生在批量命令返回后，不改变 Hook 或 RNG 顺序。
- C++ 单卡调用点分布在卡牌、药水、遗物、能力和怪物模型中；大多由战斗内效果触发。新移植的 Shiv 生成路径在创建前对齐原版 `CombatManager.IsOverOrEnding` 检查。

## 验收

- 已逐行对照原版 267–304 行、C++ `CardPileCmd.cpp:94–117` 和 `engine/containers.h` 的 `StaticVec::operator[]`。
- 新增 `tests/test_generated_card_add.cpp`，覆盖非战斗、无 CombatState、空批量和有效战斗生成牌路径；针对无结果的单卡包装器验证抛错且不会把默认槽当作成功。已接入 CTest。
- CMake Release 构建退出0，`ctest --test-dir build --output-on-failure` 13/13通过；完整 `scripts/check.sh` 退出0，见 `/tmp/mcc-env-snapshot-full-check.log`。未做独立原版非法调用轨迹对拍；合法生成路径仍依赖已有 L3/L4 和 Hook 差分。
