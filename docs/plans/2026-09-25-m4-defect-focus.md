# M4 故障机器人球修饰批次

分支：`feat/m4-defect-focus`。前置：M2 完成、M3 暂跳过；Defect OrbQueue 和四球批次在 `4565678` 通过 A0/A10 L3 与完整门禁。

## 目标与任务

1. 按 v0.111.0 源码接入 `FocusPower` 对球被动/唤起数值的修饰，以及 `BiasedCognitionPower` 的回合初 Focus 递减。
2. 接入 `Defragment`、`Biased Cognition`、`Focused Strike`、`Hotfix`、`Hyperbeam` 五张牌；后三张依赖临时 Focus 能力。另接入 `Data Disk`、`Gold-Plated Cables`、`Runic Capacitor`、`Symbiotic Virus` 四件遗物。
3. 用 Oracle 和模拟器逐状态比较 Focus、球槽、球顺序、球数值和生命/格挡/能量；覆盖遗物、叠加、负 Focus、升级和跨回合触发。
4. 为 L4 的 Oracle 与模拟器回放增加相同的自定义牌组、遗物入口，使整局记录能稳定覆盖本批内容。
5. 根据 L4 首个 `Unported` 继续接入本批可达的 Defect 常见牌；目前为 `Leap`、`Go for the Eyes`、`Beam Cell`、`Uproar`、`Momentum Strike`、`Barrage`、`Lightning Rod`。

## 验收

- `tools/gen/gen_dispatch.py`、构建、`ctest`、`scripts/check.sh` 全部通过。
- Defect A0/A10 自定义牌组与遗物组合 L3 零 `Unported`、零分歧；至少一批同装载 L4 决策记录零分歧；报告记录场数、状态数和实际触发证据。
- 更新路线图和 README；未覆盖的 M4 内容继续明确标为进行中。
