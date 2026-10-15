# Env portable 与怪物历史摘要集成

API基线ec0eb1b，整合main的0c6c596/575e099。沿用完整Env/RF目标，用户已有仔细检查后merge授权。

1. 保留portable immutable FSM状态顺序，集成POD历史摘要及全部seen_moves校验；生成组合schema。
2. 保留上一构建证据与二进制，在组合构建上重新产生portable包，不迁移旧构建数据。
3. 完整scripts/check.sh、157定义/状态新进程矩阵、TestMode=false RF四组公共操作，
   两种registry预热顺序下恢复后继；两条超过1024回合的原始长局严格快照重放。
4. 报告区分旧2000身份与当前组合构建，本轮不冒充再次全2000通过；真实profile/live/trace/RF全条件/规模仍缺。
5. 门禁通过后封存组合提交，交付另一对话；不修改其冻结seed树/产物。

涉及engine/sim/state*、env/snapshot_validation.cpp、MonsterModel.cpp、生成schema、报告/路线图。
