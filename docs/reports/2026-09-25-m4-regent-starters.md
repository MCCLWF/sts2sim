# M4 储君起始批次执行报告

计划：[`../plans/2026-09-25-m4-regent-starters.md`](../plans/2026-09-25-m4-regent-starters.md)。分支：`feat/m2-completion`。前置条件：M2 严格 L4 与铁甲战士卡牌矩阵已达到验收线，M3 按路线暂跳过。

## 实际接入

- 储君起始遗物 `Divine Right`、起始牌 `Strike Regent`、`Defend Regent`、`Falling Star`、`Venerate`。
- 储君牌 `Gather Light`、`Solar Strike`、`Cloak of Stars`。
- 故障机器人和亡灵契约师的打击、防御基础牌；这四张牌仅完成源码移植，职业机制仍未接入。
- L3 的 Oracle 与模拟器快照增加 `stars`，星星的获取和消耗可直接逐状态比对。

## 验证结果

- `tools/gen/gen_dispatch.py`：448 个已移植模型，生成分发通过。
- `scripts/check.sh`：C++ 构建、2/2 单元测试、三组冒烟、Oracle 构建、L2 200/200、L3 A0/A10 各 40/40、L4 带/不带 Neow 各 40/40，全部通过。原始日志在本机 `/tmp/m2_m4_final_check.log`。
- Regent 标准起始牌组，种子 `M4REG001`—`M4REG008`、策略 `1,2`：A0 **16/16 场，279/279 状态**；A10 **16/16 场，338/338 状态**；均为 0 `Unported`、0 分歧。
- Regent 自定义牌组 `Strike Regent, Defend Regent, Falling Star, Venerate, Gather Light, Solar Strike, Cloak of Stars`，同样种子和策略：A0 **16/16 场，305/305 状态**；A10 **16/16 场，336/336 状态**；均为 0 `Unported`、0 分歧。A0 Oracle 轨迹中 `Falling Star` 打出 40 次、`Cloak of Stars` 打出 45 次；快照星星数量从 0 到 8。
- Defect/Necrobinder 四张基础攻防牌以 Ironclad 的跨职业自定义牌组单独验收，种子 `M4BASIC001`—`M4BASIC008`、策略 `1,2`、`TOADPOLES_WEAK`：A0 **16/16 场，271/271 状态**，A10 **16/16 场，294/294 状态**；职业起始机制仍需后续独立验收。
- M2 卡牌矩阵：**540/540 格，8640/8640 场，147638/147638 状态**，0 `Unported`、0 分歧；本机原始日志 `/tmp/m2_card_matrix_final.log`。

## 已知限制

- Regent 仅覆盖本报告列出的牌和星星起始机制，职业牌池余牌尚未接入。
- Defect OrbQueue、Necrobinder/Osty、第二/三幕等 M4 范围尚未验收，因此 M4 仍在进行中。
