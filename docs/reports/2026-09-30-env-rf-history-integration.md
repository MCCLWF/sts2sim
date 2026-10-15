# 共享历史规则与 TestMode=false Env 奖励输入集成

本批继续完整 Env/RF 计划。API tip `8d7b5ce` 集成 seed 聊的
`8b8c820 → f63ae51` 为 `e19d8bc → 8d7b5ce`。原分支的
[历史与克隆证据](2026-09-30-deep-v2-history-clones.md)保持原资格：53旧失败50完整一致、
0规则分歧、3容量失败；全2000深层 captures 尚在复验。该结果不能替代 API 分支回归。

## 实际变化

组合包含事件 .NET 排序、Cascade 的 X-cost modifier、DrumOfBattle play-count hooks，
EmotionChip 非完全格挡历史、Stomp/Midnight IsClone 判定、Poison.Trigger 内
Accelerant 次数、CloneCard 的 AfterCloned 字段清理与 Iteration 跨 Side 计数。
两项 CombatData history bool 均进入字段遍历/Hash/snapshot，保留 API 的 ChoiceOption.enabled。
生成 schema 指纹 `61b8e11cf3b32a92ec22c87d49c2fd12f4bd3340290a661b5f2b2519a4f977c4`；
旧 schema snapshot 明确拒绝，当前无 portable migration 承诺。

集成后的公共回归显式关闭 TestMode：水晶球6/6及57 prefixes、变牌16/16、
Courier 商店5/5及2 prefixes 均完成，source/runtime fingerprints 稳定。
水晶球 Answer -1/123 均返回 InvalidRequest(5)，源快照不变且 successor 为 null。

Calling Bell 首轮 TestMode=false 回归在原版 NRewardsScreen.ShowScreen 创建 Godot
场景时退出 -6；不是通过。原件 `history-integrated-relic-v1/oracle.stderr` 保留。
现将水晶球已有 UI 输入 adapter 统一安装在 Env 操作命令的 scoped setup 中，
退出时恢复所有 patches；不改 RelicCmd.Obtain、RewardsSet.Offer、GenerateWithoutOffering、
RewardsSetSynchronizer 或 CardReward.OnSelect。

该修复后两 seed × 四遗物 **8/8、36 native Choice prefixes 一致**，TestMode=false，
clone/restore/Answer、源不变、trace on/off 和 source/runtime 指纹检查通过。
扩展四 seed × 五角色 × A0/A10 × Calling Bell/Cauldron/Astrolabe/Claws，
**160/160 用例、700 个 native Choice 前缀一致**，十个 job 的 fingerprints 均保持稳定。
修复后的水晶球复核 **6/6、57 prefixes**，Driftwood→Orrery 一次重抽
**2/2、30 prefixes**，均 TestMode=false。最终完整门禁 v2 **exit 0**：52/52 CTest、L2 200/200、L3 81 完整战斗和 2 probes、
L4 103/103及18956/18956决策一致，0 Unported/0 other failures。

## 原件与验收

```bash
cmake --build build -j 8 --target env_api_shop_fixture env_api_crystal_sphere_fixture sts2sim_cli
DOTNET=/home/wst/.dotnet/dotnet scripts/check.sh
python3 tests/api/rf_operations.py --seeds RFOP0001,RFOP0002 --oracle-test-mode off --output-dir <new-directory>
# 扩展矩阵每角色/A0/A10，默认 RFOP0001..4 与四种遗物
python3 tests/api/rf_operations.py --character IRONCLAD --ascension 0 --oracle-test-mode off --output-dir <new-matrix-directory>
```

外部原件在 `artifacts/legacy/env-rf-api-2026-09-30/history-integrated-{crystal,transform,shop}-v1`、
`history-integrated-relic-v2`、`history-integrated-crystal-v2`、
`history-integrated-reroll-off-v1`、`history-integrated-crystal-invalid.json` 与
`relic-testmode-off-matrix-v1/<character>-a<ascension>`。
集成完整门禁 v1/v2 均退出0。最终产品门禁日志
`.local/env-rf-history-integration-full-gate-v2.log`，包含实际 .NET SDK/native 合同及
跨幕/胜利/逐点快照恢复。最后消费者 metadata 调整只把通用冻结说明中的默认 TestMode
文字替换为该命令的实际 false 模式；比较判据和产品均不变。既有原件的命令配置、
每行 oracle_mode.test_mode 与消费端硬断言仍保留，修订后的字段再以新目录复核。

这是 shipped-DLL headless 输入/表现层适配资格，不是 live capture。真实档案、
任意 live decision 捕获/恢复、完整因果 trace（仍 trace_complete=false）、完整 RF 21 项
矩阵、portable snapshots 和剩余容量/规模验证保持未完成。本批记录时 main 未合并；
之后阶段代码已 fast-forward 到 `da39de0`，当前完成状态及中断的深层复验见
[API 检查点第九批](2026-09-30-env-rf-api-checkpoint.md#第九批当前-api-完成状态复核)。

## 最终封存与磁盘维护

metadata 修订后的新 consumer 复查：Crystal Sphere **6/6、57 prefixes**，
四种遗物 **4/4、18 prefixes**，source/runtime 指纹稳定。产品与比较判据未改。
Python 语法、git diff whitespace 以及生成 schema check 通过。

最后检查遭遇共享磁盘 ENOSPC，Python cache 写入失败；取得空间后重跑成功。
为保留原件且恢复空间，对本 API 已结束批次的 **4704 份 prefix JSON** 无损 gzip：
每份先验证解压 SHA256 与未压缩文件相同，写校验 ledger 后才移除同份未压缩副本。
释放 **1963133491 bytes**；查阅 prefix 时使用相同路径的 `.json.gz`。
外部 `evidence-gzip-ledger.jsonl` 记录原路径/压缩路径/内容 SHA/两种大小。
Oracle JSONL、manifest、summaries、结果及失败原件继续保留；没有删除游戏原件。
仅删除可重建的 Dispatch.cpp.o 缓存以取得初始压缩空间；已验收 CLI/library 不变。
