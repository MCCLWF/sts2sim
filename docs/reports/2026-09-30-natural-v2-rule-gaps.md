# 8000 新自然身份与规则缺口

## 资格与结论

本树基底 `8d94dfa`，接入并行 API 的自动药水 discard 最小补丁 `15e3d84`（本树 `b6a8893`）。shared reward options、free-star、coverage-v2/harness 和 custom Rewards 自动完成合同均已先集成。属于 E3/M4 条件补覆盖；API 开发由另一聊天继续。

冻结版本在 **800 个新 run seed × 5 职业 × A0/A10 = 8000 个身份**上执行自然 HP/Neow/coverage-v2，每个身份独立且互不重复的 policy seed，与先前 200 个 run seed 不重叠。原始结果：**7934 identical、40 diverged、26 native_error；501097/503287 决策**，全部 8000 原版终点为 death@act0。批次退出1符合发现失败的合同，EOF 源码和运行时资格检查完成，summary 已生成。

原始 66 条失败在本次修复后严格重放 **66/66 identical，5084/5084 决策**，完整死亡终点与逐点 snapshot successor 均对齐。另 24 条已纠正采集合同的 custom Rewards 注入边界原件 **24/24、3738/3738**对齐，包括原第239点 Unmovable 分歧；这是 boundary 而非完整胜利。完整 `taskset -c 0-2 scripts/check.sh` 退出0：**48/48 CTest、L2 200/200、L3 81完整战斗/1625状态+2短探针/4状态、L4 103路线/18956决策**全部通过。

本提交尚未在修复版重新重放全部8000身份。不能用66条失败复验代替原7934通过身份的新版本复验。自然样本没有后两幕覆盖；深层仍需另批HP注入且单列资格。完整实机/API、逐抽取双端 RNG、全原生合法集合、任意容量上界和全条件覆盖均未认证。

## 7 个原始首因桶

分类来自首差异之前的实际 action，不能把 `first.context.action`（下一动作）当首因。原件和分类表保留；各桶数量如下。

| 原始首因 | 身份数 | 修复与 v0.111.0 依据 |
|---|---:|---|
| 自动药水丢弃 | 26 | 全为 FairyInABottle，允许 Discard、禁止手动 Use；沿用 API 聊天 `15e3d84` 最小补丁，不限制 Oracle 策略迁就错误 |
| TouchOfInsanity | 15 | `Models/Potions/TouchOfInsanity.cs` 的 `CostsEnergyOrStars(false) || CostsEnergyOrStars(true)`；补齐 CardModel 同名源 predicate，排除零/负费用；单一可选项由源 FromHand 直接选取 |
| Glitterstream | 8 | `Models/Cards/Glitterstream.cs` 先 `Hook.ModifyBlock` 算下一回合 block 再 GainBlock；Frail 下3，旧移植5 |
| Gigantification/Osty | 5 | `Models/Powers/GigantificationPower.cs` 检查来源卡所有者，Osty attacker 不应被排除；无捕获时 query 也×3；按 AttackCommand 身份消费，嵌套同卡/异卡不消费父攻击 |
| SneckoOil | 4 | `Models/Potions/SneckoOil.cs` 在 RNG 前排除 X 和本地负费用；Greed/AscendersBane 等不能额外抽取随机数 |
| Unmovable | 4 | `Models/Powers/UnmovablePower.cs` 数 BlockGainedEntry，而非独特 CardPlay/出牌次数；排除当前同次 CardPlay 的所有记录，与叠加 Amount 比较 |
| Inky | 4 | `Models/EnchantmentModel.cs` 初始化/克隆 canonical DynamicVars；CardCmd 首次附魔原本归零，现装载唯一 canonical 变量，Inky Weak=1；Adroit 后续仍按 Amount 重算 |

Unmovable 历史表示使用 POD 总数、round/side/playerTurn stamps 和 Clear epoch；临时 CardPlay 保持自身同 stamp/epoch entry 数，没有指针进入 GameState。读取源码所有 `BlockGainedEntry` 消费者，仅 Unmovable 会读此类。保留原版 `Move|Unpowered` 的计数资格，不按 Powered 自行过滤。extra-turn playerTurn 递增前的历史仍按源 stamp 可见，不能 StartTurn 无条件清零。压缩表示资格登记为 D-009；GameState 当前 **804552 字节**，保持 trivial-copy，不能沿用旧状态大小的性能宣称。

Gigantification 的原版私有 `commandToModify` 身份在临时 AttackCommand 上记录归属 PowerRef；正常 prefix replay 从 stationary base 重建该对象。POD 原 active/card 状态不保存堆对象地址。未认证任意中途 native capture 或跳过前缀重建的恢复。

## 原版 primitive 与回归

`tools/UnmovableChecks` 直接链接正版 DLL，`GameBootstrap.Init(testMode:false)`：**16 个格挡历史、10 个宠物/嵌套 AttackCommand 身份观察、1 个 Inky 初始变量**。C++ `seed_rule_primitives` 的期望来自以上原生输出，覆盖同卡不同次、多entry、叠加、null、不同props、side/round/playerTurn、Clear epoch、快照及后继、无 ModelSource 攻击、嵌套不消费、只消费一次。probe 退出0；cleanup 有原版空路径文件 IO 日志，未吞异常或改变数值。导出 JSON 行保留，source/DLL 不提交。

首轮55原件重放52完整一致、2崩溃、1Inky分歧。两条候选崩溃由 gdb 定位为 null ModelSource 的默认 kind=Card 被错误当实际卡，已加非空判断；Inky 初始变量修复后全部66完整通过。上述过程失败日志保留，不能算作通过。

```bash
~/.dotnet/dotnet build tools/UnmovableChecks/UnmovableChecks.csproj -c Release -o .local/unmovable-oracle
~/.dotnet/dotnet .local/unmovable-oracle/UnmovableChecks.dll
cmake --build build --target sts2sim_cli test_seed_rule_primitives -j3
build/test_seed_rule_primitives
python3 tools/differential/batch.py replay --out /path/to/new-output --jobs 3 --traces /path/to/original/oracle.stdout.jsonl --cli build/sts2sim_cli
taskset -c 0-2 scripts/check.sh
```

## 执行动作审计

8000原版 captures 实际执行 combat play276604、end114025、use potion2684、discard potion2564；Silent Neutralize/Survivor/Defend 的执行均已非零。非奖励选择数量0..5都实际执行，空选非空池756次，中间合法数量183次。奖励Skip实际2302次，出现9311次；**本自然批没有 REROLL/SACRIFICE 实际执行证据**。C++ 首差异前已对齐后继单独计数，不能把原版失败后的 action 计作 C++ 覆盖；对应全量审计 JSON 保留。

并行 API 审计发现 headless CardReward.OnSelect 保留初次 alternatives，而原版 UI RefreshOptions 会在第一次重抽后移除 REROLL。当前 coverage-v2 直接从传入 alternatives 采样，不能证明刷新后合法性。由 API 聊天处理真实合法动作和专用 native 矩阵，本轮不改变冻结 driver，资格补充为 O-017。不能以“两个无头端相同”当真实 UI 正确。

## 本地产物与指纹

证据根：`/home/wst/Desktop/MCC/sts2sim/artifacts/legacy/mass-seeds-2026-09-30`。原始 `natural-v2/manifest.json/results.jsonl/summary.json/jobs`；失败输入列表 `natural-v2-all-failure-inputs.json`、首因 `natural-v2-first-causes.json`；复验 `natural-v2-all-fixed-replay-r2`；custom边界 `custom-boundary-rule-fix-replay`；probe `seed-primitives-native-r3.stdout.jsonl`；执行审计 `coverage-audit-v2/natural-v2-coverage.json`；完整门禁 `natural-v2-rule-gaps-check.log`。每个 job 保留原输入/stdout/stderr/process command/exit及SHA256，不修改原件。

| 对象 | SHA256 |
|---|---|
| 原始冻结源 | `7fb8e781b1c4de7ca1ff3c57a3b189f7221d270f1f7ae07ca66f042cfda31171` |
| 原始 CLI | `6a64e1f6e634a05769e7c707a6d25fa90110ec4506217b7ed7c49a7e8b31b6e9` |
| 原始 Oracle | `0dde35b88ef3ddc8e4b857a857c7a9a23314c1f9149d4bf0eda6b28181a72330` |
| 原始 manifest | `4b3fd2059061a7af0d5d9aa20e4c119a2c300f59dc6aedf5bc62978cdc5fb025` |
| 66条修复复验源 | `fef6e11c324f86185f9b6b3bb386840f0e45bae8bb2904109289f14760fee11b` |
| 修复 CLI | `e515cba636b2145815fd23f0c45bd58088317fa5ec793e0ba5c829900fb281e4` |
| 完整门禁日志 | `d64901c8181ec301ef307259079d775c94cb7f799bdfbef4fb016704b4eaee11` |

旧2000身份基线的3容量失败仍未解决（1023活卡池两条、1024怪物日志一条）。没有删活牌、补抽RNG、缩比较字段或截断源历史来制造通过。完整 API 路线、真实profile/live capture由并行聊天继续；本提交只交付已测的规则补齐。
