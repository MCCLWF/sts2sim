# History Course 上一回合攻击牌记录

新采集的 100 个独立 run seed × 五角色 × A0/A10 中，`FF7429B102A14F2` / Ironclad / A0 / policy seed `892152143` 在第 934 决策首次出现 Test Subject HP 252/268、CombatTargets 47/46 差异。原始完整 native trace 和失败结果保留在本地批次 `deep-v3-fresh1000-method-values`。

原版 `HistoryCourse.AfterAutoPrePlayPhaseEntered` 查询 `CardPlaysFinished.LastOrDefault`，条件包括 `HappenedLastPlayerTurn`、Attack、非 Dupe。新回合抽牌阶段 Hellraiser 自动打出 Twin Strike，模拟器唯一的最近攻击牌记录被覆盖，丢失上一回合的最后一张攻击牌。

## 实施

- 单人历史保存最近两个发生过合格攻击牌完成事件的玩家回合及其最后一张攻击牌；同一回合继续更新最近记录，进入另一个回合时保留前一个记录。
- History Course 按精确 `TurnNumber - 1` 匹配两条记录，不回放更早的无攻击回合。
- 两条历史记录都纳入 POD 字段 visitor、hash、snapshot 与引用验证；保持 prefix replay 和卡牌生命周期语义。
- 扩展已有历史记录/恢复合同，并加入上述完整原生路线回归；不修改原版动作、RNG 或比较器。

## 验收

1. 原始失败 native 输入 SHA 不变，严格重放全部 960 决策、终点及 snapshot successors。
2. 新捕获同一路线的原生回归实际执行 History Course/Hellraiser 重叠路径并完整一致。
3. `scripts/check.sh` 全部门禁通过，记录实际外层退出与本次源/构建身份。
4. 在修复版本重放全部新 1000 输入，分别报告完整一致、规则差异和容量失败；不把旧版本资格转移到新版本。

本项属于 M4 规则查漏与通用 Env 历史语义。完整 Env/RF、正常 HP 的晚幕覆盖、全合法动作集、双端 RNG draws 以及容量/性能验收仍未完成。临时分支使用已有根目录，合并后删除，不创建工作树。
