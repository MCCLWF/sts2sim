# M4 共享遗物批次 10

## 目标

继续补齐可由现有奖励和战斗选牌接口直接承接的共享遗物：Cauldron、Orrery、Toolbox。

## 任务

1. 对照 v0.111.0 源码移植三个遗物的触发时机、奖励生成和卡牌池选择。
2. 生成模型分发代码并确认遗物注册。
3. 跑定向构建与测试，再运行 `scripts/check.sh` 全量门槛。
4. 更新 README、路线图和执行报告，记录共享遗物模型覆盖数及未实现项。

## 验收标准

- Cauldron 获得时提供 5 个随机药水奖励；不移植 TestMode 专用固定药水分支。
- Orrery 获得时提供 5 组奖励，每组为 3 张角色牌选 1。
- Toolbox 在战斗第 1 回合抽牌前生成 3 张不同的无色战斗牌并加入手牌。
- `scripts/check.sh` 全部通过，L4 无新增分歧。
- 文档明确列出覆盖与遗留限制。

## 依据与文件

- 真值：`../Slay the Spire 2/src/Core/Models/Relics/{Cauldron,Orrery,Toolbox}.cs`，版本 v0.111.0。
- 实现：`game/Models/Relics/` 下三个模型头文件与生成分发代码。
- 进度：`README.md`、`docs/roadmap.md`；结果：`docs/reports/2026-09-25-common-relics-10.md`。
