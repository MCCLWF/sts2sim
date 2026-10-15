# Env 特权模型 / Hook 调用帧

基线 `3810a23`，分支 `feat/env-rf-complete`，沿用
[完整因果追踪计划](../plans/2026-09-30-env-causal-execution-trace.md)。
这是部分实现，完整 Env/RF 目标继续 active。

## 已实现

generated dispatch 的263个实际方法入口和Hook的144个定义接观察作用域。
不重写派发/监听者遍历。冻结模型完整ID、覆盖/默认事实、状态内引用类型；
每个帧记录父帧、进入/退出顺序及正常/异常退出。现有操作帧和RNG帧接同一顺序域。
引用仍是当前状态内引用，没有声称永久身份或完整创建/退休账本。

这些帧包含隐藏牌堆监听者身份/顺序，必须显式请求preview bit4。
Python `include_causal`、.NET `includeCausal` 与 typed flags接入；bits1/2/4可以组合。
默认普通preview及单独RNG诊断不收集这些帧。非零trace限制必需，容量失败无后继且源不变。
run query仍仅flags0/1；`trace_complete=false`、完整RF矩阵资格保持未完成。

## 当前真实证据

- `tools/ModelExecutionChecks`：TestMode=false正版DLL；Harmony仅观察同步decimal方法。
  32输入、720模型调用、784帧的模型ID/覆盖事实/父子/进入退出顺序、decimal完整位值、
  修饰者列表与C++一致。原件外部`model-execution-v4/native.jsonl` SHA256
  `fd9a0da8c8af3b4dc3fe838f7e6e8fafeafc8337dc8de2f63dd6e110b29275b3`。
- C ABI descriptor、Python、.NET、scoped engine四项合同通过，原件
  `.local/env-causal-contract-v3.log`；该证据属于组合主线之前的构建。
- Hook生成检查精确匹配144个定义，`scripts/check.sh`已接`--check`，避免漏记新增Hook。
- 初次探针有反射继承MethodInfo补丁目标错误、Native未注册敌人、CPP夹具漏初始化能力变量。
  原失败日志/输出保留；分别用声明类真实方法、`AddCreature`和正规`alloc_power`修正夹具，
  未修改游戏规则、随机抽取、原件或比较字段。

## 与主线组合的实际验收

`99a87b7`接调用帧，`0d258a2`组合main `0b6517c`，规则修复/coverage-v3没有冲突。
native build `0313a1a88e69b13722d520a10b5f9be18c6ba951848901d91951a5de9bbc5f1a`，
shared library SHA `37b71eff2bd697be32b6d308fde8decefc1b8b0030b609148a52b1506f8c7f0c`，
schema `184ae97c4efc8b56e97e642136b825fab851363d53c142700de771e9e7af6f70`。
所有RF进程EOF exit0并核验source/runtime不变；未放宽字段或改写Oracle原件。

| 检查 | 组合构建实际结果 | 原件 |
|---|---|---|
| 完整 `DOTNET=~/.dotnet/dotnet scripts/check.sh` | 54/54 CTest；L2 200/0 mismatch；L3 81完整/1625状态及2有界探针/4状态；L4 110/110、24183/24183决策；最终`all checks passed` | API树`.local/env-causal-combined-full-gate-v1.log` SHA `6629428f0c19733090fee3e1e645cec6dddcd4c49ff61751d5536cf2568adb2c` |
| 同一组合构建消费不变DLL组件输入 | RNG 8seeds/2880顶层/11376调用/288洗牌；模型32inputs/720调用/784帧逐项一致，两个checker exit0 | 外部`causal-combined-checks-v1` |
| TestMode=false RF，`--rng-trace --causal-trace` | 遗物8/36前缀、水晶球6/57、商店5/2、Driftwood/Orrery重抽2/30；合计21case/125前缀；trace开关完整状态相同 | 外部`causal-{relic,crystal,shop,reroll}-oracle-v1` |
| 两种预热顺序的新进程RF恢复 | 每个进程125源/后继、全部RNG记录及model/Hook帧严格相同，最终原版事实/结果保持一致 | 外部`causal-fresh-rf-replay-v1` |
| 新导出portable矩阵 | 两个进程各157源/214后继/12非法拒绝通过；111怪物仅传输immutable definition | API树`.local/env-causal-portable-model-matrix-v1.log`和外部`causal-portable-model-matrix-v1` |

以上外部目录均在`artifacts/legacy/env-rf-api-2026-09-30`下。组合证据manifest SHA
`53163d6d1bc120d5aea0403b4cab6b834dcd1ebe8a54d46fb2be00bf25b0ccff`。
实际21个运行文件已SHA核验归档于`causal-combined-runtime-v1`，23578530字节，manifest SHA
`39633caa3b4a0c1f055161257de59efa6a62b164ebe61973c7bfc0cf0f9f1821`。
旧构建/失败证据保留，不用新库替代旧portable资格。

另一对话的全2000正式复验已EOF；其资格属于main **0b6517c**：1996完整一致、0状态分歧、
4容量失败，2322917/2332668决策、全部671原版victory一致；批次exit1保留。
HP10000是注入深度探针，未认证完整原生合法动作集或双端逐抽RNG。
原manifest `653cb6ada5ce6a790f22ee0402bff54a4e794ef61e9292f391e258158ef9e622`，
不能把它直接算成API组合0d258a2的2000资格。

## 主线合入与产物复核

另一对话的原型编译依赖审计EOF后，main已FF到`0ac3303`；原有`D CLAUDE.md`保留。
主线`cmake --build build -j4`完整exit0，共享库与CLI的实际SHA均与API树相同，
native build保持`0313a1a8`。8项主线ABI/native operation/preview/snapshot/Python/.NET合同全部通过。
主线共享库在新进程消费API树157源/214后继、12非法拒绝通过；两种预热RF续接也各125源/后继、
全部RNG与调用帧逐项一致，所有复核进程exit0。
原件外部`.local/env-causal-main-{build,contract,cross-checkout,fresh-rf-replay}-v1.log`，
RF复核manifest在`causal-main-fresh-rf-replay-v1`。

## 待完成

本组合增量已通过完整门禁并合入，完整Env目标继续active，API开发继续在自己的工作树。
本组件证据不等于263方法/144Hook全参数条件验证，不覆盖Task完成语义、任意实机capture、
方法完整参数/返回、所有命令、对象创建/退休/引用重用、真实档案或全RF21项条件矩阵。
6348实际活牌的容量下界未解决；不升级整局/性能资格。
