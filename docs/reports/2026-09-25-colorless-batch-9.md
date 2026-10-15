# 无色牌批次 9 报告

## 前置状态

- 基于 v0.111.0 的 GoldAxe、Rend、SeekerStrike、Volley 源码。
- 本批开始前，无色牌池 65 张中有 40 张已具备 C++ 模型头文件。
- 工作区处于 `feat/m2-completion`，M2 与 M3 接口已验收。

## 实现

- Gold Axe 根据本场已完成的卡牌出牌次数计算伤害；其升级保留关键词按升级表应用。
- Rend 根据目标身上的非临时减益 Power 数量计算伤害；力量类临时 Power 及其他临时 Power 均排除。升级数值由游戏升级表驱动。
- Seeker Strike 用 `CombatCardSelection` RNG 按 .NET 排序后洗牌，从抽牌堆取最多三张候选，再按抽牌堆选择界面的稀有度/卡牌顺序让玩家选择一张加入手牌。
- Volley 使用解析后的 X 值进行随机多段攻击。新增的 `ResolveEnergyXValue` 经过 `ModifyXValue` Hook，也修正了 Whirlwind 原先直接读取已捕获 X 值的问题。
- `CardSelectCmd::FromCombatPile` 增加受限候选重载，沿用既有抽牌堆展示排序与选择路径。
- 把 `ITemporaryPower` 的底层 Power 分类提取到 `PowerModel`，供 Unsettling Lamp 与 Rend 共用。单人无色牌模型头文件覆盖达到 44/65。

## 验收

命令：`./build/test_colorless`

结果：退出码 0。专项用例检查 Gold Axe 的出牌计数伤害、Rend 升级后的临时 Power 排除、Seeker Strike 的随机候选和抽牌堆移动、Chemical X 修饰下 Volley 的 4 次攻击，以及 Whirlwind 对 X 值 Hook 的解析。

命令：`git diff --check`

结果：通过，无空白错误。

命令：`./scripts/check.sh`

结果：退出码 0，末尾为 `all checks passed`。原始结果摘要：

```text
100% tests passed, 0 tests failed out of 8
smoke: death=93 unported=7
smoke: death=99 unported=1
smoke: boss=4 death=96
Build succeeded.
    1 Warning(s)
    0 Error(s)
L2 newrun: 200 runs, 0 mismatched
L3 combat: 40/40 combats identical, 0 unported, 0 diverged, 784/784 states matched
L3 combat: 40/40 combats identical, 0 unported, 0 diverged, 827/827 states matched
L4 run: 40 runs: 40 identical, 0 stopped at unported content, 0 diverged; 1958/1958 decisions matched
L4 run: 40 runs: 40 identical, 0 stopped at unported content, 0 diverged; 2131/2131 decisions matched
all checks passed
```

Oracle 构建的单条警告是 `OracleRun.cs(234,84)` 对 `RelicCmd.Obtain` 的 nullable 参数警告；构建成功且没有错误。

## 范围说明

本批完成 4 张无色牌模型，不代表无色牌池完整。其余未移植模型仍可能通过卡牌生成效果出现；铁甲战士获取其他角色牌的路径仍未实现，M4 继续进行。
