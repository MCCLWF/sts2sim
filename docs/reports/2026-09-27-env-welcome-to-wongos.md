# 战士二幕 Welcome to Wongo's 路径与票券验证

日期：2026-09-27。分支 `feat/generic-env-rf-api`。计划：[事件计划](../plans/2026-09-27-env-welcome-to-wongos.md)、[票券计划](../plans/2026-09-27-env-wongos-mystery-ticket.md)。真值为固定 v0.111.0 原版事件、票券遗物和正式 Oracle。

## 实现与状态

- 进事件先从玩家稀有遗物袋按 `IsAllowedInShops` 拉出规范的 FeaturedItem，并保存在事件状态；四个选项按原版金币门槛构造。购买与离开四个分支保留原版命令和事件 RNG 顺序。
- Wongo 历史积分作为 `NewRunOptions.progress_wongo_points` 输入并复制到 POD run 状态；本次购买获得的积分写入玩家额外字段。完成页/徽章按源码的积分取模与总数分支计算，快照和原版保存 JSON 纳入相应字段。当前 C ABI Reset 尚无此输入，真实档案捕获也尚缺；现有全解锁 Oracle 路径进度为0。
- WongosMysteryTicket 的五场战斗计数、第五场额外三件标准遗物奖励、发放后停用和显示计数由 GPT-6 Luna xhigh 移植并集成。SandCastle 等无关遗物不参与此路径。

## 原版证据

原二幕轨迹在 Orobas 接通后先停于455/564决策的本事件；事件及票券接入后相同命令 `--verify-snapshots --require-complete` 推进至 **546/564** 决策，之前零分歧，下一独立阻塞为 `LostWisp.CalculateVars`。日志 `/tmp/mcc-env-wongo-ticket-act2-l4.log`。实际本事件选择索引2购买 Mystery Box，金币498→198、获得票券。

额外原版验证：`l4_run.py --seeds SEED0002 --policy-seeds 1 --no-neow --hp 10000 --relics WONGOS_MYSTERY_TICKET --stop after-act1 --verify-snapshots --require-complete --verbose` 为 **1/1局、256/256决策**零分歧零Unported，日志 `/tmp/mcc-env-wongo-ticket-five-combat-l4.log`。原版轨迹 `/tmp/mcc-env-wongo-ticket-five-combat-oracle.jsonl` 中票券显示计数依次5→4→3→2→1，第五场奖励包含普通金币/药水、额外三件 Relic 和卡牌，随后票券不再显示正数计数。该差分同时经过同动作快照恢复验证。

## 限制

已有轨迹只实际选中 Mystery Box；Bargain Bin、Featured Item、Leave 与非零历史积分的原版分支仍需定向对拍。后续 Lost Wisp 接通后，同一条二幕轨迹已完整通过564/564决策并在第二幕Boss战死亡，见[专项报告](2026-09-27-env-lost-wisp.md)。真实游戏进度捕获和 C ABI 参数尚待 SDK/桥接阶段。完整 `scripts/check.sh` 本批仍待执行；本报告不将有限路径当作事件全部条件覆盖。
