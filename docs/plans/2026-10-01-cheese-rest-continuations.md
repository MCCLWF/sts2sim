# 新深层 seed 的奶酪房间及休息点续接修复

归属 E0/E3 与通用 Env 单人规则回归。基底 main 702d14f；API 伙伴在独立既有工作目录开发 RNG trace，彼此保留明确验收边界。200 个全新 seed ×5角色 ×A0/A10 的 coverage-v3 深层采集冻结 Git、源码、CLI、Oracle 至包装进程 EOF；候选仅在外部目录编译，不新增工作树。

## 任务

1. 保留本次2000身份原始结果及native原件，记录冻结输入/运行指纹和结束状态。
2. RoomFullOfCheese.Gorge 按原版 EventModel.SelectCardsToAddToDeckFromGrid 逐张 CardPileCmd.Add；保持公共批量Add和ZenWeaver的原生批量语义。
3. CLI的OracleRun单次休息策略在nested reward/proceed返回RestSite时完成同一个显式Proceed；last_chosen/proceed_enabled确保不跳过首次操作。公共Env的MiniatureTent继续允许额外操作。
4. 增加3条原版整局回归，额外断言路径实际触发：两个角色Gorge+BingBong选两张不同牌，以及MiniatureTent+TinyMailbox的HEAL→nested rewards→Map。
5. 跑完整scripts/check.sh、严格原始失败重放和较大相关原件回归；更新报告、路线图、README和坑点。通过后在原目录fast-forward合入main，删除临时修复分支。

## 已有证据与限制

外部候选5条原件8419/8419、206条相关原件256159/256159完整一致，快照后继开启；准备的3条fresh native回归3512/3512且实际触发。外部overlay指纹需与构建record配对，不替代正常仓库构建和完整门禁。

4条新容量失败原件保留，最大已导出活牌堆6348（未计deck原件及非pile身份根）；2048/4096扩容不足，8192尺寸探针也不是完整边界证明。此任务不以丢活牌、改RNG或放宽比较规避容量。旧2000基准仍归原冻结commit；逐抽RNG、全部原生合法集合、TestMode/full-live及全Env/RF资格不随本次提升。

## 涉及文件

- game/Models/Events/RoomFullOfCheese.h
- tools/cli/replay_cmd.cpp
- tests/l4/run_continuation_regressions.py
- scripts/check.sh
- docs/audits/pitfalls.md、docs/roadmap.md、README.md
- 本计划及对应执行报告
