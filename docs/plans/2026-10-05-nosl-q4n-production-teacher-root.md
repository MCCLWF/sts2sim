# NoSL Q4-n：正常规则的真实学生战斗根

基线 `ba77fdb`。Q4-m 未过扩采门槛，X1 仍为学生基线。本轮从学生实际轨迹恢复原版战斗，
检验最新版 CombatSolver 的教师质量与可靠动作绑定，不再把生成的起始牌组当作学生状态。

## 当前发现与边界

源码确认 `Sts2Oracle` 的默认 run 与上游 OfflineSearchHarness 初始化都使用 TestMode=true。
前者只还原了部分玩法读取，与 AGENTS 的 TestMode=false 要求不一致。
历史 trace 一致仍是其声明范围的证据，但不能视为全普通规则资格。
本轮新路径显式使用 TestMode=false、NonInteractiveMode.AutoSlayerCheck，保持真实本地化、
全解锁老档案、原始 RNG/房间/动作历史。只跳过可审计的视觉/音效/存档 IO，不改游戏效果。
现有 C++ Env、在线 actor、旧 upstream checkout 均不增加未来输入或在线搜索。

## 执行

1. 增加正常规则整局回放选项/必要表现层支持，记录真实 TestMode 值；从冻结 X1 的训练种子
   动作前缀抵达战斗，优先第 8 层起的真实构筑，不注入牌/资源。
2. 新离线宿主链接已锁定 0.50.0 的搜索/宿主辅助代码，在同一原版对象图截取 root。
   不改上游源码；before/after 完整公开状态、RNG 与后续学生动作核对，搜索不得污染 live root。
3. 先 1–3 个根做边界 smoke，再最多 16 个预先选取根。单根 beam30、nodes10000、10秒软预算、
   DOP1，进程硬限时 90秒，失败也保留。至少 12 个根完整完成搜索，且至少 25% 根相对冻结
   学生对应战斗的战损降低 >=3 HP 或把战败变成战胜，才进一步投入教师数据/学习。
4. 要有真实路线动作在原版和 Env 上的身份/合法性/后继对拍；只完成搜索不称可用 BC 标签。
   发现不一致先查规则/适配，不重造 root 或放宽判据。
5. 若合同和质量通过，再单独冻结采集/学习方案及预算；本计划不提前授权无界大量教师搜索。

## 验收和交付

- 正常规则回放直接记录 TestMode=false；选择/资源/RNG 与 Env 核对，不能把旧 on-mode 门禁
  重新包装成 off-mode 证据。未覆盖原版带视觉因果的特殊机制明确拒绝/报告。
- teacher 能从实际学生 prefix 抵达同一个根，搜索不污染原版状态，动作身份可验证。
- noSL actor 仅直接推理；教师完全离线。本轮未训成强学生时目标继续 active。
- 行为改动需完整 `scripts/check.sh`；计划、分支、报告一一对应，保留其他会话改动。
- 重计算先小样本，若使用已授权云实例则有独立关机保护、取回 SHA256 校验并确认 shutdown。
  在恢复合同完成前不启动付费云 GPU。

涉及 `tools/Sts2Headless`、`tools/Sts2Oracle`、新 teacher 宿主/学生根工具、必要检查；
报告 `docs/reports/2026-10-05-nosl-q4n-production-teacher-root.md`，产物 `artifacts/nosl-q4n/`。
