# E0 严格采集合同与 coverage-v2 集成

## 前置与范围

- 基线 `5266daf`；独立分支 `test/seed-policy-contract-integration`，不修改正在进行规则修复或完整 Env/RF API 的工作区。
- 已读 AGENTS、design、roadmap、deviations、pitfalls，以及两项待集成任务的计划与报告。
- 集成 `4c2088a` 的实际 level/mode 能力合同与 `3c60126` 的原生策略覆盖、动作协议及历史策略兼容。
- 不改 Env、ABI、model 或 state；所有原始失败、trace 与其它工作区的产物保留。

## 工作项

1. 保留两项提交的文档、测试、Oracle/CLI、check.sh 全部有效修改。
2. 参数的显式请求先校验，再解析默认值：只有 `run L4` 使用策略版本并默认 coverage-v2；L3/replay 显式策略版本启动前拒绝，config.policy_version 为 null。原始 begin 策略身份不改写。
3. 保持 L3 的 HP/Neow/stop/limit/快照请求拒绝与实际无快照声明，L4 的 probe/starting-* 请求拒绝；版本变化续跑测试按 run/config 合同验证。
4. 在独立工作区构建 Oracle/CLI，串行限定协议 suite，然后固定同一源码/二进制验证两枚已冻结自然 seed × 五职业 × A0/A10 × policy 1/2，以及四条 coverage-v2 Silent 选择探针。
5. 保存命令、进程日志、manifest、原始 trace、首次差异与资格范围，写集成报告并提交。

## 验收与限制

- 限定 suite：`python3 -m unittest discover -s tests -p 'test_differential*.py' -q`；构建后不能跳过 CLI 协议用例。
- 正常 pilot 复用 `DF7AA96A3B6D3` / `DE1209365EF58` 作为固定策略整合回归输入，不宣称新抽取探索。
- 选择探针复用 POLICYV2CHOICE01/02 × policy 1/2，Silent A0、no-Neow、HP=10000、Driftwood/GamblingChip、首幕 Boss 前；单列为注入边界场景。
- 已知 Large Capsule 跨职业模拟器缺口由 root 的独立规则任务处理；本分支保留该预期失败，不改变策略、种子或比较字段。
- 不执行完整 main 合入门禁或大型 seed 批次，不宣称 E0/E3/M5 完成。集成后的完整检查及规则修复组合验收留给 root。
