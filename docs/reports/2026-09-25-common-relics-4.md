# 通用遗物批次 4 报告

分支：`feat/m2-completion`。计划：[`../plans/2026-09-25-common-relics-4.md`](../plans/2026-09-25-common-relics-4.md)。

## 实现

- **Intimidating Helmet**：打出能量值至少为 2 的牌前获得 4 点格挡。
- **Miniature Cannon**：升级攻击牌造成的有源攻击额外造成 3 点伤害。
- **Sling of Courage**：进入精英房间时获得 2 点力量。
- **Very Hot Cocoa**：首回合开始时获得 4 点能量。

生成分发接入四件遗物；通用遗物直接钩子累计 17 个。`tests/test_relics.cpp` 检查升级前后伤害加成、打牌前格挡、精英房间力量和首回合能量。

## 验证与边界

完整 `scripts/check.sh` 是本批回归门禁；其结果记录在对应提交。Girya、Shovel 的休息点自定义选项和战士战斗中取得其他角色卡牌的路径仍未实现，后者继续显式 `Unported`。
