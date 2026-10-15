# M4 储君起始机制与跨职业基础牌

分支：`feat/m2-completion`。M2 的严格 L4 与卡牌矩阵为回归门槛；M3 暂跳过。

## 目标

- 接入储君起始遗物 `Divine Right`、起始牌 `Strike/Defend/Falling Star/Venerate`，以及 `Gather Light`、`Solar Strike`、`Cloak of Stars`，验证星星的获得和支付。
- 接入故障机器人和亡灵契约师的打击、防御基础牌，为后续 OrbQueue 和 Osty 批次提供牌面基线。
- 扩展 L3 双方状态快照，显式比对星星数量。

## 涉及文件

- `game/Models/Cards/`、`game/Models/Relics/DivineRight.h`、`game/generated/`。
- `tools/Sts2Oracle/OracleCombat.cs` 与 `tools/cli/dump.cpp`。
- 本计划、执行报告与 `docs/roadmap.md`。

## 验收

1. 分发生成、C++ 和 Oracle 构建通过。
2. Regent 起始牌和扩展牌组各在 A0/A10、8 种子 × 2 策略的 L3 对拍中无分歧、无 `Unported`。
3. Defect/Necrobinder 基础攻防牌用跨职业自定义牌组做 A0/A10 L3 对拍。
4. M2 矩阵 540/540 和 `scripts/check.sh` 不回归。
