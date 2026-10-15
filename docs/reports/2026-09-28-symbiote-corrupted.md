# Symbiote / Corrupted Env 移植报告

## 范围

本批按游戏 v0.111.0 源码完成普通事件 `Models/Events/Symbiote.cs` 和附魔
`Models/Enchantments/Corrupted.cs`。没有改动搜索策略、CombatSolver 或旧的专用
预测接口。

## 源码语义

- `IsAllowed` 只允许 `CurrentActIndex > 0`。
- 初始选项按源码顺序生成：可附魔攻击牌时提供 `APPROACH`，否则提供锁定的
  `APPROACH_LOCKED`；第二项始终是 `KILL_WITH_FIRE`。
- `APPROACH` 使用牌组附魔选择器选一张合法牌，施加 1 层 `Corrupted`，然后结束事件。
- `KILL_WITH_FIRE` 使用牌组变牌选择器选 `CardsVar(1)` 张牌，并按事件 RNG 顺序逐张
  随机变牌，然后结束事件。
- `Corrupted` 只接受 Attack；拒绝 Status、Curse、Quest、牌组中的 Unplayable 和已有
  附魔牌。
- `Corrupted` 对 powered attack 返回 `1.5` 倍伤害；每次牌打出后对持有者造成 2 点
  `Unblockable | Unpowered | Move` 伤害。

导出的 `content.json` 没有保存 Symbiote 的 `CanonicalVars`（其中 `CardsVar(1)` 是
固定值），因此 C++ 保留了同一个常量 1，并在事件头文件中记录了这个导出限制。

## 修改

- `game/Models/Events/Symbiote.h`
- `game/Models/Enchantments/Corrupted.h`
- `game/Models/EnchantmentModel.cpp`
- `game/Models/EnchantmentModel.h`（过时注释修正）
- `game/Commands/CardCmd.cpp`
- `game/generated/AllModels.h`、`game/generated/Dispatch.cpp`
- `docs/audits/map-source-inventory.json`（移除 Symbiote 的未移植标记）

## 验收

```text
cmake --build build -j2                         PASS
ctest --test-dir build --output-on-failure      16/16 PASS
```

源码优先的 oracle 对拍：

```text
SEED0007 policy 1, --stop victory --verify-snapshots
695/1371 decisions identical; next blocker is ChoicesParadox.AfterPlayerTurnStart

SEED0007 policy 2,3,4,5, --stop after-act2 --verify-snapshots
4/4 identical; 2839/2839 decisions; 0 divergence
```

这些路线覆盖了 Symbiote 的选项生成和不同策略分支；当前三幕终点仍受其他未移植内容
限制，不能据此宣称完整铁甲战士闭环。

```text
python3 tools/coverage/map_source_inventory.py --check \
  docs/audits/map-source-inventory.json                    PASS
git diff --check                                             PASS
```

## 剩余工作

`ChoicesParadox` 和 Darv 等后续源码批次仍需按相同流程完成。Symbiote 的真实档案、
UI hover tip 和本地化展示不属于 Env 状态语义，本批未加入搜索或 UI 逻辑。
