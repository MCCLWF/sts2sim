# Crystal Sphere 公共 Env 查询与原版选择续接

分支 `feat/env-rf-complete`，本批基底 `68a9a9d`。属于完整 Env/RF 21 项矩阵第 8 项的
有界增量，不是完整 API 验收或 main 合并结论。计划见
[本批计划](../plans/2026-09-30-env-crystal-sphere-api.md)。

## 前置状态与交付

原版 v0.111.0 的 Crystal Sphere 初始化、点击、揭示、奖励及 Env Choice 已移植，
此前只有直接小游戏样本；尚无公共棋盘查询与整条 EventOption→小游戏→奖励→事件完成的
逐前缀独立 DLL 差分。现新增 `run_queries.event.crystal_sphere`，输出 privileged 棋盘、
物品类型/尺寸/位置、工具、剩余次数和揭示顺序；普通 Observe 保持不包含隐藏棋盘。

新原版 oracle 使用实际 EventSynchronizer.ChooseLocalOption、AwaitPendingOptionTasks、
CellClicked、SetTool、CompleteMinigame、OneOffSynchronizer 以及原版奖励选择。
TestMode=false；仅替代 Godot 场景创建、UI 等待/输入，所有效果、同步器、Hooks 与 RNG
继续在 shipped DLL 中执行。新 test-only shared fixture 仅创建相同源根，通过公共 C ABI
快照导入得到 Env；随后 Python consumer 仅使用公共 query/Predict/Answer/clone/restore。

本批发现并修复默认 `stop=operation_done` 的 typed Action 把执行委派给普通 Step 时
丢失请求上下文：新的 typed Action 保留 `pending_operation`，Answer 结果保留
`continuation_of`。typed Answer/Resume 仍续接原 root/stop policy，不以当前 Answer 请求
替换原操作。完整合同覆盖默认 action 的嵌套选择、快照恢复、typed/ordinary Answer
等价、boundary Resume 后继续 Choice，以及完成后 pending 清除。无新增 POD 字段。

## 命令与原始证据

```bash
cmake --build build -j 8 --target test_operation env_api_crystal_sphere_fixture
/home/wst/.dotnet/dotnet build tools/Sts2Oracle -c Release
./build/test_operation
python3 tests/api/rf_crystal_sphere.py --seeds SEED0002 --output-dir <new-r3-directory>
# 对五角色分别执行 A0/A10；默认四种子、双选项、三条输入顺序：
python3 tests/api/rf_crystal_sphere.py --character IRONCLAD --ascension 0 --output-dir <new-matrix-directory>
DOTNET=/home/wst/.dotnet/dotnet scripts/check.sh
```

- r3 单种子：**6/6 用例，57 个 native Choice 前缀**，指纹保持稳定。
- 扩展矩阵：SEED0002、RFCRYSTAL0001..3 × 五角色 × A0/A10 × 两事件选项
  × 三输入序列，**240/240 用例、2120 个 native Choice 前缀一致**。
- 每个 job 24/24、212 prefixes，source/runtime unchanged=true；批次源文件、native
  library、fixture、Oracle/runtime、正版程序集和本地化等输入全部保存指纹。
- 覆盖大/小工具、重复点击、边缘格、三个卡奖励稀有度、诅咒、药水、金币与遗物。
  语料合计实际揭示全部 15 个物品 index；不宣称覆盖全部棋盘布置。
- 每个 source/Choice/final 比较棋盘、物品、揭示顺序、HP/金币/牌组/遗物/药水的公共实例
  payload，以及所有 run/player/event RNG 的四状态字和 counter。每个 prefix 检查
  clone/restore/query/legal Answer，源不变、旧请求失效、trace on/off 后继等价。
- 最后一击不自动跳过奖励，原版每个奖励及卡牌选择都对应额外 native Choice；完成事件
  后 Env 回到地图决策，pending 清除，完整 final snapshot 可恢复。

外部原件：`artifacts/legacy/env-rf-api-2026-09-30/crystal-api-r3` 与
`crystal-api-matrix-v1/<character>-a<ascension>`，不提交游戏资源、大日志或快照。

失败原件也保留：r1 在卡奖励尝试创建 Godot screen 后 native task 故障；按源码只替换
该 screen 返回/输入，保留原版 CardReward.OnSelect、同步器和奖励完成。r2 全 6 用例
在 pending typed Action 的 snapshot 合同处失败，已修复产品的上下文丢失后重跑通过。
没有修改 RNG 或放宽 oracle 比较来获得通过。

## 门禁与资格

完整 `scripts/check.sh` **exit 0**：CTest **52/52**、L2 **200/200**、L3 **81 场完整战斗
及 2 个 bounded probes**、L4 **103/103 对局、18956/18956 决策**一致，全部差分命令
报告 0 Unported/0 other failures。日志原件 `.local/env-crystal-api-full-gate-v1.log`。
L4 包括跨幕、最终胜利、Pael's Wing 和逐点快照恢复；有界/注入资格沿用原命令。

这是从实际新局 Neow 根替换为 Act 2 Crystal Sphere 的注入测试；不是自然跑到事件的
整局，也不是真实 Godot 游戏内捕获。相同未结束 Neow 被替换的 warning 与已有表现层
日志保留在 stderr，预定各 case 均完成并输出原始结果。

剩余：罕见 partial placement retry 仍明确 Unported；任意游戏中途小游戏 capture、真实
档案、portable snapshots、完整因果 trace 和 RF 其余矩阵继续待验收。
`trace_complete=false` 保留，完整 Env/RF 目标保持进行中。

后续磁盘维护将已结束 API 批次的 prefix JSON 无损压缩为相同路径 `.json.gz`，
逐份验证解压 SHA256 后保留压缩原件；manifest、Oracle 和结果不变。
外部 `evidence-gzip-ledger.jsonl` 提供路径与内容校验，详见
[共享集成报告](2026-09-30-env-rf-history-integration.md)。
