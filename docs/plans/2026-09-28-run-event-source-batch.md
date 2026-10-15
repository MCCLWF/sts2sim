# Run 级事件源码批次

## 目标

按 STS2 v0.111.0 原版源码成批完成事件行为缺口，处理 Amalgamator、Doll Room、Field of Man-Sized Holes、Relic Trader、Stone of All Time、ZenWeaver、The Lantern Key 和先古 Tanx；补全通用事件战斗预建入口。随后按地图源码清单继续，不用固定种子的首停定义工作范围。

## 本批方法

1. 对照 `data/content.json`、原版 C# 的全部 override、选项、条件、RNG、选择和副作用。
2. 在通用 Env 的 EventModel/Commands 上逐字移植；新状态必须保持 POD。
3. 重新生成派发、更新地图清单、构建与运行正版差分；记录没有直接路径证据的行为。
4. 完整内容批次合并后，再以每次新生成的 100+ 种子进行整局探索和回归语料固化。

## 验收

- 本批七个普通事件和 Tanx 的行为方法都有实现或明确的纯表现层说明。
- `tools/gen/gen_dispatch.py`、地图清单检查、`scripts/check.sh` 通过。
- 报告区分源码移植、直接 oracle 证据和未覆盖的随机路径。
