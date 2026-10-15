# 通用遗物批次 2 报告

分支：`feat/common-relics-2`。计划：[`../plans/2026-09-25-common-relics-2.md`](../plans/2026-09-25-common-relics-2.md)。

## 实现

- **Bag of Marbles**：首回合开始时对可攻击敌人施加 1 层易伤。
- **Bag of Preparation**：首回合抽牌数增加 2。
- **Tungsten Rod**：持有者失去生命时减免 1，最低为 0。
- **Mango**：获得时最大生命增加 14。
- **Lee's Waffle**：获得时最大生命增加 7，随后回复至满血。

以上钩子按原版 C# 行为接入生成分发。当前通用遗物直接钩子累计 8 个；不代表所有共享遗物均已完成。

## 验证

`tests/test_relics.cpp` 在真实 run/战斗状态检查上述五件遗物，并保留前一批三件遗物的回归检查。完整 `scripts/check.sh` 结果见本批提交记录。

## 边界

Girya 和 Shovel 的休息点自定义选项仍未实现。战士战斗中取得其他角色卡牌的路径仍明确为 `Unported`。
