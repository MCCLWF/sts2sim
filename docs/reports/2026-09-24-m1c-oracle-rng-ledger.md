# M1-c oracle 随机数抽取账本执行报告

日期：2026-09-24  
分支：`feat/oracle-rng-ledger`

## 前置条件

- 已读取并遵守仓库根目录 `AGENTS.md`。
- `STS2_SRC=/home/wst/Desktop/MCC/Slay the Spire 2/src/Core`，游戏程序集路径由 `tools/Directory.Build.props` 解析。
- 使用 `~/.dotnet/dotnet`（.NET 9）。

## 实现

- 新增 `OracleRngLedger`：通过 Harmony 补丁覆盖 `Rng` 的公开 `Next*` 和 `NextGaussian*` 重载，并记录流名、方法、规范化参数、结果、抽取后的计数器和调用者。
- `RunRngSet`、`PlayerRngSet` 构造完成后建立实例到流名的反向映射；战斗中按 `monster.<combat_id>` 补登记怪物 RNG，未登记的有种子 RNG 使用 `other`。
- 泛型 `NextItem<T>`、`WeightedNextItem<T>`、`Shuffle<T>` 不直接补丁；它们调用的 `NextInt`/`NextFloat` 会被记录。Gaussian 内部的嵌套抽取只输出外层一行，计数器差值保留实际消耗的抽取数。
- `combat` 新增 `--rng-ledger`、`--seeds`、`--characters`、`--policy-seeds` 和每场 `begin` 行；场间调用 `RunManager.Instance.CleanUp()`。
- 战斗状态补充怪物意图及攻击伤害/段数、手牌费用、遗物、药水槽位和充能球。
- 新增 `tests/l3/check_ledger.py`，按相邻状态的计数器差检查账本；`Rng.Chaotic` 不纳入账本，因为其种子来自墙钟时间且只用于非游戏逻辑表现/存档 ID。
- 删除 `Oracle.cs` 中不再使用的 `godotprobe` 命令。
- 更新 `README.md`、`docs/roadmap.md` 和偏离登记册。

## 验收命令和结果

### Oracle 构建

```text
~/.dotnet/dotnet build tools/Sts2Oracle -c Release
Build succeeded. 0 Warning(s), 0 Error(s)
```

### M1-c 批量账本

```text
~/.dotnet/dotnet run --project tools/Sts2Oracle -c Release --no-build -- \
  combat --seeds TEST123,TEST124,TEST125 \
  --characters IRONCLAD,SILENT --policy-seeds 1,2 \
  --max-actions 300 --rng-ledger
python3 tests/l3/check_ledger.py /tmp/ledger-accept.out
```

结果：12 场均输出 `end`；285 个 `state`；`ledger check passed: 285 states, 273 intervals`。

同一命令连续运行两次后 `cmp` 逐字节通过。

### 仓库合入检查

```text
scripts/check.sh
```

结果：C++ 构建成功，2 个单元测试全部通过，L2 `200 runs, 0 mismatched`，最终输出 `all checks passed`。

## 已知限制

- `Rng.Chaotic` 的非确定性抽取不输出；详见 `docs/audits/deviations.md` 的 O-007。
- 当前 headless combat 没有玩家选择器，Silent 的 `NEUTRALIZE`/`SURVIVOR` 在随机策略中跳过；详见 O-008。选择前缀机制完成后应移除该过滤。
- 本次工作只增加 oracle 和账本检查，没有实现 C++ 战斗重放或选择动作比对。
