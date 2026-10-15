# M4 Defect 充能球与起始牌

分支：`feat/m2-completion`。M2 已完成；M3 按当前路线暂跳过。本工作接入 OrbQueue 的 POD 状态、按源码顺序的充能/唤起/被动调用、Defect 起始遗物和起始牌。

## 实现范围

1. OrbState 定长池、玩家 OrbQueue 和 OrbModel 句柄；Hook 监听者按原版位置枚举球。
2. `OrbCmd` 充能、前后球唤起、被动触发、槽位调整；回合末与回合初的球触发。
3. Lightning/Frost/Dark/Plasma 基础球行为及其可达 Hook；Defect `Cracked Core`、`Zap`、`Dualcast` 起始链路。
4. L3 双方快照记录球槽容量、球的顺序和数值，避免只比较球名而漏掉容量变化及 Dark 叠值。

## 验收

- 构建、单元测试和 `scripts/check.sh` 通过；状态保持 trivially copyable。
- Defect 起始牌 A0/A10 多种子 L3 零分歧；额外自定义牌组覆盖满槽唤起、Dualcast 和球的回合触发。
- 已知尚未接入的球修饰遗物/能力以 `Unported` 明确停止，报告记录具体范围。
