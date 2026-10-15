# Archaic Tooth 获得时起始牌转换

日期：2026-09-27。分支 `feat/generic-env-rf-api`。计划：[起始牌转换](../plans/2026-09-27-env-archaic-tooth.md)。真值 v0.111.0 `ArchaicTooth.cs` 和正式 Oracle。

## 实现

保留原版 Bash→Break、Neutralize→Suppress、Unleash→Protector、FallingStar→MeteorShower、Dualcast→Quadcast 五种映射。获得时重新查当前牌组第一张对应起始牌，创建替换牌、继承升级状态，并交给通用 `CardCmd::Transform`；不把 Orobas 生成选项时保存的展示卡直接当作转换对象。带附魔起始牌的 MutableClone/Enchant 完整属性尚未支持，明确抛 `Unported`。

## 对拍

重新生成分发、Release 构建通过。对原版八条二幕轨迹 `/tmp/mcc-env-act2-batch-oracle.jsonl` 执行 `l4_run.py --trace ... --verify-snapshots --require-complete --verbose`：**SEED0002/policy2 原先在取得 Archaic Tooth 时以未移植停止，现在完整 747/747 决策一致，终点第二幕 Boss 战死亡**；SEED0002/policy1 同样完整564/564决策一致。其他五条继续停在独立的 Tezcatara/Pael 初始选项，零分歧；第八条第一幕死亡。日志 `/tmp/mcc-env-act2-batch-archaic-l4.log`。这批共有3/8完整、3674/5684已比较决策，停止原因来自另两个先古；不能把它称为八条整局通过。

实际取得后的转换路径已有原版差分和逐决策快照恢复证据。附魔、所有五角色映射分支及其它种子仍需单独验收；完整 `scripts/check.sh` 本批待运行。
