# 严格对拍批次的 level 与验证能力合同

## 目标与前置

- 基线 `5266daf`，独立分支 `fix/differential-level-contract`。
- 已在原基线单例验证：L3 `--hp 10000` 返回 identical，但实际 HP 为 80，manifest 同时声明未执行的快照校验。
- 批次启动前拒绝其 level/mode 无法实现的配置，避免参数只写入 manifest 而没有参与原生执行。
- L3 只声明其实际执行的状态/选择/RNG 计数差分；L4 保留可选逐决策快照及后继校验。

## 工作项

1. 分离 L3/L4 的有效默认终点和执行上限；明确拒绝不支持的 level 参数。
2. 拒绝 replay 模式中没有应用的采集配置，保留 L3 历史输入实际使用的回退配置。
3. 修正 manifest 的 L3 `verify_snapshots` 及 summary 比较范围。
4. 增加数据驱动工具合同测试，覆盖拒绝发生在输出目录和原生进程创建前。
5. 执行限定工具协议套件及小型正版 Oracle 探针，保存报告并提交。

## 验收与范围

- `python3 -m unittest discover -s tests -p 'test_differential*.py' -q` 全部通过。
- L3 `--hp 10000` 必须在启动前失败，不生成新的 manifest 或采集产物。
- 合法 L3 单例与 L4 小终点单例对拍；实际 manifest/command/summary 的快照声明相符。
- 不修改 Env、ABI、OracleRun、策略；不做完整大型门禁或大批 seed。
- 旧 L3 trace 缺少升阶等输入的全面资格校验、双端 RNG/合法动作/执行覆盖继续列为限制，不能由本修复宣称完成。
