# M4: 完成全部非英雄专属遗物

日期：2026-09-28
源码基线：STS2 v0.111.0 (`41cef1ea`)
分支：`feat/all-nonhero-relics`

状态：✅ 已完成；结果见[完成报告](../reports/2026-09-28-nonhero-relics.md)。

## 目标与范围

完成非英雄专属遗物在整局模拟中的规则实现，并以原版 oracle 对拍。依据 `data/content.json` 的 `RelicModel` 清单和原版 `RelicPools`：覆盖共享遗物池、事件遗物池（含先古、开局和事件遗物）、回退遗物池、已废弃遗物池中的全部 259 件遗物。排除 Ironclad、Silent、Defect、Regent、Necrobinder 五个角色专属遗物池；英雄专属遗物随各角色工作完成。

## 当前盘点

- 导出内容包含 299 个遗物模型；其中 259 个属于本计划范围。
- 当前 `IsUsable(ModelKind::Relic, ...)` 仅允许其中 235 个进入模拟，另有 24 个非英雄专属遗物未完成：
  `BLACK_STAR`, `BLESSED_ANTLER`, `BLOOD_SOAKED_ROSE`, `BRILLIANT_SCARF`, `CIRCLET`, `CROSSBOW`, `DAUGHTER_OF_THE_WIND`, `DEPRECATED_RELIC`, `DIVINE_DESTINY`, `GIRYA`, `GLITTER`, `INFUSED_CORE`, `IRON_CLUB`, `LORDS_PARASOL`, `PANDORAS_BOX`, `PHYLACTERY_UNBOUND`, `POLLINOUS_CORE`, `RING_OF_THE_DRAKE`, `ROYAL_POISON`, `SAI`, `SHOVEL`, `SPIKED_GAUNTLETS`, `VELVET_CHOKER`, `WAR_HAMMER`。
- 其余 235 个虽已可用，也必须以 oracle 对拍结果和原版源码覆盖盘点确认；`IsUsable` 本身不是准确性证明。

## 实施要求

1. 为全部 24 件缺项逐字移植 v0.111.0 原版逻辑及其依赖的奖励、能量、卡牌生成、休息点、商店、先古、状态序列化等路径。
2. 所有影响模拟行为的状态均须正确进入 snapshot、hash、GC/root 遍历和恢复；只跳过视觉表现层。
3. 逐一对照原版源码审核全部 259 件范围内的模型覆写；对 24 件缺项按其行为选择定向场景与动作，用 `Sts2Oracle` / L3 或 L4 差分对拍。其余已实现的 235 件以生成器覆写检查、既有定向记录及完整差分门禁验收。修复模拟器首因，不放宽比较器或修改原版逻辑。
4. 对全部 259 件遗物检查成员覆写均已实现或有明确的无行为/表现层依据；确认全范围 `IsUsable`，同时保持五个角色专属遗物池不在本工作承诺内。
5. 更新 `docs/roadmap.md`、`README.md`，记录验收命令、原始对拍结果和已知限制到报告。

## 验收标准

- 全范围非英雄遗物 259/259 被源码审阅并可用；没有本范围 relic 的 `Unported` 路径。
- 受影响的新旧机制有直接测试或定向 oracle 场景；逐项 oracle 对拍无分歧，分歧数为 0。
- 相关完整路线对拍无分歧；snapshot 验证覆盖新增持久状态。
- `scripts/check.sh --quick` 通过；对牌组/地图/战斗/奖励有影响的本次完整差分命令实际执行并记录。
- 最终报告列出所有命令、计数、结果与未覆盖限制；不以测试通过替代源码审计。

## 涉及区域

`game/Models/Relics/`、`game/Models/RelicModel.*`、`game/Models/AbstractModel.*`、相应命令与 run/combat/reward/rest-site/shop 子系统、`tools/Sts2Oracle/`、`tests/`、生成文件，以及路线图、README 和本报告。

## 结果

范围内 259 件均被源码池清单识别，并在生成的 `IsUsable` 中可用；24 件原缺项已补齐。新增 L3 遗物组、完整新增遗物路线、严格 run 续跑和全量 `scripts/check.sh` 均零分歧。剩余 23 件不可用遗物全部属于明确排除的五个英雄专属池。完整验证与覆盖数字见[报告](../reports/2026-09-28-nonhero-relics.md)。
