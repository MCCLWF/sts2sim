# 通用药水批次 4 报告

分支：`feat/common-potions-4`  
日期：2026-09-25

## 本批次

- `AttackPotion`、`PowerPotion`、`SkillPotion`：从持有者的角色牌池生成 3 张不同类别牌，使用现有选择前缀让玩家选择 1 张，所选牌本回合免费并加入手牌。
- `BlessingOfTheForge`：升级当前手牌中所有可升级牌。

选择型药水复用了 `CardFactory::GetForCombat`、`CardSelectCmd::FromChooseACardScreen` 和 `CardPileCmd::AddGeneratedCardToCombat`。这保留了环境在 `NeedChoice` 时回滚并重放的语义。

## 验证

- `tests/test_potions.cpp` 覆盖选择数量、选项索引、生成牌进入手牌和药水槽移除。
- 当前已实现并标记可用的通用药水总数为 32 个。
- Colorless Potion 暂未接入；它可能生成尚未移植的无色牌，不能借此绕过无色牌的未实现边界。
