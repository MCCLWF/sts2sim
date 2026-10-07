# 严格对拍批次工具

复用现有正版 DLL oracle 和 C++ CLI。每个采集 job 独立启动 oracle，每个重放独立启动 CLI；默认 4 个并行进程槽，避免原版 singleton/静态状态跨局污染。不改变 RNG、策略或比对字段来迎合模拟器。

先构建 `build/sts2sim_cli` 与 Release `tools/Sts2Oracle`。从仓库根运行：

```bash
# 开始前抽取并冻结 10 枚新种子 × 五角色 × A0/A10，共 100 条。
# 正常 HP，默认 Neow；自然死亡或胜利为完整终局。
python3 tools/differential/batch.py run --out artifacts/diff/natural-001 --count 10 --jobs 4

# 提高 HP 是测试辅助，必须另建批次；不能作为正常游戏分布的样本。
python3 tools/differential/batch.py run --out artifacts/diff/long-001 --count 10 --hp 10000 --jobs 4

# 显式保留历史策略轨迹；新采集默认 coverage-v3。
python3 tools/differential/batch.py run --out artifacts/diff/legacy-001 --count 10 --policy-version legacy-v1

# 同一源码、二进制、输入和配置下恢复；已记录的失败也不会被重抽/覆盖。
python3 tools/differential/batch.py run --out artifacts/diff/natural-001 --count 10 --jobs 4 --resume

# 重用历史原版采集，不必再启动 oracle。
python3 tools/differential/batch.py replay --out artifacts/diff/regression-001 --trace-dir /path/to/captures --jobs 4

# 只测试两个具体 trace；L3 也可用 --level l3。
python3 tools/differential/batch.py replay --out artifacts/diff/focused-001 --traces /path/to/a.oracle.jsonl /path/to/b.oracle.jsonl

# 离线批次恢复使用已冻结 jobs；已复制的输入不依赖外部原始路径继续存在。
python3 tools/differential/batch.py replay --out artifacts/diff/regression-001 --jobs 4 --resume
```

`--seeds` 接受逗号列表；`--seeds-file` 接受 JSON 字符串数组或每行一个种子。输出目录必须专属于该批；不存在“覆盖已有批次”的模式。旧 L3 trace 没有记录升阶、注入能量等配置时必须通过选项显式提供，不能猜测；新采集已在 begin 中记录这些输入。

`run --level l4` 默认 `coverage-v3` 使用独立策略 RNG：包含所有原生可打卡牌（含无目标牌和 Neutralize/Survivor）、战斗药水使用/丢弃、所有合法多选数量和原生奖励 alternatives（含 Skip/Reroll）。v3绑定正在领取的原生CardReward，依据其CanReroll排除重抽后缓存中仍残留的REROLL；每个奖励动作记录native_reward_can_reroll与legal_reward_indices，解析时缺失或选中不可用项均拒绝，不改变被比较的游戏状态字段。`coverage-v2`保留旧headless缓存采样和RNG轨迹，其第二次REROLL不具备真实UI合法性；`legacy-v1`保留更早策略的遗漏及RNG调用。版本写入原版begin与采集manifest的config/job身份，实际捕获版本不符或续跑改版本均拒绝；没有版本字段的旧trace按历史策略接受离线动作回放，不能冒充新策略覆盖。`--policy-version`仅控制L4新采集，L3或replay显式指定任一版本均在启动前拒绝；这两种模式的manifest config.policy_version为null，原始trace.begin的策略身份完整保留。独立L4脚本的`--trace`同样拒绝显式策略版本。旧v2轨迹不自动升级、不改动作来绕过API的禁用选项。

新 L4 combat 动作编码为 `use_potion`（slot、target）和 `discard_potion`（slot），回放文本分别为 `potion <slot> <combat_id|-1>`、`discard <slot>`；旧 play/end/select 编码保持兼容。药水 Self 的原版目标是玩家 CombatId，null 只用于原版允许的无生物目标药水。

精确重采不同角色/升阶的历史病例可用 `--cases-file cases.json`，格式为 `[{"seed":"A","character":"SILENT","ascension":10,"policy_seed":1}, ...]`；只执行这些身份，不做角色与种子的笛卡尔展开。修复采集端后用新目录重采；旧失败真值仍保留，不能当作修复后引擎的验收数据。

L4 默认开启每决策快照恢复和后继校验，`--no-verify-snapshots` 可用于分诊普通执行与恢复失败；L3 的原生战斗重放不支持该校验，manifest 与 summary 的 `verify_snapshots` 固定为 false，比较范围不宣称快照后继。默认进程超时 180 秒，可用 `--timeout` 调整。源/二进制/配置/输入变化需要新目录；恢复会核对已记录原始文件和结果的 SHA256，一旦污染就拒绝继续。源码或二进制在批次执行中变化会拒绝资格，保留原始证据。

参数必须与实际执行的 level/mode 相符，否则在创建批次目录和启动原生进程前明确拒绝。L3 不支持 `--hp`、`--no-neow`、`--stop`、`--max-decisions` 或 `--no-verify-snapshots`；L4 不支持 `--encounter`、`--starting-energy`、`--starting-stars`、`--probe-*` 或 `--max-actions`。L3 的 `--probe-helper` 与 `--probe-after-card-turn` 需要 `--probe-cards`。L4 默认终点为 victory、决策上限 10000，L3 默认动作上限 300。

replay 使用冻结 trace 的身份与动作，不接受 seed/角色/升阶/策略筛选、采集上限、HP、Neow、stop、encounter 或新 probe 配置；L4 的牌组与遗物也从 trace.begin 读取。L3 仅保留其原生重放实际支持的历史 trace 配置回退（deck/relics/energy/stars），并优先使用 trace.begin 中的已记录值。`--cases-file` 固定全部身份，不能同时指定 count/角色/升阶/策略展开。

原生采集通过 Oracle 的只读 `runtime-provenance` 命令解析实际外部目录：冻结 GameData 顶层全部文件（含游戏程序集、本地库）与英文语言表的清单及 SHA256；缺必需程序集/玩法语言表时拒绝采集。目录、清单或内容改变会拒绝续跑，批次首尾检测到差异也使整批失去资格。旧批次只记录 Oracle 输出目录时，缺失的外部依赖指纹不能事后追补为当时证据；离线回放仍保留原捕获资格，不启动 Oracle 查询。

SIGINT/SIGTERM 会停止排队任务、清理执行中的进程组并保存已读取的原始字节；嵌套 L4 matrix supervisor 先收到终止通知以清理自己的独立子进程，超时后仍未结束则强制停止。UTF-8 损坏明确失败，落盘原始 stdout/stderr 不做替换。恢复会核对系统身份，`.def` 等编译输入也进入源码指纹。未执行的 job 仍需原始输入；中断 job 的已复制输入可从归档恢复。

## 结果和判据

- `manifest.json`：所有预先固定 job、源码文件 hash、Git 身份/dirty 状态、CLI、Oracle 输出目录 DLL/runtime 配置与实际外部 GameData/语言表指纹、.NET host hash/已安装 runtime、相关环境变量、完整采集配置。
- `jobs/<id>/oracle.stdout.jsonl`、`oracle.stderr.txt`、`oracle.process.json`：原始真值和采集命令/退出码。离线重放的原始捕获版本未知时，明确记为 legacy，不冒充当前 oracle hash。
- `replay-*.stdout.jsonl`、`replay-*.stderr.txt`、`replay-*.stdin.txt`、`replay-*.process.json`：模拟器输出、动作、实际命令和进程诊断；崩溃与超时仍保存。
- `result.json`、追加的 `results.jsonl`：每条输入内的所有 run、实际匹配前缀、状态和第一个分歧。原始结果均保留，失败也算计划 job。
- `summary.json`：planned/finished、完整终局数、自然/注入标记、终点幕/原因、状态计数、实际比较前缀；决策数不能替代对局数。
- `triage.json`：按状态、决策类型与规范化字段路径/错误聚类，保存确定的代表种子和首次位置。桶是排查线索，不宣称同字段的所有分歧都已证明同一源码根因。
- `interrupted/`：未写入 results 的中断产物，包括完成落盘但来不及追加的结果，续跑时原样归档后重试。

`identical` 仅表示声明范围内的输出投影与终点一致。`probe_identical` 是显式单卡/药水探针，不计完整战斗；Act 边界不计完整整局。`unported`、`truncated`、`oracle_error`、`native_error`、`invalid_trace`、`timeout`、`tool_error` 默认全部失败。`--explore` 仅允许 Unported 退出 0，仍单列，不算 identical；截断和其它失败在探索模式也不能通过。

L3 拒绝空/残缺普通战斗，检查状态/动作/选择数量并按实际观察顺序比较。L4 拒绝空/残缺轨迹、未答决策、终点/stop 不符；`max_decisions` 只能标截断。模拟器 signal/exit 先作为进程错误识别，半行 JSON 不会误报 oracle 无效；完整输出前缀仍可比较。后续 Unported 或进程错误不能掩盖更早的实际规则分歧。

## 当前限制

比较范围为 oracle 已导出的状态、选择候选、RNG 流计数以及快照后继。双端逐抽取 RNG、完整原生合法动作集合、hook/条件执行覆盖、guided 探索和自动最小化尚未接入。原版 TestMode 与无头表现补丁的真机资格仍有限。提高 HP、替换牌组/遗物与显式遭遇必须标为注入场景，不能当自然可达分布或正常通关率。

外部文件在批次首尾及续跑时采样，未采用不可变副本，不能排除中途变更后又恢复的情况。语言表记录实际完整清单并要求八张核心玩法表，未强制整个官方语言包清单；.NET shared framework 记录 host hash 与安装版本/路径，尚未逐程序集 hash。

当前工具的进程组清理和输出锁使用 POSIX（Linux）；未做 Windows 验收。原始产物不提交 Git，正式报告记录本地证据路径；需长期协作归档时另作明确发布任务。
