# E0 严格采集合同与 coverage-v2 集成报告

## 范围与结果

- 基线 `5266daf`；独立分支 `test/seed-policy-contract-integration`。保留并集成原 `4c2088a` level/mode 合同与 `3c60126` 策略/动作协议、测试、check.sh 和文档。
- 本集成未修改 Env、ABI、model 或 state；完整 API 和规则修复仍在各自独立工作区进行。
- 冲突解决后限定协议 suite 串行 48/48 通过，无跳过。自然固定 pilot 为 32 identical、8 既有 Large Capsule 分歧；四条 coverage-v2 选择探针为 4/4 identical。
- 两批均在已提交 `15677e8df22bf4c7b23c3e4902b900029f57710f`、干净源码和独立 CLI/Oracle 上执行，首尾指纹相同。失败未替换种子、改策略或放宽比较。

## 组合合同

`--policy-version` 的 parser 默认 None，以区分显式请求。只有 `run --level l4` 解析默认 coverage-v2，并进入 config/job 身份及原版 begin 版本要求。L3 和 replay 显式指定 legacy-v1 或 coverage-v2 均在创建输出、runtime 查询或启动进程前 exit 2；两种模式的 config.policy_version 为 null。独立 L4 脚本的 `--trace` 同样拒绝显式策略版本。

L3 保留 HP、Neow、stop、L4 limit 的拒绝，并拒绝其不执行的 `--no-verify-snapshots`；manifest/summary 的 verify_snapshots=false，不声明后继校验。L4 保留 probe、starting-energy/stars 和 L3 limit 的拒绝及真实快照开关。

trace 的 begin 不被改写，legacy/coverage 身份由原始输入保留；新采集 require_policy_version 校验保持。策略变化续跑测试改为 run/config 合同：默认 v2 成功，原配置 resume 不重复采集，改 legacy 在采集前拒绝。新增数据驱动合同覆盖两版本的显式拒绝、默认 v2、原始身份与采集版本不匹配失败。

## 命令与原始证据

证据根：`../artifacts/legacy/seed-policy-contract-integration-2026-09-30/`；原始产物不提交 Git。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target sts2sim_cli -j 2
~/.dotnet/dotnet build tools/Sts2Oracle -c Release -v quiet -nologo
python3 -m unittest discover -s tests -p 'test_differential*.py' -q
python3 tools/differential/batch.py run --out "$OUT/natural-fixed-v2" \
  --seeds-file "$OUT/frozen-seeds.json" --policy-seeds 1,2 --jobs 2 --timeout 60
python3 tools/differential/batch.py run --out "$OUT/focused-choices-v2" \
  --seeds-file "$OUT/frozen-choice-seeds.json" --characters SILENT --ascensions 0 \
  --policy-seeds 1,2 --jobs 2 --timeout 60 --no-neow --hp 10000 \
  --relics DRIFTWOOD,GAMBLING_CHIP --stop before-act1-boss
```

Oracle 构建成功，0 error / 1 个既有 nullable warning；CLI 构建成功。首次 suite 在 CLI 尚未构建时 `48 tests OK (skipped=1)`，构建后最终 `Ran 48 tests in 3.432s / OK`，日志 `protocol-final.log`。

| 批次 | 结果 | 资格与终点 |
|---|---|---|
| natural-fixed-v2 | exit 1；32 identical / 8 diverged；1971/2564 决策 | DF7AA96A3B6D3、DE1209365EF58 × 五职业 × A0/A10 × policy 1/2；正常 HP/Neow；40 条原版均第一幕自然死亡，32 条一致的完整终局。两枚 seed 是固定策略集成回归，不是新抽取探索 |
| focused-choices-v2 | exit 0；4 identical；1677/1677 决策 | POLICYV2CHOICE01/02 × Silent A0 × policy 1/2；HP/relic/no-Neow 注入，首幕 Boss 前边界；0 完整整局 |

自然失败均为 DF7AA96A3B6D3、policy 2、Silent/Defect/Regent/Necrobinder × A0/A10。代表 `d1e9f1df550a4b7aba40` 的首分歧在 index 1 / map / `$.run.deck[12]`，原版 STRIKE_SILENT，模拟器 STRIKE_IRONCLAD。与原策略任务报告同一 Large Capsule 缺口，由独立规则任务修复，本集成保留其原始失败。

两批公共指纹：

- source SHA256 `2d3cf28d3a37492f4e8fca3c3e026f3f1afaf3464a0ca974ea488070bff04dc7`
- CLI SHA256 `4bb96a6218f0fece798bd61ebf29a75dc97d997ee6aff8120272425bfb9a55dc`
- Oracle SHA256 `c5ce13001ace062acbdabc7c661069328a1ea49efaab2e3bf981c6f8fcc939ff`
- natural manifest SHA256 `d60cd9b13a7f207f9e1ade3b76e3788e735bd508d97cce885e13381be3dfccc3`
- focused manifest SHA256 `d1d9b798550cb6c40c18454baef841b7c52a2626a590a4cf986b6fb3efc6ec2c`

## 限制与后续发现

未执行完整 scripts/check.sh 或大规模新 seed 验收；本报告不能替代 main 合入门禁、原生合法动作全覆盖、方法/条件覆盖、双端 RNG 抽取或万局认证。TestMode/headless 与实机资格限制沿用原记录。

两批完成后，API 并行任务指出 custom Rewards 全取完应自动关闭，两端旧逻辑共同增加了额外 Proceed。已只读核验 RewardsSet.Offer、RewardsSetSynchronizer、NRewardsScreen.UpdateScreenState：非 CombatRoom 奖励集在全部 SuccessfullySelected 时自动结束，普通 CombatRoom 仍保留 terminal Proceed。该采集合同修复及模拟器组合验收必须另存修前/修后证据；以上批次保持修前身份，不能因旧两端 identical 宣称已验证这条源规则。
