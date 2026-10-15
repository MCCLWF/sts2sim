# Thieving Hopper Weak 移植报告

日期：2026-09-26  
分支：`feat/all-encounters-comparison-mod`  
基线：v0.111.0；真值为反编译源码和 Sts2Oracle。

## 实现

- 新增 Thieving Hopper Weak、Thieving Hopper 及 Swipe、Escape Artist、Flutter 三个能力。
- 按 Draw 后 Discard 的原序筛选有 DeckVersion 的牌；优先非 Imbued 的 Uncommon，再 Common/Rare/Event，再 Basic/Quest，最后 Ancient/Imbued。没有优先级命中时从全候选列表选取。偷牌使用 CombatCardGeneration RNG。
- 偷牌先移除战斗副本，再移除牌组原牌，并将副本保存在 Swipe 的固定池索引中。
- Flutter 对 powered attack 伤害乘 50%，每次未被完全格挡的攻击消耗一层；耗尽后进入 STUNNED，并按最后记录招式的原生 follow-up 继续。
- Escape Artist 从 5 倒数至 1；完整移植 Thievery→Flutter→Hat Trick→Nab→Escape。
- 死亡前将原牌重新登记进 RunState 并生成 SpecialCard 奖励。领取直接加入原牌到牌组，保留升级、附魔等原实例信息；逃跑不会返还。新增 SpecialCard 的排序、已生成判断、Populate 和直接领取路径。
- 动画、音效及历史统计不进入无头游戏状态；未新增其他角色的卡牌。

## 验证观察扩展

L3 战斗结束记录现在同时输出 run 状态（含牌组、金币、run RNG）及额外奖励。`--take-special-rewards` 在结束后调用原生/模拟器各自的 SpecialCard 领取方法，再输出领取后的 run 状态。只扩展观测及显式测试动作，没有替换 oracle 游戏逻辑或放宽比较。

oracle 的 SpecialCard JSON 通过读取奖励的原生私有 `_card` 字段记录牌名；CLI 的同一个奖励输出函数供 L3/L4 共用。

## Oracle 对拍

全部使用 Ironclad、策略种子 1,2。

| 配置 | 种子 | 战斗 / 状态 | 结果 |
|---|---|---:|---|
| A0 默认牌组 | HOP01,HOP02 | 4/4 · 66/66 | 一致 |
| A10 默认牌组 | HOP10 | 2/2 · 35/35 | 一致 |
| A0 Bludgeon 防守/逃跑 | HOPR01,HOPR02 | 4/4 · 63/63 | 一致 |
| A10 Flutter 耗尽 | HOPF10 | 2/2 · 35/35 | 一致 |
| A0 纯防御/逃跑 | HOPE01 | 2/2 · 35/35 | 一致 |
| A0 混合稀有度偷牌 | HOPP01,HOPP02 | 4/4 · 64/64 | 一致 |
| A0 死亡返还/领取 | HOPRET01,HOPRET02 | 4/4 · 12/12 | 一致 |

标准累计增加 6 场、101 状态：**264/264 战斗、4915/4915 状态**。专项 16 场、209 状态独立记录。

Flutter trace 包含 STUNNED 与后续招式；逃跑 trace 完整走过五种招式且结束牌组维持 9 张；死亡返还的 4 场均在奖励中记录 `BLUDGEON+1`，领取后牌组从 9 张变回 10 张，领取后的 RNG 也一致。

标准命令：

```bash
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py --seeds HOP01,HOP02 --characters IRONCLAD --policy-seeds 1,2 --ascension 0 --encounter THIEVING_HOPPER_WEAK -v
DOTNET="$HOME/.dotnet/dotnet" python3 tests/l3/l3_combat.py --seeds HOP10 --characters IRONCLAD --policy-seeds 1,2 --ascension 10 --encounter THIEVING_HOPPER_WEAK -v
```

专项命令均增加 `--take-special-rewards`，按逗号展开以下牌组并传入 `--deck`：

- HOPR01,HOPR02 A0：4 张 BLUDGEON+1、6 张 DEFEND_IRONCLAD+1；`--relics TUNGSTEN_ROD,ANCHOR`。
- HOPF10 A10：4 张 SWORD_BOOMERANG+1、6 张 DEFEND_IRONCLAD+1；相同遗物。
- HOPE01 A0：10 张 DEFEND_IRONCLAD+1；无额外遗物。
- HOPP01,HOPP02 A0：UPPERCUT+1,BLUDGEON+1,POMMEL_STRIKE+1,BASH+1 与 6 张 DEFEND_IRONCLAD+1；相同遗物。
- HOPRET01,HOPRET02 A0：10 张 BLUDGEON+1；无额外遗物。

trace 只保存在本机临时目录。Ancient/Imbued、pet-target 和保存读档未在这些专项中动态覆盖；优先级及实例字段按源代码检查，单人其他角色扩展仍暂缓。

## 回归与门禁

- 观察扩展后重新运行 Decimillipede 的 Fatal 交互与最终击杀探针，分别 6/6 ·119/119、2/2 ·2/2 一致，现直接比较结束时金币、牌组及 run RNG。
- 分发生成、C++ 构建成功；oracle .NET 构建 0 错误，原有 nullable 警告 CS8604 仍在 OracleRun.cs 的遗物获取调用处。
- `scripts/check.sh --quick` 退出码 0：CTest 8/8；冒烟 death=94 unported=6、death=100、boss=3 death=97。
- `git diff --check` 通过；完整 L2/L3/L4 门禁未在本批重跑。
- 正式遭遇 **86/90**，怪物内容模型头文件 **103/126**。剩下 KaiserCrabBoss、KnowledgeDemonBoss、QueenBoss、TestSubjectBoss；目标尚未完成。
