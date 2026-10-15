# sts2-ai、CombatSolver 与 sts2sim 同机比较

日期：2026-10-02。基底 sts2sim main `996cf33`；本轮只加比较消费者和文档，不改游戏规则、Env 或搜索算法。

## 版本资格

GitHub 活跃身份与 API `/user` 均确认 `MCCLWF`。`git pull --ff-only origin main`
和 `gh repo view iRyougi/sts2-ai` 返回 Repository not found；该账号没有待接受邀请。
用户随后确认失去私有仓库权限，并授权使用现有 checkout。因此战斗搜索版本为
**本地 sts2-ai `<私有提交>`，不是最新私有 main**。
已有两处 CLAUDE.md 删除保留，搜索源没有改动。

维护者公开 Core 导出 `iRyougi/sts2-sim` 已拉到
`c6282abc8c361682ec7f9c5daca3683cceea8252`（2026-10-02 12:06:49Z），提交标记
同步私有上游 `29105a1b`。它只有 `Sts2Sim.Core`，没有搜索器，故只用于补充最新公开 Env 测量，
不将其与本地搜索混编或宣称最新版完整 sts2-ai 已测。

原版 **CombatSolver** 独立于上述 sts2-ai，固定提交
`d7b46203593beb51f792628a2587b04f0850d1c2`（0.47.3），重新 Release 构建模组与
OfflineSearchHarness。它使用真实游戏的离线预测/回退；本轮同时测量，不沿用旧报告34.82秒冒充新结果。

## 条件与预算

- 同一台 i7-14650HX、24逻辑CPU、Linux、GCC Release 与 .NET9 Release。
  存在桌面负载，三轮交替进程；不同时跑构建、回归或另一计时进程。
- Env 标量绑定 logical CPU2；并行绑定2/4/6/8/10/12/14/0，最多8个P核。
  原180根：5角色×A0/A10×Cultists/Exoskeletons/MechaKnight×3seed×第1/3回合。
  每根128预热、7组×64操作、极短操作增加循环，三进程轮换起始根。
- 搜索进程不绑核；每配置每根新进程，三轮交替次序。GNU time 保存全进程
  CPU、墙钟、峰值RSS；搜索耗时排除建根/回放/输出，C#包含第一次搜索的JIT。
  这与持续运行服务的热态延迟不同。
- 21根沿用之前同一组A0起始牌组：Beam30、数字节点帽10000、请求10秒上限、无初始药水。
  原生节点是父节点（跨宽度/目标成员），sts2-ai Beam节点是动作尝试；原版也是组合总父节点。
  **相同数字帽不是相同模拟量**。原生可加宽，sts2-ai Beam固定宽30，搜索覆盖也不同。
- 补充6根统一2秒上限、节点帽100000000（不先被节点限制），比较同墙钟效果。
  两线程可使用额外CPU，因此同墙钟也不是同CPU预算；不将其当作胜率总体结论。
- 本地 `CombatSolver.Beam.BeamSearch` 是实际可调用的新战斗搜索，逐回合保留普通/setup/progress
  路线，真实克隆Core并用intent等评分；它为串行。`Search/`里部分完整移植合同不是可运行完整Coordinator。
  `Sts2Sim.Combat.<私有组件>` 是<私有组件>仍使用的逐动作旧搜索，另列诊断。

## 最新公开 Env

180/180共同根、540/540边界（root、EndTurn、基本牌）投影一致；两边所有三轮180根成功且根未变。
资格投影含HP/资源/有序牌堆/生物状态和战斗RNG counter；不是所有内部字段逐位证明。

三轮每根中位数再取根间中位数，单位µs：

| 操作 | sts2sim 完整Env | 公开C# Core |
|---|---:|---:|
| Clone | 2.06 | 11.79 |
| Clone+EndTurn | 124.55 | 124.84 |
| Clone+PlayCard | 36.77 | 36.11 |
| 直接底层Clone+EndTurn | 105.82 | — |
| Predict EndTurn | 141.15 | — |
| LegalActions / 等价夹具 | 13.56 | 27.93 |
| Observe | 14.16 | 94.03 |
| Hash / ExactCombatDigest | 11.90 | 77.19 |

C++克隆完整Env，C#克隆CombatState；Hash与观察编码不同，不能解释成纯语言排名。
前一份Env报告的C++Clone13.39µs/Clone+EndTurn469.03µs属于旧构建，不能继续当当前性能。
这里对比的新数字都重新测量，不将历史普通Clone4.548µs均值当作本矩阵中位数。

固定Silent A10 Cultists第3回合，8192个独立克隆＋EndTurn任务，三轮中位吞吐：

| 工作线程 | sts2sim ops/s（相对1线程） | 公开C# Core ops/s（相对1线程） |
|---:|---:|---:|
| 1 | 9335（1.00×） | 1816（1.00×） |
| 2 | 16077（1.72×） | 5847（3.22×） |
| 4 | 28236（3.02×） | 7952（4.38×） |
| 8 | 52745（5.65×） | 8256（4.55×） |

两边根均未被并发修改。C#超线性早期加速不能推广到一般任务；单核绑核下JIT/GC调度、
预热效果可能影响结果，尚未分解证明其原因。该表衡量独立任务吞吐，不能冒充一场搜索的延迟加速。
配对Clone几何均值C++/C#为0.172×（180/180根更快）；EndTurn为0.990×（108/180更快），
PlayCard为1.011×（83/180更快）。因此两种执行操作整体已接近，不能只比较两个总体中位数
就宣称每种场景C++都更快。Env标量进程峰值RSS约11.4–11.6MiB/C#95.7–97.0MiB；
C#每轮约11361–11375次Gen0收集。搜索消费者的大状态缓存会显著增加RSS，另表记录。

## 战斗搜索结果

21/21根的投影与9条战斗RNG的counter+s0–s3全部匹配，三个重复共63/63资格通过。
sts2-ai Beam路线在独立Core克隆上实际执行，胜利/战损与预测一致、原根digest不变；
原生路线仍经过完整Env hash重放验证。资格不是每个内部字段或其他场景的全状态等价。

以下为三轮“21场合计”的中位数；每轮四配置均21/21胜利、效果完全稳定：

| 配置 | 搜索秒 | 进程墙钟秒 | 进程CPU秒 | 平均CPU核 | 峰值RSS MiB | 总战损 |
|---|---:|---:|---:|---:|---:|---:|
| sts2sim 1线程 | 46.29 | 47.63 | 47.50 | 1.00 | 411.0 | 349 |
| sts2sim 2线程 | 42.11 | 43.46 | 52.91 | 1.22 | 514.8 | 349 |
| sts2-ai Beam（<私有提交>，串行） | 44.42 | 55.57 | 61.62 | 1.11 | 177.8 | 456 |
| 原版CombatSolver（d7b4620，DOP1） | 35.62 | 68.98 | 72.45 | 1.05 | 211.7 | 350 |

我们的两线程减少搜索墙钟**9.0%**（1.10×），CPU时间增加11.4%，RSS峰值增加25.2%；
三轮63/63路线、quality、最终完整hash与单线程相同。不是2倍加速，且有资源代价。
相较本地sts2-ai Beam，我们单线程搜索约慢4.2%，两线程约快5.2%，但总体更省血：
15场更好、4场相同、2场更差，总少损107HP；最差单场多损4HP。
相较原版，14场相同、4场更好、3场更差，总349/350接近，但最差单场多损15HP；
两线程搜索仍慢18.2%。这些都是起始牌组小样本，不能推成A10整局胜率。

全进程CPU/墙钟包含每个宿主不同的建根、JIT、回放与输出。原版离线宿主的这些额外工作
尤其明显；**不能用72.45/52.91推导搜索算法本身的CPU效率**，也不能用68.98/43.46
直接推导持续服务的部署延迟。表内实际资源占用是正确的，但工作范围不同。
这批调用均在各自进程内执行Env/预测，Python只调度整次请求，不是每节点IPC基准。

### 展开与多线程为何有限

| 配置 | 记录展开/尝试 | 真正原生预测 / 原版transition |
|---|---:|---:|
| sts2sim 1线程 | 116565父节点 | 430032次原生预测 |
| sts2sim 2线程 | 121103父节点 | 472287次原生预测 |
| sts2-ai Beam | 102843动作尝试 | 没有同口径原生预测计数 |
| 原版CombatSolver | 39354组合父节点 | 126165次transition |

原生与原版的计数范围仍有评分预报、前缀重放等差别，不能当成等价单位。
原生扩更多父节点和不同宽度/目标，是候选保留与从根重搜组织差别；更多工作并未换来
明显更好的21场结果。原版筛选更有效，不是C++或多线程自然应当胜出的证明。

单线程Pass细分：普通搜索约占55.9%（第一轮合计25.32秒），集中搜索19.995秒。
普通搜索先跑，集中成员才并行；11场Cultists有两个目标，另外10场Fuzzy只有一个。
即使两个集中任务理想均分，按本批比例也只有约1.28×的理论加速上限；实际还要支付
缓存分配、调度、同步和重复模拟。两线程真正预测多9.8%，进一步抵消收益。
此前4目标Scrolls探针4线程4.52→3.00秒属于另一有限场景，可解释目标更多时收益更大；
不能用它替代本轮21场数据，也不能把8线程请求当作8个有效搜索任务。

因此**当前成员级单场搜索收益小；Env独立任务、多场批量并行收益明显**。两者不是同一种实验。

### 同2秒上限与旧产品搜索

六根为五角色Cultists/BMS0001，加Ironclad Fuzzy/BMS0001。
同传入2秒上限、高节点帽；每场原生最大实测2.039秒，sts2-ai2.014秒，存在不可中断
单次模拟及收尾的少量超时；原版最长1.692秒且可提前停止。不强迫任何搜索花满预算。

| 配置 | 三轮胜利数（每轮6根） | 完成全部6场的总战损 | 搜索总秒中位 | 进程CPU秒中位 |
|---|---|---|---:|---:|
| sts2sim 1线程 | 6/6、6/6、6/6 | 133、138、133 | 10.67 | 11.09 |
| sts2sim 2线程 | 6/6、6/6、6/6 | 133、133、133 | 10.69 | 13.58 |
| sts2-ai Beam | 5/6、4/6、5/6 | 不汇总部分路线为完整战损 | 11.53 | 16.03 |
| 原版CombatSolver DOP1 | 6/6、6/6、6/6 | 150、150、150 | 6.91 | 16.50 |

sts2-ai Silent三次都只返回部分路线；第二轮Necrobinder也未找到完整胜利路线，
没有实际判成战斗死亡，不能写作死亡率。固定墙钟会产生时间截断差异，原生单线程
第二轮战损也变化，正是为何要保留重复结果。此小样本说明当前原生在短请求上有优势，
不证明总体更强；原版提前停，不能据此假设它花满2秒后的结果。

逐动作旧搜索诊断：Ironclad/Cultists/CSPR0BE1，20次决策，共搜索18.57秒，胜利、战损33，
进程CPU19.85秒、峰值RSS138.0MiB；对应原生1/2线程2.76/2.41秒、战损43。
它花了更多全场搜索时间，效果更好，不能列成同预算的速度/质量排名。
不启用会大量分配的层诊断，所以展开数不可用（JSON null），不是零。
<私有组件>旧产品默认Beam4/100节点/10秒**每决策**；
本轮诊断使用比较用Beam30/10000节点/10秒每决策，明确不是产品默认配置。

## 复现与证据

计划：`docs/plans/2026-10-02-sts2-ai-latest-comparison.md`。
消费工具：`tools/combatsolver/sts2ai_compare`；Env工具沿用`tools/bench/compare`。
原始本地产物：工作区上层`artifacts/legacy/sts2-ai-latest-2026-10-02`，包含版本/账号/失败拉取说明、
二进制SHA256、完整输入/命令、每次输出、CPU/RSS、资格差异与各轮结果；不提交大原始数据。

```sh
cmake --build build --target env_compare_native -j 8
dotnet build tools/bench/compare/managed/Bench.csproj -c Release -p:Sts2AiRoot="$PUBLIC_CORE"
python3 tools/bench/compare/run.py --output "$RESULTS/env" --native build/env_compare_native \
  --managed tools/bench/compare/managed/bin/Release/net9.0/Bench.dll --mode verify
# 以相同参数分别运行 --mode measure 和 --mode parallel；随后 summarize.py。
dotnet build tools/combatsolver/sts2ai_compare/Compare.csproj -c Release -p:Sts2AiRoot="$STS2_AI_ROOT"
python3 tools/combatsolver/sts2ai_compare/run.py --native build/combatsolver_search \
  --managed tools/combatsolver/sts2ai_compare/bin/Release/net9.0/Compare.dll \
  --sts2ai "$STS2_AI_ROOT" --upstream "$COMBATSOLVER_ROOT" --out "$RESULTS/search" --repeats 3
# time-budget另加 --cases "$TIME_CASES" --nodes 100000000 --budget-ms 2000。
STS2_SRC="$GAME_CORE" scripts/check.sh
```

完整`STS2_SRC="$GAME_CORE" scripts/check.sh`退出0：57/57 CTest（33.13秒）、L2 200/200、
L3 81完整战斗+2有界探针/1629状态、L4 111/111路线及25143/25143决策一致，最终
`all checks passed`。比较C#消费者Release构建0警告/0错误；Python语法与`git diff --check`通过。
前期消费者遭遇API名称写错的两次构建失败、原版未设SteamRoot的一次失败保留在原始日志，
最终修正后再测量；失败没有混入有效结果。本报告不升级Env完整保真/自然整局资格，
不提供NoSL或训练结论。
