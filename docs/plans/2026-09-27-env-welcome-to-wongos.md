# 通用 Env：战士二幕 Welcome to Wongo's

日期：2026-09-27。分支 `feat/generic-env-rf-api`。战士整局主线；CombatSolver 冻结。

## 真值与目标

固定游戏 v0.111.0 `Models/Events/WelcomeToWongos.cs`、`Models/Relics/WongosMysteryTicket.cs`、`RelicFactory/RelicGrabBag` 和 `SaveManager.Progress.WongoPoints`。实际 `/tmp/mcc-env-act2-oracle.jsonl` 在 Orobas 接通后通过455/564决策，停于本事件选项生成；原版在本事件购买索引2的 Mystery Box，随后获得 Wongo's Mystery Ticket。

## 工作

1. 初始选项必须先从遗物袋取 Rare 且可入商店的特色遗物，再按金币决定四项的锁定状态。特色遗物保持规范模型直到实际购买，不凭当前轨迹跳过抽袋或其他选项。
2. 四个分支按原版顺序调用 LoseGold、遗物获取、积分与完成页；离开分支用事件局部 RNG 从升级牌中选一张降级。进度积分来自真实捕获的游戏进度事实，加入可恢复状态及 new-run 输入，不把原版规则伪装成固定零值。
3. 票券遗物效果由 GPT-6 Luna xhigh 独立移植；主线程生成分发、构建并对拍实际二幕轨迹，核对五场后奖励规则及恢复/查询。
4. 新状态字段纳入 POD schema/hash/快照校验及原版保存 JSON 中确有的玩家额外字段。

## 验收

相同轨迹的行动、选项、金币、遗物、地图/RNG、逐决策恢复零分歧；可用真实原版轨迹延续至下一明确阻塞或终点。所有未覆盖的 Wongo 积分/票券分支保持明确记录；完整 `scripts/check.sh` 通过后方可将本批视为集成。
