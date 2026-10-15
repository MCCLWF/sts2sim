# 无色牌批次 10：Bolas 与 Thrumming Hatchet

## 目标

移植两张攻击牌，并支持它们在上回合打出后于下次手牌抽取前回到手牌。

## 范围

- 为每个卡牌实例记录最近完成打出的玩家回合，保留 POD 状态。
- 在 `CombatHistory::CardPlayFinished` 写入该回合；两张牌的 `BeforeHandDraw` 只在玩家本人下次抽牌前、且牌不在手牌时加入手牌。
- Bolas：0 费，3 点 Move 伤害，升级后 4 点。
- Thrumming Hatchet：1 费，11 点 Move 伤害，升级后 14 点。
- 维持 Splash 的跨职业获取路径为 `Unported`。

## 验收

- 测试两张牌的伤害、升级值、同回合不回手、上回合打出后回手、当前手牌状态不产生重复添加，以及超过一回合后不回手。
- 运行生成分发、针对性测试、`git diff --check` 与完整 `scripts/check.sh`。
