# 批次 level/mode 与快照资格合同修复

## 基线与结果

- 基线 `5266daf`，独立分支 `fix/differential-level-contract`；未改 Env、ABI、OracleRun 或策略。
- 批次参数现在按实际 adapter 能力校验。不支持的配置在创建输出目录、查询 runtime 或启动原生进程前返回 exit 2。
- L3 的 manifest/summary 明确 `verify_snapshots=false`，比较范围只声明实际执行的状态、选择与 RNG 计数差分。L4 的声明跟随实际 `--verify-snapshots` 开关。
- 限定协议套件最终 43/43 通过；正版 L3 一例 11/11 状态、L4 一例 49/49 决策 identical。没有执行大型 seed 任务或完整 `scripts/check.sh`。

## 已证实的首因

修复前从原基线执行：

```bash
python3 tools/differential/batch.py run --level l3 \
  --out /tmp/sts2-harness-audit-l3-hp-20260930 \
  --seeds HARNESSAUDIT01 --characters IRONCLAD --ascensions 0 \
  --hp 10000 --jobs 1 --timeout 30
```

原始结果为 exit 0、identical、11/11 状态。manifest 记录 `hp=10000`、`verify_snapshots=true`，但 oracle begin 不含 HP，实际首个状态为 HP/max HP 80/80，结果 scope 为 natural；实际 replay 命令也没有快照验证。

原因是 `batch.py` 的共享 parser 接受这些值，`tests/l3/l3_combat.py::oracle_command/evaluate_runs` 没有应用 HP、Neow 或快照选项。反向的 L4 也接受而忽略 encounter、energy、stars 与 card probe 等参数。replay 的身份筛选、采集上限及 L4 loadout 覆盖同样没有实际应用。

## 修复范围

- `tools/differential/batch.py::validate_execution_options`：L3 拒绝 HP/Neow/stop/max-decisions；L4 拒绝 encounter/energy/stars/probe/max-actions。L3 helper/after-card-turn 必须伴随 probe-cards。
- replay 拒绝没有执行的采集配置；其身份与动作来自冻结 trace。L3 保留已有实际可用的 deck/relics/energy/stars 历史输入回退。
- cases-file 已固定身份，拒绝同时指定 count/角色/升阶/策略展开，避免请求悄悄失效。
- 默认 stop/limit 在 level 校验后解析；L3 不再记录 L4 默认 stop 或决策上限，L4 不再记录 L3 默认动作上限。
- `write_summary` 随实际验证配置声明 comparison，新增显式 `verify_snapshots`；L3 manifest 固定 false。
- 新增 6 个数据驱动合同测试，包含零值与空 helper 的明确请求，检验拒绝发生在任何输出或原生进程启动前；另核验 L3/L4 正常资格与 replay 实际命令一致。

## 验收命令与原始结果

从本分支执行同一 L3 `--hp 10000` 命令，换新输出目录 `/tmp/sts2-harness-contract-rejected-l3-hp-20260930`：

```text
returncode=2
out_created=false
batch.py: error: L3 does not support --hp
```

本次复用未改动的基线 CLI 与 Release Oracle；本地 `build/sts2sim_cli` 是指向基线 CLI 的符号链接（未提交），工具解析后的实际路径、SHA256 与外部运行依赖均进入各探针 manifest。合法正版探针：

```bash
python3 tools/differential/batch.py run --level l3 \
  --out /tmp/sts2-harness-contract-l3-final-20260930 \
  --seeds HARNESSAUDIT01 --characters IRONCLAD --ascensions 0 \
  --jobs 1 --timeout 30 \
  --oracle /home/wst/Desktop/MCC/sts2sim/tools/Sts2Oracle/bin/Release/net9.0/Sts2Oracle.dll

python3 tools/differential/batch.py run --level l4 \
  --out /tmp/sts2-harness-contract-l4-final-20260930 \
  --seeds HARNESSAUDIT02 --characters IRONCLAD --ascensions 0 \
  --stop before-act1-boss --jobs 1 --timeout 30 \
  --oracle /home/wst/Desktop/MCC/sts2sim/tools/Sts2Oracle/bin/Release/net9.0/Sts2Oracle.dll
```

```text
L3: exit 0; identical 1; matched/total 11/11; natural 1;
    verify_snapshots=false; complete_full_runs=0;
    comparison=exported state/choice/RNG counters;
    manifest SHA256=0771454ac09562599344b348313654d71979a161c879eab0700f11868405f088
L4: exit 0; identical 1; matched/total 49/49; natural 1;
    endpoint=death@act0; complete_full_runs=1; verify_snapshots=true;
    manifest SHA256=9956d6132b4e16c8532d5520473e7c2dc610193a2baf3c750dbf7f7ff43462c0
```

L4 的自然死亡为完整终局，没有到达请求的 Boss 边界；不能当作三幕胜利或深幕覆盖。

```bash
python3 -m unittest discover -s tests -p 'test_differential*.py' -q
```

```text
Ran 43 tests in 8.523s
OK
```

并行执行最终 Oracle 探针时另一次同命令出现 1 个既有时序用例失败：nested supervisor 的 0.5 秒超时先于 sleeping CLI 创建 marker。该轮为 `Ran 43 tests in 20.745s / FAILED (failures=1)`；随后单独执行上面的限定 suite 全通过。未修改该用例或超时来放宽判据。

## 批量执行与首分歧复现

新 seed 应在独立冻结源码/二进制基线上分批执行，例如每批 `run --count 100 --jobs 4` 为五角色 × A0/A10 的 1000 个计划 job。正常 HP 与 `--hp 10000` 的 L4 深幕注入分别用独立目录。开发聊天继续编辑另一 worktree；本批运行期间不得改本基线或重新构建其正在使用的 CLI/Oracle。修复后另建输出目录 replay 历史捕获；源码变化后不能在原批次上 resume。

先读取 triage 代表 job 的 result，使用其 `replay-00000.process.json` 中的实际 command 与 `replay-00000.stdin.txt` 原动作重放。若首分歧在 L4 `first.index=i`，最短能显示该 decision 的动作前缀为 **i+1 条**：CLI 在耗尽动作时先输出 end，才走当前 decision 的 emit。仅取 i 条动作看不到分歧状态。保留原 trace，不把截断后的诊断输入计入完整终局。

## 限制

- 旧 L3 trace 缺升阶时，现有 adapter 仍会回退到 A0；批次没有新的 legacy-ascension 选项。本修复不宣称这些缺元数据输入取得当前完整采集资格。
- 源码与运行依赖仍只在首尾/续跑采样，不能检测中途修改后又还原；.NET shared framework 未逐程序集冻结。
- replay 输入的原捕获版本仍标 legacy/offline；未自动导入旁边旧 manifest 的原始捕获 provenance。
- comparison 只覆盖已导出的状态、选择与 RNG 计数及 L4 的快照后继。双端逐抽取 RNG、原生完整合法动作、方法/条件覆盖、guided 探索和自动最小化尚未认证。
- TestMode/无头适配与真实客户端的资格边界沿用已有偏离登记，不能以 seed 数量替代真机验收。
