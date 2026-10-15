# Env portable snapshot 开发与验收记录

基线 `73d053e`，`feat/env-rf-complete`。属于完整 E1 恢复目标的一项增量；
不是 E0–E5/RF 完成报告。原版规则以 v0.111.0 源码与 shipped DLL 为准。

## 实现

保留 v4 的 loaded-runtime 限制，增加显式 `sts2env.portable_snapshot` v1 和一个 C ABI 导出。
所有 production FSM move/condition/weight/intent、事件选择/致死谓词和 stun 回调注册
同构建稳定符号，不序列化函数指针。定义保留创建时的所有常数、分支、初始状态及注册顺序；
事件 LocKey 字典包括 description/custom_done_page/text_key/done_page。

来源/内容/schema、编译器/平台/编译配置构成 SHA256 build identity；数学 runtime 和浮点控制
另行核对。规范化后的 state/base 中的定义、stun、事件回调下标在目标 runtime 重绑定，
pending operation 的 source hash 按本地根重新求值。完整 v4 POD/稳定根/前缀重放校验仍执行。
wire hash 覆盖状态与字典/定义，用于完整性，不能当作 native source hash 或签名。
Python/.NET 使用同一实际共享库；旧 session-bound DTO 仍拒绝跨 SDK owner 混用。

## 当前实测证据

```bash
cmake --build build --target env_portable_fixture env_api_shop_fixture env_api_crystal_sphere_fixture -j 6
python3 tests/api/portable_snapshot.py --fixture build/env_portable_fixture \
  --library <build/libsts2env_c.so绝对路径> --matrix --output-dir <新的外部证据目录>
DOTNET=~/.dotnet/dotnet scripts/check.sh
```

`portable-model-matrix-v4` **157 组**：16 个专门状态、30 个五角色/A0/A10/三遭遇战斗根，
111 个 IsUsable monster immutable definition 的传输库存夹具。
两次独立新进程采用相反的 registry 预热顺序，各 **157/157 source、214/214 后续状态**
与 producer 普通执行结果逐字段一致；原 v4 外进程全部被拒绝。
12 类非法输入无 successor 且 diagnostic Env 未变化，其中9类已重新计算合法 wire hash，
证明不是只由 checksum 拦截：未知符号、错误类型/owner、定义链接/重复、未用字典、RNG顺序、
多余 replay base、逻辑 hash；另含 build/math runtime/wire hash 错误。

专门状态包含 EnemyPhaseDone、SafeOutcome、Astrolabe/Orrery typed Choice、事件文本、
7个非空stun回调及null stun、Nibbit/Chomper的创建后字段变化。
111 个定义夹具只验证传输/绑定，未滚动缺少自然遭遇上下文的初始分支，不能写成111怪物行为验收。
首轮全部 Add 的库存夹具触发 No valid next state；失败目录保留，后改为明确的定义传输范围，
实际行为范围由另外30战斗根及定向后继承担。

过程失败保留：同一源码行回调符号碰撞由column identity修复；首轮新进程事件操作发现
on_chosen/致死谓词也需portable绑定；四个共享FSM owner需按源码显式关联。
没有以删除runtime校验、跳过效果或放宽状态比对换取通过。

## 原版 RF 恢复后执行与完整门禁

本轮 consumer 每次 Answer 前实际使用 portable 恢复分支，TestMode=false，所有批次
`qualified=true`、source/runtime unchanged，退出0：

| 冻结批次 | 原版用例 | Choice 前缀 | manifest SHA256 |
|---|---:|---:|---|
| portable-relic-oracle-v1 | 8/8 | 36 | 7bbd396639d246728855902f700c15f05daf9800dd25bd3d6dc8b85a39db09d4 |
| portable-crystal-oracle-v1 | 6/6 | 57 | 61aa667227427fd9623ad28eadf090079052c174c5261a600df3a81adee6669e |
| portable-shop-oracle-v1 | 5/5 | 2 | 1e995dacea9b30e7e1fc30668b7270bf8e6c16b08ef61ebd8b5839df2b93b106 |
| portable-reroll-oracle-v1 | 2/2 | 30 | 87c3de81a52943c11125aa184835307aff5f06fc4397772bbdf7e67565ca52a9 |

随后独立新进程读取这些冻结原件，正/反 registry 预热各 **125/125 恢复及后继**通过：
所有规范化状态、完整查询（native source hash 重新绑定）、Answer 的下一前缀严格一致，
最后一步对比已保存的原版最终事实/RNG及操作结果。21个case中17个有Choice；
没有Choice的商店操作不算新进程恢复后继。不是重新采集原版，也不是实机 capture。
工具检查父 summary/manifest/oracle hash、原共享库字节身份及输入未变化。

```bash
python3 tests/api/rf_operations.py --seeds RFOP0001,RFOP0002 --oracle-test-mode off --output-dir <新目录>
python3 tests/api/rf_crystal_sphere.py --seeds SEED0002 --output-dir <新目录>
python3 tests/api/rf_operations.py --seeds RFOP0001 --operation merchant_purchase \
  --purchase-orders '0,0;5,5;7,7;10,10;13' --gold 10000 --character IRONCLAD \
  --ascension 10 --oracle-test-mode off --output-dir <新目录>
python3 tests/api/rf_operations.py --seeds RFOP0001,RFOP0002 --operation obtain_relic \
  --relics ORRERY --card-reward-policy reroll_once --setup-relics DRIFTWOOD \
  --oracle-test-mode off --output-dir <新目录>
python3 tests/api/portable_rf_replay.py --library <实际共享库绝对路径> \
  --input <relic批次> --input <crystal批次> --input <shop批次> --input <reroll批次> \
  --output-dir <新目录>
```

新进程RF原件 `portable-fresh-rf-replay-v2`，manifest
`39a0d37541c44c5b1d1a1df5404d34960ccdd6226fd026a8fac32cd22f80c331`。
首轮脚本把两种报告里的 `off` 字符串和 `false` 布尔混用而启动前拒绝，失败日志保留；
修正为按明确报告格式校验，不放宽游戏字段比较。

完整 `scripts/check.sh` **exit0**：53/53 CTest、L2 200/200、L3 81完整战斗和2有界探针、
L4 103/103路线及18956/18956决策一致，零Unported/其它失败。
日志 `.local/env-portable-full-gate-v1.log`，SHA256
`467b969406ee6ee6ac2081bc939216f6d3d776db5f431afbdd93f2f757c0a836`。
增加数学runtime非法输入后另跑portable CTest exit0，完整157矩阵两新进程各12拒绝通过。
没有修改已通过完整门禁的产品源码；新增RF重放工具单独执行。

本轮 portable build SHA256
`f2db10149591101e3d89e8347e5102eb63dba0974d67d3807593a4e8cbaaadc7`，
runtime `round=0;glibc=2.43;mxcsr_control=8064`。代码/配置变化后必须重新导出与验收；
这些旧包不承诺导入新构建。
所有原件位于工作区外 `artifacts/legacy/env-rf-api-2026-09-30/`，失败版本仍保留。

## 完整目标仍缺

真实 profile、任意实机 capture、完整因果/RNG/对象寿命 trace、RF 21 项全部条件及规模/性能。
portable仅允许同构建/数学runtime/浮点配置，跨构建迁移明确拒绝。
本轮已补上述125个新进程RF后继证据；仍不能替代其它RF来源/条件及完整实机验收。
