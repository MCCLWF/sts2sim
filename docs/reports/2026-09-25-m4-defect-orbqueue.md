# M4 故障机器人充能球批次执行报告

计划：[`../plans/2026-09-25-m4-defect-orbqueue.md`](../plans/2026-09-25-m4-defect-orbqueue.md)。M2 已验收；M3 按当前路线暂跳过；M4 仍在进行中。

## 接入内容

- POD 球池与 `OrbModel` 分发；玩家球队列、容量、充能时满槽唤起、主动唤起、被动触发、回合末/回合初触发与战斗 Hook 顺序。
- Lightning、Frost、Dark、Plasma 四种基础球。Dark 的累计唤起值存于状态中。
- Defect 起始遗物 `Cracked Core`、起始牌 `Zap`、`Dualcast`；新增 `Ball Lightning`、`Cold Snap`、`Glacier`、`Darkness`、`Fusion`、`Capacitor`、`Coolheaded`、`Rainbow` 八张球牌。
- L3 Oracle 与模拟器快照增加球容量、队列顺序和各球唤起值，覆盖球机制的隐含状态。

## 对拍结果

- Defect 标准起始牌组：A0 **16/16 场、225/225 状态**；A10 **16/16 场、226/226 状态**。
- 密集 `Zap`/`Dualcast` 自定义牌组：A0 **16/16 场、157/157 状态**；A10 **16/16 场、165/165 状态**。
- 四球混合牌组（上述八张新增牌及 `Zap`、`Dualcast`）：A0 **16/16 场、169/169 状态**；A10 **16/16 场、177/177 状态**。另将十张牌全部升级后在 A10 对拍 **16/16 场、166/166 状态**。
- 合计 **112/112 场、1285/1285 状态**，0 `Unported`、0 分歧。混合牌组的 Oracle 轨迹实际打出全部十张牌，四种球均出现，Dark 唤起值最高达到 24。
- `scripts/check.sh`：完整构建、单元测试、冒烟、L2、L3、L4 门禁通过；日志为本机 `/tmp/m4_defect_final_check.log`。

## 后续范围

Focus 等球修饰能力、更多 Defect 牌与遗物、Osty、后两幕和其余 M4 内容尚未完成。下一批应从球修饰 Hook 的真实可达链路或 Necrobinder 宠物链路继续扩充，并继续逐状态对拍。
