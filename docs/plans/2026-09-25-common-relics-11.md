# M4 共享遗物批次 11

## 目标

移植四件使用牌组筛选、复制或附魔的共享遗物：Dolly's Mirror、Gnarled Hammer、Punch Dagger、Royal Stamp。

## 任务

1. 对照 v0.111.0 源码移植各自牌组候选条件、选择数量、附魔数值和随机流消耗。
2. 为选择和附魔结果添加定向遗物测试。
3. 生成模型分发代码并运行 `scripts/check.sh`。
4. 更新 README、路线图和执行报告，记录剩余共享遗物。

## 验收标准

- Dolly's Mirror 复制所选非 Quest 牌进入牌组。
- Gnarled Hammer 最多选择 3 张可附魔攻击牌，施加 Sharp 3；Punch Dagger 选择 1 张可附魔攻击牌，施加 Momentum 5。
- Royal Stamp 消耗与源码一致的 Niche 洗牌，并给一张可附魔攻击/技能牌施加 Royally Approved 1。
- 定向测试及 `scripts/check.sh` 全部通过；L4 无新增分歧。
- 文档记录本批覆盖和剩余限制。

## 依据与文件

- 真值：`../Slay the Spire 2/src/Core/Models/Relics/{DollysMirror,GnarledHammer,PunchDagger,RoyalStamp}.cs` 与相应附魔类，版本 v0.111.0。
- 实现：`game/Models/Relics/`、`tests/test_relics.cpp` 和生成分发代码。
- 进度：`README.md`、`docs/roadmap.md`；结果：`docs/reports/2026-09-25-common-relics-11.md`。
