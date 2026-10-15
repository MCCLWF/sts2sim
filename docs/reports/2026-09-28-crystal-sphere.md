# Crystal Sphere 通用 Env 事件批次

日期：2026-09-28。分支：`feat/generic-env-rf-api`。真值是 STS2 v0.111.0 的
`Models/Events/CrystalSphere.cs` 和 `Events/Custom/CrystalSphereEvent` 源码，
以及直接运行正版游戏程序集的 [原版小游戏 oracle](2026-09-28-crystal-sphere-oracle.md)。
本批没有修改 CombatSolver 或其归档代码。

## 实现

- 事件初始选项、二幕且所有玩家至少 100 金的准入、`50 + NextInt(1,50)` 的支付费用、Debt 方案及 3/6 次占卜。
- 11×11 雾格、15 件物品的尺寸与原版顺序、每个物品的一次事件 RNG 落位、Big/Small 点击以及揭示顺序；Doubt 在揭示时立即加入牌组。
- 事件 RNG 在卡牌、药水、遗物和金币奖励上的使用。卡牌奖励必须直接构造 `CardCreationOptions`，不能用会禁用升级抽取的 `ForNonCombatWithUniformOdds`。
- 网格、物品位置、揭示顺序、剩余次数、工具、奖励 RNG 来源都在可复制状态中；快照导入会拒绝无效格值、揭示掩码和工具状态。
- `CrystalSphere` 的数值 DynamicVars 由内容导出器按 v0.111.0 源码回填；原版 getter 因同时构造 Godot 字符串变量而不能由无头导出器直接读取。

## 原版黄金样本

`SEED0002` Ironclad A0、二幕事件、支付计划，原版 oracle 和 C++ 定向测试相同：费用 66；费用后事件 RNG 计数 1，15 件落位后 16。C++ 校验了全部 15 件的坐标。

| 六次点击 | 原版与 C++ 共同结果 |
|---|---|
| `B:5,5;B:4,2;B:5,3;B:3,4;S:6,1;B:8,2` | 揭示物品索引 `[3,0,14]`；Fruit Juice、Vajra、30 金；奖励填充后事件 RNG 计数 19 |
| `B:7,7;B:1,4;B:1,2;B:6,9;B:9,6;B:0,6` | 揭示 `[4,5,6,7,1,2]`；Debt 后揭示时立即加入 Doubt；三组卡牌分别为 `BLOOD_WALL/HEADBUTT/BODY_SLAM`、`HOWL_FROM_BEYOND/WHIRLWIND/INFLAME`、`PYRE/STOKE/PACTS_END`；药水为 Fire Potion、Dexterity Potion；奖励构造后 RNG 18、填充后 36 |

原版 oracle 直接调用原版 `BeginEvent`、`CrystalSphereMinigame`、`CellClicked`、`ToReward` 和 `Reward.Populate`，没有驱动 Godot 奖励选择 UI。C++ 测试按相同顺序调用对应逻辑。因此以上证明物品和奖励的这两条路径，不是所有种子与所有点击序列的完整验收。

## 回归与限制

`cmake --build build --target test_relics -j2 && ./build/test_relics` 已通过两组原版黄金样本；
`dotnet build tools/Sts2Exporter/Sts2Exporter.csproj -c Release` 通过，零警告、零错误；
用该导出器重新导出到 `/tmp/mcc-content-crystal-check.json` 后，Crystal Sphere 的三个数值变量与仓库 `data/content.json` 一致（全文件仍有既存的三个遭遇 Slot 差异）；
`python3 tools/coverage/map_source_inventory.py --check docs/audits/map-source-inventory.json` 通过。
最终批次完整 `scripts/check.sh` 通过：CTest **16/16**，L2 **200/200**，L3 **80/80 战斗、1611/1611 状态**，默认 L4 **80/80 局、4089/4089 决策**，另有严格二幕、胜利、遗物及献祭路线零分歧。默认 L4 仍只到第一幕 Boss，并不能替代本事件的定向 oracle。

原版在 `PopulateItems()` 落位失败时会保留已放物品并最多再尝试九次。C++ 当前固定布局在 100000 次随机落位探索中未遇到失败；若真正发生，Env 明确返回 `Unported`，不会假装精确继续。需要补齐此罕见分支与真实客户端的小游戏输入/奖励选择桥。该 oracle 是直接调用原版小游戏的定向样本，普通 L4 尚未覆盖真实 Crystal Sphere 房间。
