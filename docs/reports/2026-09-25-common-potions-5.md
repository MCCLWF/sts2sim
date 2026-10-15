# 通用药水批次 5 报告

分支：`feat/common-potions-5`  
日期：2026-09-25

## 本批次

- `CureAll`：获得 1 点能量并抽 2 张牌。
- `LuckyTonic`：获得 1 层 Buffer。
- `StableSerum`：获得 2 层保留手牌效果，每个持有者回合结束递减 1 层。
- `PowderedDemise`：对敌人施加 9 层 Demise，每个敌方回合结束造成无视格挡伤害。
- `Duplicator`：下一张由持有者打出的牌额外结算一次，然后移除效果。

补充的 `BufferPower`、`RetainHandPower`、`DemisePower` 和 `DuplicationPower` 均接入现有 Hook/回合结束路径。

## 验证

- `tests/test_potions.cpp` 覆盖能量、抽牌、减层、回合结束伤害和复制出牌计数。
- 当前已实现并标记可用的通用药水总数为 37 个。
- 本批次合入前运行完整 `scripts/check.sh`。
