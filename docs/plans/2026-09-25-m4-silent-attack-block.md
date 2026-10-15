# M4 Silent 攻防与次回合抽牌批次

分支：`feat/m2-completion`。在已通过的 M2 和 Regent 批次之上，接入 Silent 的 Backstab、Dash、Predator、Footwork 和 `DrawCardsNextTurnPower`；同时把已有的 Slice 纳入本批对拍。

## 实现

- 按 v0.111.0 源码移植牌的伤害、格挡、能力施加顺序与升级数值。
- `DrawCardsNextTurnPower` 用回合开始时的能力数量增加抽牌，回合开始后移除。
- 生成分发代码；不改变状态的 POD 约束。

## 验收

- 构建与单元测试通过；Silent 自定义牌组在 A0/A10 多种子 L3 零分歧。
- 确认轨迹实际打出本批次牌，并在次回合触发额外抽牌。
- `scripts/check.sh` 不回归，报告记录原始结果与限制。
