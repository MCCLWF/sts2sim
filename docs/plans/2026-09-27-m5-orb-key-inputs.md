# M5：原 Beam 充能球键输入

分支：`feat/combatsolver-env-backend`。前置提交 `73f21c4`。完整原 state key、政策/评分与正式 Env 执行仍未完成。

## 目标与工作

1. 对照固定原 `CombatBeamSolver.AppendOrbs` 和 `OrbMirrors.GetPassiveValue/GetEvokeValue`，审计五种球的查询、Focus、Dark/Glass decimal 字段、队列容量/顺序。
2. 新增独立 owned C++/C ABI/managed 球查询；从私有副本调用已移植 getter，检查完整状态纯度，未知球/Hook 拒绝，不从 native 预先灌入计算结果。
3. 用固定 upstream 的 StateFingerprintBuilder 原编码实现队列部分指纹，供 EnvSearchBranch 读取。完整原状态键/历史/执行门禁继续关闭。
4. 三组真实根覆盖五种球、重复实例、容量与球数不同、Focus 正负与 clamp、Dark/Glass 小数内部值；逐位核对正版 getter/固定原 OrbMirrors，并反射调用实际 AppendOrbs 比较最终部分指纹。不复制测试版编码作为参照。
5. 验证 managed/raw ABI 的 owned、并发、源/clone 隔离、失败和释放；完整 scripts/check.sh，通过后写报告并更新现状。

## 验收边界

充能球部分指纹一致不代表完整 state key 可用；必须保留 complete=false。已移除球的历史池、任意 live 根、全部 Hook/风险及正式后端尚待各自验收。既有 full-live 两回合和原 Beam 12 场工具继续保留。
