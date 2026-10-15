# 通用药水第二批执行报告

分支：`feat/m2-completion`  
日期：2026-09-25

## 已接通

新增 8 个直接效果药水：

- Beetle Juice：施加四层 Shrink。
- Fortifier：按当前格挡再获得两倍格挡。
- Fysh Oil：获得力量与敏捷。
- Mazaleth’s Gift：获得 Ritual。
- Potion of Binding：对可命中的敌人施加虚弱与易伤。
- Potion Shaped Rock：造成 15 点无属性伤害。
- Ship in a Bottle：获得格挡并获得下一回合格挡 Power。
- Star Potion：获得 3 点星星。

## 验证

`tests/test_potions.cpp` 现在覆盖 22 个药水模型；本批新增的每个效果都在真实战斗状态中检查。当前专用测试通过；完整门禁将在提交前执行。

## 边界

攻击/技能/能力药水、需要牌选择的药水、依赖尚未移植 Power 的药水和事件专属药水仍保持 `Unported`，没有静默替代。
