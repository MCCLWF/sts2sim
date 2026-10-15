# M5：通用卡牌事实与 Mod 可打出政策

分支 `feat/combatsolver-env-backend`，接续 Mod 历史投影。原算法完整 Env 后端仍未开启，固定 upstream 不修改。

1. getter 协议升级 v4，卡牌输出未按搜索筛选的游戏字段与全部变量类型/BaseValue；C# Mod 负责特殊字段/附件/字符串覆盖拒绝及 Derived 筛选，原指纹编码和对拍判据不变。
2. 删除 C++ `CanPlayCard_core` 实现和对应 C ABI；Mod 的公共 `CapturePolicyProbe` 依据通用 getter 的资源、费用、关键词和原始拒绝原因实现原规则，Enthralled、Smog 与 X 费用政策均在 C#。
3. 球查询协议 v2 改为通用游戏 getter 语义；完整原键声明由 Mod 维护，C++ 不命名原 AppendOrbs。
4. 原游戏 getter、原 Beam 指纹、原 CanPlayCard/费用、并发/Dispose/未移植隔离、C ABI 验证；scripts/check.sh 完整门槛，更新报告/README/roadmap。
5. 计算型变量原终局读取政策仍需迁出，随后补全历史/策略账本、状态键、评分、原 Beam Env 执行和最终游戏验收；本阶段不得宣布最终目标完成。
