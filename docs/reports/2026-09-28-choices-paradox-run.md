# Choices Paradox 整局批次

## 源码与修改

真值为 v0.111.0 `Models/Relics/ChoicesParadox.cs`。第一回合开始时，持有者从已解锁角色卡池抽取五张不同的战斗可生成牌，对五张都施加 `Retain`，显示简单网格，选择一张加入手牌。空候选时原版记录 softlock 错误并返回；Env 当前直接返回（全解锁的正式卡池非空）。

新增 `game/Models/Relics/ChoicesParadox.h`，复用通用 `CardFactory::GetForCombat`、`CardCmd::ApplyKeyword`、`CardSelectCmd::FromSimpleGrid` 和 `CardPileCmd::AddGeneratedCardToCombat`，再由 `tools/gen/gen_dispatch.py` 生成分发。未修改 CombatSolver。

## 实测

```text
cmake --build build -j4                                        PASS
DOTNET=$HOME/.dotnet/dotnet python3 tests/l4/l4_run.py \
  --seeds SEED0007 --policy-seeds 1 --no-neow --hp 10000 \
  --stop victory --verify-snapshots --verbose                  PASS
ok SEED0007 A0 policy 1 (1371 decisions)
L4 run: 1 runs: 1 identical, 0 stopped at unported content, 0 diverged; 1371/1371 decisions matched
```

完整 `scripts/check.sh`：PASS，包括 16/16 CTest、L2 200/200 新局、L3 80/80 战斗、默认 L4 80/80 第一幕路线、既有严格二幕/终局路线、新增 `SEED0007` 严格终局路线和 Pael's Wing 路线。

这是一条高生命值定向路线的证据。所有内容的源码审核与全角色随机长局验收尚未完成；定向通过不表示全量内容完成。接下来的新方向是先完成原版全量内容清单和批量移植，再用每轮新生成并保存的 100+ 个种子做整局差分。
