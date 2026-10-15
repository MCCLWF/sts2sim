# M5：通用能力事实与 Mod 搜索账本

分支 `feat/combatsolver-env-backend`，前置 `542febc`。按用户要求，Env 保持通用游戏模拟器，CombatSolver 复刻在派生 C# Mod。原个体能力键已对拍，完整原 key/评分/执行仍未完成。

1. 撤回未提交的 C++ root/retired 槽位账本；改为通用能力 amount/attach/remove 事实。外层预测协议 v6 保存事实与选择重放基线；不写普通 GameState。
2. 通用能力查询输出原始 amount-on-start/duration 标志和全部变量的类型、数值及子类事实。C++ 不选择原搜索语义字段、不保存原退休槽位、不执行指纹编码。
3. C# Mod 负责原字段筛选、能力 P 聚合与 r 集合；分支独立持有根槽位配置，从 Env 事实派生退休记录。Clone/Predict 不调用原生未来效果。
4. 使用固定原 GetPowerFingerprint/聚合核对，并增加实际原 Beam 的根 Strength 归零及重新获得场景。验证选择重放、源/兄弟隔离、协议严格校验。运行完整 scripts/check.sh，记录原始结果。
5. 更新报告、README、路线图。登记尚待迁移的旧 card/policy/history 查询；正式执行 gate 保持关闭直到完整原算法闭包证明完成。
