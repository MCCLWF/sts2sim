# L3 自定义牌组验收报告（2026-09-25）

## 前置条件

- 分支：`feat/l3-custom-decks`。
- 游戏版本：v0.111.0，oracle 与 C++ 均使用全解锁存档。
- 已阅读 `AGENTS.md`、`docs/design-engine.md` §6 和 `docs/audits/pitfalls.md`。

## 实现摘要

- oracle 在 `RunState` 初始化后、进入战斗前使用 `CardPileCmd.RemoveFromDeck`、`RunState.CreateCard`、`CardPileCmd.Add(..., PileType.Deck)` 和 `CardCmd.Upgrade` 替换牌组；额外遗物按给定顺序调用 `RelicCmd.Obtain`。
- C++ 使用对应的 `RunState::CreateCard`、`CardPileCmd`、`CardCmd` 和 `RelicCmd` 调用，顺序与 oracle 一致。这些调用不主动抽取 RNG；`CardPileCmd::Add` 使用底部位置，也不抽取随机数，因此不会改变进入遭遇前的 RNG 流。
- 战斗选牌在 C++ 中采用 `GameState` 静止点快照和 `Sim::set_prefix`：动作触发 `NeedChoice` 时恢复快照，消费 `s<N>:i1.i2...` 的数量和下标答案后重放。这样保持引擎选择前缀约束，不在战斗循环中增加状态机。
- 选牌决策输出沿用 oracle 的 `decision` / `action` JSON；`tests/l3/l3_combat.py` 对比选项、数量和卡牌顺序，忽略 `options.source`。

## 验收命令与原始结果

```text
DOTNET=~/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds L3CHK0001 --policy-seeds 1 --ascension 0 -v
ok    L3CHK0001 IRONCLAD policy 1 ['SLUDGE_SPINNER'] (22 states)
L3 combat: 1/1 combats identical, 0 unported, 0 diverged, 22/22 states matched
```

```text
DOTNET=~/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds L3CUSTOM06 --policy-seeds 1 --ascension 0 --encounter SLIMES_WEAK --deck STRIKE_IRONCLAD+1,STRIKE_IRONCLAD,DEFEND_IRONCLAD+1,DEFEND_IRONCLAD,BASH+1,BASH --relics AMETHYST_AUBERGINE -v
ok    L3CUSTOM06 IRONCLAD policy 1 ['TWIG_SLIME_S', 'LEAF_SLIME_M', 'LEAF_SLIME_S'] (22 states)
L3 combat: 1/1 combats identical, 0 unported, 0 diverged, 22/22 states matched
```

```text
DOTNET=~/.dotnet/dotnet python3 tests/l3/l3_combat.py --seeds L3CUSTOM05 --policy-seeds 1 --ascension 0 --encounter SLIMES_WEAK --deck BODY_SLAM -v
stop  L3CUSTOM05 IRONCLAD policy 1 at unported content: Cards.BodySlam.OnPlay is not ported
L3 combat: 0/1 combats identical, 1 unported, 0 diverged, 1/21 states matched
```

```text
scripts/check.sh --quick
quick check passed (differential checks skipped)
```

```text
scripts/check.sh
L2 newrun: 200 runs, 0 mismatched
L3 combat: 40/40 combats identical, 0 unported, 0 diverged, 784/784 states matched
L3 combat: 40/40 combats identical, 0 unported, 0 diverged, 827/827 states matched
all checks passed
```

计划中的 3 个遭遇、A0/A10、每组 16 个种子的批量结果：

```text
SLIMES_WEAK A0   16/16 combats identical, 298/298 states matched
NIBBITS_WEAK A0  16/16 combats identical, 187/187 states matched
CULTISTS_NORMAL A0 16/16 combats identical, 299/299 states matched
SLIMES_WEAK A10  16/16 combats identical, 307/307 states matched
NIBBITS_WEAK A10 16/16 combats identical, 208/208 states matched
CULTISTS_NORMAL A10 16/16 combats identical, 272/272 states matched
```

六组均为 `0 unported, 0 diverged`。

oracle 构建和 C++ 构建均通过；完整 `scripts/check.sh` 需在有游戏安装的环境中运行。
