# 通用药水批次 3 报告

分支：`feat/common-potions-3`  
日期：2026-09-25

## 本批次

- `ExplosiveAmpoule`：对所有可攻击敌人造成 10 点无加成伤害。
- `FlexPotion` / `SpeedPotion`：分别施加临时力量和临时敏捷，回合结束恢复内部属性。
- `RegenPotion`：回合结束恢复 5 点生命并递减层数。
- `ShacklingPotion`：对所有敌人施加临时负力量。
- `GigantificationPotion`：下一张由持有者打出的攻击造成 3 倍伤害，并在攻击后消耗一层。

临时 Power、药水移除和 Hook 继续走现有 `PotionModel::OnUseWrapper` / `PowerCmd` 路径。带卡牌选择的 Attack/Power/Skill/Colorless Potion 不在本批次内。

## 验证

- `tests/test_potions.cpp` 覆盖 28 个已实现药水，并额外验证新 Power 的即时效果和回合结束行为。
- `python3 tools/gen/gen_dispatch.py` 后，新增 6 个药水及其 5 个支持 Power 进入 `IsUsable`。
- 本批次合入前运行完整 `scripts/check.sh`。
