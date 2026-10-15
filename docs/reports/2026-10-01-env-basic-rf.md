# 基本 RF 收尾验收

基线 cfe50f4；分支 feat/env-rf-complete；依据[本轮范围](../plans/2026-10-01-env-basic-rf-capacity.md)。
本批基本 RF 验收通过，容量和数值极值仍待开发，不将整个目标标记完成。

## 实现

AfflictionModel 独立可写对象、UID、附件/深克隆、脱离后的保留和安全边界回收已接入。
拒绝被回收的源克隆；GC 在全部 deck/pile/reward/shop/card 根标记之后标记附件，避免
reward/shop 卡保留而附件被回收。加入1000次应用/清除及明确奖励/商店根回归。
这些保证执行/恢复正确性，不是完整寿命诊断账本。

导入只支持 run/player 的 full_veteran_compatible 档案，继续验证解锁/已见/运行事实，
unsupported 在输入时拒绝。Python 补齐已有 named reset、Oracle 诊断和 event RNG 入口合同。
状态输入规范在[env-state-input.md](../env-state-input.md)，包括数值编码、引用/RNG、
静止根与 pending 前缀、local/portable 指纹/所有权/错误。实际采集与 UI 由调用端负责。

## 验收

1. 定向构建及5项公开/GC合同实际 EOF exit0，原件 `.local/rf-basic-contract-v2.log`。
2. `DOTNET=~/.dotnet/dotnet scripts/check.sh` 实际 EOF exit0：54/54 CTest、非英雄259模型、
   L2 200/0差异、L3 81完整战斗/1625状态和2有界探针/4状态、L4 111/111与25143/25143。
   原件 `.local/rf-basic-full-gate-v1.log`；原版整局资格沿用各 capture 的注入/房间范围。
3. 构建 env_api_shop_fixture/env_api_crystal_sphere_fixture；运行独立消费者：

   `python3 tests/api/basic_rf_gate.py --output-dir .local/rf-basic-acceptance-v2`

   7个job均 EOF exit0、qualified=true，78.675s，源/runtime首尾一致。

| 新 TestMode=false 父采集 | case | Choice prefixes |
|---|---:|---:|
| CallingBell/Cauldron/Astrolabe/Claws | 8/8 | 36 |
| 3个RNG来源、batch/sequential、3种顺序变牌 | 36/36 | 0 |
| 普通商店及重复槽/删牌 | 20/20 | 8 |
| Courier 补货及重复槽/删牌 | 20/20 | 8 |
| Driftwood + Orrery 一次重抽 | 2/2 | 30 |
| Crystal Sphere 两入口及有序点击 | 12/12 | 106 |
| 合计 | 98/98 | 188 |

两新进程按正/反 registry 预热顺序各检查98 case，实际110次成功初始操作（含连续购买的
独立入口）、80次无选择操作、16拒绝、188恢复前缀及298成功后继。完整 canonical
源/后继、事实查询与原版最终事实一致；不得将98称为98种功能或全RF来源条件覆盖。
Oracle 是正版 DLL，关闭 TestMode、保留所记录的表现/输入适配和注入 fixture；不是实机采集。

冻结 source SHA：83bb9731a76a4b45e01aeabf5637ddc59f18f17036f48a3291a4786037d2907a。
C ABI SHA：dca3c03b58cf9169e1894ac03dbb4a9d766dc704b17daf62a0db8cf7eed20d8d。
原件和 manifest 在 `.local/rf-basic-acceptance-v2`，不提交 DLL/大日志。

首次新测试将附件附着到无 PlayerCombatState 的人工 root，原版 Amount setter同样要求该对象；
调整 GC fixture为战斗创建附件后的持有根。Python 取得遗物请求最初漏必填insert_index；
补齐后5合同通过。第一次汇总runner错将重复 --input 传成多值参数，六父采集通过但portable
入口拒绝，旧 `.local/rf-basic-acceptance-v1` 保留为失败；修复参数后完整新采集/重放通过。

## 本轮边界

基本 API 含现有六类 operation、Choice/Boundary 续接、查询与两种快照、C ABI/Python/.NET。
普通动作/奖励安全边界由公开合同及完整原版 L3/L4 回归覆盖，不假称独立RF全功能矩阵。
trace_complete、完整模型参数/对象寿命、rf_full_matrix_verified 继续 false。
不追加 live悬停、真实采集、部分档案、任意 stop、跨构建迁移或完整 Task/delegate 诊断。
下一批修动态容量/数值极值及对应原版、大状态公共入口/性能验收，整个目标仍进行中。
