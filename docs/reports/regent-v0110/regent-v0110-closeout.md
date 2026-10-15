# Regent v0.111.0 移植收尾报告

## 覆盖范围

依据本地反编译源 `Slay the Spire 2/src/Core` 与原版 `Sts2Oracle`，完成 Regent 单人 Env 的源级移植和核对。逐模型、逐 override、逐显式条件/循环/选择项清单见[源清单](../../audits/regent-v0110-port-checklist.md)。清单共 153 个模型/池/纪元条目：角色及池/纪元 11，卡池 91，生成/变换依赖牌 5，相关 Power 35，遗物 8，药水 3。清单没有未勾选项；`[~]` 只用于不进入单人 run-state 的显示、声音、美术、多人字段和局外故事解锁队列，epoch 位对应的局内池门控已实现。

实现包含所有卡牌及生成牌、职业遗物、职业药水和引用到的 Power；星星费用/支付与历史、玩家起始资源、按 Regent2/3/4/5/6/7 epoch 过滤卡牌/遗物/药水，以及跨回合状态的 clone/snapshot 恢复。通用战斗历史计数器在单独提交 `4846813`，由此分支及其它需要该合同的移植共同使用。共享 `CardModel` API 在 `1a3d870`；所有提交仅在 `feat/regent-complete-v0110` 独立 worktree，未 merge。

## Oracle 结果

原始 Oracle JSONL、对应 Env replay JSONL、逐模型结果 manifest 均保存在本目录。全部文件的 SHA-256 在 [regent-v0110-SHA256SUMS.txt](regent-v0110-SHA256SUMS.txt)。

| 检查 | 结果 |
|---|---|
| `REGENT_CARD_POOL`，升级 0/1/2 | 每档 91/91 模型一致；分别 855/855、802/802、802/802 个状态一致；119/119 个卡牌选择决策一致 |
| 5 个显式派生/生成依赖牌，升级 0/1/2 | 每档 5/5 模型一致；分别 42/42、38/38、38/38 个状态一致 |
| 遗物池 | 8/8 模型、80/80 状态一致；唯一触发的卡牌选择 1/1 一致 |
| 药水池 | 3/3 模型、42/42 状态一致；药水在首个状态前直接使用并核对效果/RNG，药水自身不请求交互选择 |
| Monologue 跨回合专项 | 升级 0/1/2 各 13/13 个状态一致 |
| 星星历史专项 | 13/13 个状态一致，包含首次回合和历史计数 |
| 储君胜利整局 | `SEED0002`、policy 1、无 Neow、高生命值测试配置，通过 Victory；790/790 决策一致，使用 `--verify-snapshots` 对每个决策恢复快照并比较下一状态 |

卡池各模型单独跑 Oracle 与 C++ Env；命中选择的模型同时比较选项及执行顺序。补充的 epoch 门控单测覆盖 Regent2/3/4/5/6/7 各自开/关和全解锁池行为。重点的跨回合胜利整局通过逐决策 snapshot restore，避免只验证终局结果。

## `scripts/check.sh`

完整脚本以 `STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core' scripts/check.sh` 执行，退出码为 0。原始输出、stderr 和退出码已保存：

- [stdout](scripts-check.final.stdout)
- [stderr](scripts-check.final.stderr)
- [exit code](scripts-check.final.exitcode)

摘要：CTest 18/18；L2 200 个新局设置零分歧；L3 80/80 场战斗、1611/1611 状态；标准 L4、定向遗物、跨幕和胜利路线合计 98 局、16191/16191 决策。无 unported stop 或差分。Oracle 项目有一个既有 C# nullable warning，未影响构建或验证。

## 剩余问题与范围外失败

本清单范围内没有未处理项。局外故事时间线/解锁队列仍不属于通用 Env 的 run-state 合同；测试可通过显式 epoch 位快照核对实际池门控。

验证时另做了几条探索路线，暴露出本任务范围以外的既有公共内容缺口，原始 Oracle trace 保留在本目录：

- `ToyBox.AfterObtained`、`NutritiousSoup.AfterObtained`、`ElectricShrymp.AfterObtained` 尚未移植；这些随机遗物让相应探索路线提前停止。
- headless Oracle 的 `CrystalSphere` 路线出现原版 `NullReferenceException`，因此未产生可比较的 trace。

这几项不是 Regent 内容；最终 Regent 胜利整局避开这些外部缺口并完整通过。分支未改动 `external/CombatSolver/`，也未执行 merge。
