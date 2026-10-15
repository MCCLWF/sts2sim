# M4 亡灵契约师起始链路

分支：`feat/m4-necrobinder-osty`。M2 已验收，M3 暂跳过。

## 目标

按 v0.111.0 原版实现 Osty 宠物、`DieForYouPower`、`BoundPhylactery`、`Bodyguard`、`Unleash`。覆盖开战召唤、回合开始增益、代挡、死亡与复活、Osty 出手及升级数值。对拍需纳入宠物及其能力状态，避免遗漏可见差异。继续按 L4 停止点接入 `Afterlife`、`Defile`、`Fear`、`Drain Power`、`Debilitate`、`Enfeebling Touch` 和直接依赖的能力。

## 验收

1. 生成分发和构建通过，`scripts/check.sh` 全量门禁通过。
2. Necrobinder 起始牌组 A0/A10 L3 逐状态一致；含上述牌的自定义 L4 A0/A10 逐决策一致。
3. 记录实际覆盖的种子、战斗、状态和决策；更新路线图与 README，明确 M4 其余未接入范围。
