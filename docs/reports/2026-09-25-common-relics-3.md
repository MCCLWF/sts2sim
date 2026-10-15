# 通用遗物批次 3 报告

分支：`feat/m2-completion`。计划：[`../plans/2026-09-25-common-relics-3.md`](../plans/2026-09-25-common-relics-3.md)。

## 实现

- **Akabeko**：首回合开始时给予持有者 8 点 Vigor。
- **Bellows**：首回合开始时升级手牌。
- **Bread**：首回合不增加能量，首回合开始扣除 2 点能量；后续回合最大能量增加 1。
- **Chandelier**：第三回合开始获得 3 点能量。
- **Chemical X**：X 费用或 X 星费用牌的 X 值增加 2，并保留触发提示钩子。

这些是直接战斗/能量修饰，不引入跨职业卡池或休息点选择。通用遗物直接钩子累计 13 个。

## 验证

`tests/test_relics.cpp` 检查 Akabeko 首回合 Vigor 和 Bread 能量修饰；生成分发、构建和完整 `scripts/check.sh` 作为本批门禁。

## 边界

Chemical X 的实际 X 牌轨迹和 Bellows 的升级手牌由现有战斗管线驱动；Girya、Shovel 等休息点自定义选项仍未实现。战士战斗中获取其他角色卡牌的路径继续保持 `Unported`。
