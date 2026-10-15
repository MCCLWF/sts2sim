# 对拍复核与 main 合并验收

日期：2026-09-30。用户授权仔细检查后无阻塞则 merge。合并前主线为 `120877a61afc8d9810858941694cc71559664115`，受验代码提交为 `a684053a19c60c72f65ef3999151117f2a256253`，整合分支为 `feat/strict-differential-main`。结论：本批工具及规则修复达到增量合入门槛；完整真实游戏等价性、E0/E3 与通用 Env/RF 仍未完成。

## 合并范围与复查

将开发提交 `c8f790d` 的本批 delta 移到 main 基底，解决 README/roadmap 文档冲突；保留正确的内容整合现状。未合入 `b4143be` 的 Sts2LiveDiff 网格桥、其计划或报告，`tools/Sts2LiveDiff` 与原 main 没有差异。原开发/WIP 分支保留。工作区既有 `CLAUDE.md` 删除未纳入提交。没有推送或部署，也未解冻 CombatSolver。

三位 gpt-6-luna / xhigh 代理独立核对工具、卡牌/附魔和 GC/怪物/遗物，主代理复核源码、执行集成门禁及证据核验。初审查出并修复：

- L3 忽略非 card_select 的原生 decision；现保留实际位置并明确拒绝，不能用后续错误覆盖首处分歧。
- 无 end 的 bounded probe 曾接受原生多出的 end；现要求原生也无 end，含空对象也拒绝。通过公开 replay API 的故障夹具验收。L4 已拒绝多余决策和终止后的事件。
- 新长局 `DA16EF63A6BC1 / NECROBINDER / A0 / p1` 的第 663 决策暴露 Claws：遗漏原升级及兼容附魔，并错误地逐张变牌。现先建立全部 Maul，按原版克隆属性，再调用批量 Transform。
- 新长局 `D73783C8270AC / SILENT / A10 / p1` 的第 419 决策暴露 Omnislice：缺攻击组 hooks，残留 Akabeko 的 Vigor。按原版新增共享 AttackContext，记录每段 Results，在组完成时 AfterAttack；同步修复唯一另一个调用点 EchoingSlash。显式结束支持提前 return；NeedChoice 保持暂停并由 Env 前缀重放。
- 最后复查补齐 AttackContext 非选牌异常的收尾：先执行 AfterAttack，再重新抛出原异常；NeedChoice 原样传播且不提前消费 Vigor。故障回归验证异常类型/信息保留和两种收尾状态。旧候选 `a0b5b8d`、`6c1f509` 的门禁/回放记录单独保留，外部依赖修复后的最终代码重新完整验收。
- 公共 CardCmd::CanEnchant 补齐原版基础、类型及额外条件，Claws 复用该入口；非法附魔在修改状态前抛 GameError，对应原 InvalidOperationException；原版未限制 amount>0，保留零/负值整数转换语义。
- BingBong 对任何非 null clonedBy 都不再复制；TheInsatiable 保留 Player/PetOwner 的源归属回退。已有 F01–F09、GC 等首批修复同时保留并复验。
- 依赖指纹复核发现 Oracle 从外部 GameData 加载游戏/Godot/Harmony，并从外部目录读取本地化；旧工具只冻结 Oracle 输出目录。补充只读 runtime-provenance 查询及外部目录清单/文件 SHA 校验，使续跑和批次执行前后核对覆盖实际依赖；缺必需文件、依赖增删或内容变化均拒绝资格。

## 新输入与固定回放

本轮开始前固定正常/高 HP 各 10 个不同随机种子，五角色×A0/A10，各 100 条；20 个种子与上一轮的 20 个新种子无重合。正常批首轮 100/100 一致；高 HP 首轮 98 identical / 2 diverged。失败身份与真值原件保留，修复后没有重抽、删除或替换种子。

最终在 main 基底代码上重放同一 525 条完整到死亡/胜利的输入，默认开启每决策快照恢复及后继校验：

| 固定组 | identical | 完整决策 | 原生终点 |
|---|---:|---:|---|
| 历史回归 | 125/125 | 96,440/96,440 | 31 胜利、94 死亡 |
| 上轮正常 HP | 100/100 | 3,824/3,824 | 100 第一幕死亡 |
| 上轮 HP=10000 | 100/100 | 85,888/85,888 | 20 胜利、80 死亡 |
| 本轮正常 HP | 100/100 | 3,639/3,639 | 100 第一幕死亡 |
| 本轮 HP=10000 | 100/100 | 95,824/95,824 | 32 胜利、68 死亡 |
| 合计 | **525/525** | **285,615/285,615** | **83 胜利、442 死亡** |

没有 diverged、Unported、截断、采集/进程/工具错误。225 条正常 HP 均在第一幕死亡，不能证明自然三幕路径；300 条提高 HP 的注入场景用来覆盖更深逻辑，83 次胜利不是正常通关率。525 是固定对局身份数，不是 525 个不同种子，也不是全条件覆盖率。

历史 125 中 104 条原捕获 build 未提供，仍为 legacy；21 条重采的 parent 链见首批报告与 `historical-125-qualified-final.json`。上轮新批和本轮新批各自保存采集 Oracle 程序指纹；这些旧捕获未冻结外部游戏 DLL/本地化，不能追补成完整运行时记录。离线重放不冒充全部用当前 oracle 重采。依赖指纹修复后的原生攻击探针记录实际外部依赖，当前重新构建 oracle 也用于本轮门禁；其 DLL SHA 见下表。

## main 基底验收及证据

Release 构建成功；完整 `scripts/check.sh` 退出 0：46/46 CTest、L2 200/200、L3 81 场完整战斗与 1,625/1,625 状态，另 Untouchable 2 个短探针、4/4 状态；L4 99 条固定路线、16,379/16,379 决策。门禁的幕边界路线不算完整整局。

另以正版 DLL 采集 Akabeko + Omnislice/EchoingSlash 基础/升级形态，4/4 bounded probes、8/8 状态一致；不计入 525 整局。专属 C++ 回归覆盖 Omnislice 单/多目标、EchoingSlash 击杀后两轮伤害及 Vigor 只在组结束后消费，Claws 升级/附魔克隆与非法附魔不改状态，BingBong 非自身克隆来源。工具 protocol 测试为 16 项，batch 合同/故障测试为 19 项（含外部依赖增删/改字节及运行中变化），matrix 测试为 2 项。

验收命令在仓库根执行，输出目录使用 `../artifacts/legacy/strict-diff-merge-review-2026-09-30/`。以下三项均退出 0；重复验收需另选新输出目录，避免覆盖或被工具拒绝。

```bash
STS2_SRC="$PWD/../Slay the Spire 2/src/Core" ./scripts/check.sh

python3 - <<'PY'
import json, subprocess, sys
from pathlib import Path
p = Path('../artifacts/legacy/strict-diff-merge-review-2026-09-30').resolve()
inputs = json.loads((p / 'all-525-inputs.json').read_text())['inputs']
cmd = [sys.executable, 'tools/differential/batch.py', 'replay',
       '--out', str(p / 'replay-525-main-runtime-final'), '--jobs', '4', '--traces']
raise SystemExit(subprocess.call(cmd + [r['trace'] for r in inputs]))
PY

python3 tools/differential/batch.py run --level l3 \
  --out ../artifacts/legacy/strict-diff-merge-review-2026-09-30/attack-probes-main-runtime-final \
  --seeds MERGEATTACK0930 --characters NECROBINDER --ascensions 0 \
  --probe-cards OMNISLICE,OMNISLICE+1,ECHOING_SLASH,ECHOING_SLASH+1 \
  --probe-helper '' --encounter CULTISTS_NORMAL --relics AKABEKO \
  --starting-energy 99 --jobs 1
```

| 指纹 | SHA256 |
|---|---|
| 固定 525 输入 JSON | `a406d30426d416fc3b464e831259b835c29868f48c7df0c5cc822fe0b8fa6d0a` |
| 最终源码（工具定义的执行/编译输入指纹） | `cf08098f74ab90b8341ca639802b601ea4277a20f5087534ff550a17c71821b0` |
| 最终 CLI | `442ad758be66bea427185eb162c4ef5cd417a5cb9da89f523dcb6e67bf59f41d` |
| 门禁/攻击探针 oracle DLL | `4687fd0091b5be91b92071128e615d692f7b0363edfe54d0ec21c1cbff37b7e3` |
| 正版 sts2.dll | `2b40d2df538db1ceb5fa48d958c80ab730ada1e07db88a870aff01a661768b9f` |
| GameData 文件清单/内容（207 项） | `38bcaafb9d946d1aedd34f596ab655870fd05de31dd821a97bb2dec44c63b167` |
| 英文语言表清单/内容（46 项） | `042b002a8f19f44051bdeb9b75bd679f1c5264af6c87da1e952d3d1fb7b75961` |
| 完整 gate 日志 | `b5d544f13104734b0c7c83e749baa1aee100b230bd2c4891c5c531924d71cdac` |

大体积产物保留在工作区 `../artifacts/legacy/strict-diff-merge-review-2026-09-30/`，不提交 Git：`all-525-inputs.json`、`new-natural/`、`new-long/`、`replay-525-main-runtime-final/`、`attack-probes-main-runtime-final/`、`check-main-runtime-final.log`、`final-verification-runtime-final.json` 与 `merge-review-proof-runtime-final.json/.md` 独立核验记录。完整门禁的 `gate-runtime-before.json` / `gate-runtime-after.json` 核对执行源码、CLI、Oracle、实际外部文件与宿主配置均一致。实际每 job 采集/replay 命令、原始字节、输入 SHA、退出码及结果 hash 在对应 manifest/artifacts。525 输入集合、逐项 result 与所有 raw artifacts 的 SHA 已核验；报告提交只修改 Markdown，受验执行源码与二进制不变。

## 能反映真实游戏的范围

规则真值是正版 v0.111.0 / 41cef1ea 源码与 DLL，对方项目只作第二读者。当前结论是已导出的状态、候选、RNG 流计数与静止决策后继一致；不能称为完整真实游戏认证。

AGENTS 的目标要求 oracle 不用 TestMode；现有 oracle 仍启用 TestMode 加 production/presentation 适配，O-001 与 O-006–O-010 等尚未完全撤销。死亡正式结算、异步等待/视觉驱动命中顺序等仍有限。全解锁 D-001 生效。本轮没有新增实机连续路径验收；旧实机证据最多到第四层及窄选择入口，不能作为当前全部规则和三幕的认证。

双端逐抽取 RNG（方法/参数/候选池）、完整原生合法动作、hook/条件覆盖、guided 探索、入口移植、自动最小化、1 万条规模、真实档案和全量容量上界仍待完成。[D-006](../audits/deviations.md) 记录 AttackContext 的 AfterAttack hook 自身失败时 C++ 传播错误、原版 disposal 记录该 hook 错误的区别；本批不把异常路径认作成功对拍。其它定长容器的可达上界不由这 525 条证明。

药水容量改为原版可达的 10 格，生成状态 schema 随之变化；旧版本快照与本版不兼容，必须从原始对局输入重建。批次工具当前依赖 POSIX 进程/锁机制，已验证 Linux，未验证 Windows。

新增外部依赖校验在开跑、结束和续跑时采样；没有使用不可变副本，不能证明中途从未变更后复原。语言表记录实际清单并要求八张核心玩法表，未强制完整官方包清单；.NET shared framework 记录 host 指纹与版本/路径，未逐 DLL 哈希。本次实际 207 个 GameData 文件和 46 张语言表的首尾指纹一致。

上一批四并行约 3.40 倍是其冻结版本的工具吞吐证据；本轮没有重新做 C++/C# 配对 Clone/Step 测量，也不能据此宣称 C++ 语言性能上限已经证明更高。完整目标继续保持未完成，合并的是已验证的增量工具与修复。
