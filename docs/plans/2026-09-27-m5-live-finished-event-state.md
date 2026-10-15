# M5：普通战斗根中的已完成事件

分支 `feat/combatsolver-env-backend`，前置 `ccb0efd`。当前实机自然根在 `EventSynchronizer._events` 的 1 条 Neow 记录处拒绝。该列表在离开事件时不会清空，下一次 BeginEvent 才替换；不能把它等同于仍有待执行的事件。

1. 通用 POD 增加单人事件同步器的列表/纪元页/投票事实，C++ BeginEvent 和选项入口按原顺序更新。已存在的 EventState 保存实际 Neow 当前字段。
2. Mod 在普通独立战斗边界捕获已完成且已清理、空当前选项的 Neow 实例及同步器；其他事件/闭包、待执行任务、未清理或 event combat 状态仍拒绝。Ancient 的生成选项/对话缓存仅在完成且下一次事件开始会替换的边界不影响未来。
3. assembler 覆盖真实事件槽及同步器，保留 seed-free scratch 的明确默认证明。原搜索政策留在 Mod，部分档案预测门禁不变。
4. 用 TestMode=false 的正版 Neow BeginEvent/完成/离开语义生成期望，导入并比较后续战斗不改遗留状态。重跑既有完整根、API、完整回归与隔离实机，检查导入是否移动到下一实际拒绝点。
