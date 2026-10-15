# 静默猎手 v0.111.0 完整移植报告

日期：2026-09-28。分支：`feat/silent-hunter-v0110`。目标源码：STS2 v0.111.0 `src/Core`。

## 源码清单与完成状态

- 清单：[silent-source-inventory.md](../../audits/silent-source-inventory.md)，由 [silent_source_inventory.py](../../../tools/coverage/silent_source_inventory.py) 从导出的原版 `data/content.json` 与 v0.111.0 C# 源码生成。
- 覆盖 `SILENT_CARD_POOL` 的 91 张卡（含单人模式会过滤的多人专属卡）、8 件遗物、3 瓶药水，以及生成的 Shiv、Inky、关联 Power 和依赖模型。闭包共 154 个模型：92 CardModel、46 PowerModel、9 RelicModel、3 PotionModel、1 EnchantmentModel 和 3 个池模型。
- 清单逐项列出全部 667 个源码 override 和 314 个控制流检查点（条件、短路、循环、switch 等，含继承 override）。未解析的模型引用、缺失源码、缺少具体模型/池数据项和 `Unported` stub 均为 0。
- `tools/gen/gen_dispatch.py` 对源中每个可分派 override 要求 C++ 具体实现声明；只允许可证明由等级 delta 表完整表达的纯 `OnUpgrade` 例外。多人约束、池 epoch gate、所有单人可达内容由 `silent_source_pools` 覆盖。
- 源码逐项核对状态：实现完成；没有已知未处理的 Silent 模型、override 或分支。具体行为实现位于相应 `game/Models/` 模型、共享 Hook/命令和内容池过滤中；动态公式集中在 `game/Localization/DynamicVars/DynamicVarSet.cpp`。

## 验证

### 构建与全仓库差分

完整门禁命令：

```sh
STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core' scripts/check.sh
```

仓库脚本中的 map inventory 使用固定 sibling source path，因此运行时在 `/home/wst/Desktop/MCC/sts2sim-wt/Slay the Spire 2` 临时建立了指向源码安装的 symlink；门禁结束后移除。第一次直接执行在 map inventory 处因缺少该 sibling 路径退出，原始环境诊断保留在 [check-default-source-failed.log](check-default-source-failed.log)；最终完整执行结果在 [check.log](check.log)：18/18 CTest、smoke 和 Oracle build 通过；L2 为 200/200 runs；L3 为 80/80 fights、1,611/1,611 states；L4 为 98/98 runs、16,191/16,191 decisions，所有差分均为 0 divergence、0 unported。Oracle build 报告一条现有可空性警告，位置为 `tools/Sts2Oracle/OracleRun.cs:293`。

### 静默猎手定向原版 Oracle

`tests/l3/l3_combat.py` 使用 Oracle 生成原版 JSONL，然后从这些 trace 重放 C++ Env。所有差分为 0 divergence、0 unported；汇总 22 场、399 个状态完全匹配。每个 `*-l3.txt` 保存原始逐场摘要，`docs/audits/raw/silent-v0110/` 保存 Oracle JSONL 与无损 Base64 stderr（用 `base64 -d <file.stderr.b64` 解码）：

| 轨迹 | 验证内容 | 结果 |
|---|---|---:|
| [nightmare](../../audits/raw/silent-v0110/nightmare-oracle.jsonl) | Nightmare 目标卡选择和复制；另以 Env 快照测试验证待选状态恢复 | 1 场 / 6 状态 |
| [cunning](../../audits/raw/silent-v0110/cunning-oracle.jsonl) | Cunning Potion 生成 Shiv | 1 / 17 |
| [shiv](../../audits/raw/silent-v0110/shiv-oracle.jsonl) | Fan of Knives、多敌、Shiv 生成与全体目标 | 1 / 21 |
| [poison](../../audits/raw/silent-v0110/poison-oracle.jsonl) | Poison Potion、Poison、Accelerant、Bouncing Flask | 1 / 9 |
| [discard-relics](../../audits/raw/silent-v0110/discard-relics-oracle.jsonl) | Tingsha、Tough Bandages 与弃牌 | 1 / 20 |
| [ghost](../../audits/raw/silent-v0110/ghost-oracle.jsonl) | Ghost in a Jar、Wraith Form | 1 / 23 |
| [knife-trap](../../audits/raw/silent-v0110/knife-trap-oracle.jsonl) | Blade Dance、Knife Trap 重复出牌、Cunning Potion | 6 / 121 |
| [powers](../../audits/raw/silent-v0110/powers-oracle.jsonl) | Afterimage、Serpent Form、Strangle、Well Laid Plans、Wraith Form | 6 / 110 |
| [relic-matrix](../../audits/raw/silent-v0110/relic-matrix-oracle.jsonl) | Helical Dart、Ninja Scroll、Paper Krane、Snecko Skull、Tingsha、Tough Bandages、Twisted Funnel | 4 / 72 |

Env 单元测试 [test_silent_play_capture.cpp](../../../tests/test_silent_play_capture.cpp) 验证嵌套出牌、同牌多 Strangle 实例、Afterimage/Serpent Form 捕获、快照往返与 hash、一张牌选择恢复，以及 Knife Trap 的 CalculatedShivs。`test_silent_pools` 核对全池成员、单人/多人过滤、Silent2/3/4/5/6/7 epoch gate。

## 未完成项报告

- **未完成的范围内模型、override 或源码分支：无已知项。** 源清单计数完整，所有模型均已接入运行时分派或数据池，未解析映射和 stub 检查均为零。
- **Oracle 覆盖边界：**定向原版 trace 覆盖代表性机制与选项恢复，不是对 154 个模型所有组合状态的笛卡尔积穷举。全仓库 `scripts/check.sh` 同样验证公共回归路径；两者都不能替代每个卡牌组合的穷举。
- 与 Regent 主任务协调后，已将其 combat setup 星星历史修正 `dd04945d47314fd2f89ee72a96cebacfb7cfc209` clean cherry-pick 为本分支 `1760c95`。房间进入阶段获得的星星现在保留在首个玩家回合历史中；提交来源报告为 1 场/13 个状态 Oracle 对齐。完整 `scripts/check.sh` 在包含该修正的状态上重跑。
- Oracle headless stderr 中保存了原版初始化时的 Sentry/Godot 提示和带尾空格的 `Failed to open file:` 行；为通过 whitespace 检查，将 stderr 无损 Base64 编码。stdout JSONL 轨迹完整，差分全部通过。
- 新增共享捕获状态使用 8-byte 条目，容量为 `1024 * 26 = 26,624` 条，最坏占 212,992 bytes；本构建 `sizeof(CombatData)=213,264`、`sizeof(GameState)=563,080`。容量覆盖卡牌池与可能的玩家/敌方持有 Power 数上界，测试包含嵌套与快照恢复。

此实现保留在独立 worktree 和分支，未合并；没有修改 `CombatSolver`，没有把搜索策略放入 Env。
