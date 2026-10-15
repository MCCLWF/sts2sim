# 执行报告：整局骨架（M1-g、M1-i、M1-j）与 M1 验收

- 日期：2026-09-25
- 分支：`feat/run-skeleton`（计划：[`plans/2026-09-24-m1-run-skeleton.md`](../plans/2026-09-24-m1-run-skeleton.md)）
- 结论：**M1 完成**。三条验收标准的实际状态见第 3 节。

## 1. 做了什么

**选择前缀（M1-g）**
- `env/Env`：顶层决策点快照；效果中途的选择抛 `NeedChoice`，环境带着答案前缀从快照重放。
- 选择类型：
  - `CardSelectCmd` 全部选择路径：手牌、战斗牌堆、牌组的升级 / 变牌 / 删除 / 通用、三选一、网格。有序子集编码：先答数量，再按选择顺序答下标。
  - 奖励三选一。
  - 选包（`FromChooseABundleScreen`）。
  - 嵌套奖励界面（`RewardsCmd.OfferCustom`：效果会等奖励界面关掉再继续执行）。

**整局骨架（M1-i）**
- 地图：移动；问号房（`UnknownMapPointOdds` 与商店黑名单）。
- 战斗奖励：金币、药水概率、卡牌奖励。
- 房间：
  - 事件框架与 Neow。
  - 休息点：`RestSiteOption.Generate`，休息、锻造，单人版 `RestSiteSynchronizer`。
  - 宝箱：`BeginRelicPicking` 抽遗物，开箱给金币，玩家选遗物或跳过。
  - 商店：5 张角色卡（其中一张打折）、2 张无色卡、3 件遗物、3 瓶药水、删牌服务；购买、删牌、购买后刷新。
- 对象回收（GC）。
- 新增的命令：
  - `CardCmd.Transform` / `TransformToRandom`（含 `CardTransformation` 与变牌候选）；
  - `CardCmd.Afflict` 与苦难框架（`AfflictionModel` 的成员与分派、未上卡的苦难）；
  - `CardPileCmd.RemoveFromDeck` / `AddCurseToDeck`；
  - `CardFactory.CreateForMerchant`。
- 未移植类型 hook 的守卫统一放在 `game/Hooks/HookGuard.h`：有监听者覆写就抛 `Unported`。
- 生成的 `IsUsable(kind, id)` 表：哪些模型可以用，不会碰到 `Unported`。

**L4 整局对拍**
- oracle `run`：
  - 休息点、商店（含随机购买）、宝箱都按客户端的顺序驱动。
  - UI 节点负责的状态改动由 oracle 补上：宝箱遗物的获得、删牌后标记已用。
  - 选包由策略选择，替代 TestMode 固定取第一个包（O-009）。
  - 支持 `--no-neow`（不带 Neow 开局）和 `--hp N`（测试辅助：两边一起把开局最大生命设为 N，让随机策略活到后面的房间）。
- C++ `replay` 和 `tests/l4/l4_run.py` 逐个决策点比对。

**冒烟（M1-j）**
- `sts2sim_cli smoke`：随机智能体处理全部决策类型。
- 选项：
  - `--ported-only`：只选可用内容；
  - `--greedy`：战斗里一步前瞻；
  - `--hp N`：开局最大生命。
- 已接进 `scripts/check.sh`。

## 2. 发现的问题

- 宝箱遗物由 UI 节点获得，删牌的"已用"也由 UI 节点设置。无头 oracle 原来拿不到宝箱遗物，删牌也可以反复使用。已修复，记入 `docs/audits/pitfalls.md`。
- Codex 的 `SurprisePower` 移植有两处和 C# 不一致：`MarkGoldStolen` 的调用顺序，以及缺了 encounter 类型检查。已修正，Gremlin Merc 80/80 一致。
- oracle 原来在休息点执行完选项才输出 rest 动作，锻造中途的选牌因此排到了 rest 前面，重放时报错。已改成先输出动作。

## 3. M1 验收

| # | 标准 | 状态 |
|---|---|---|
| 1 | L2：开局和地图零分歧 | ✅ `scripts/check.sh`：5 角色 × A0/A10 |
| 2 | L3：铁甲战士起始牌组打第一幕首层遭遇零分歧 | ✅ 8 个弱遭遇 2000 场；另有 23/28 个常规/精英遭遇 |
| 3 | 随机智能体从开局走到第一幕 Boss 前，不崩溃，不卡死 | ✅ 见下 |

第 3 条的依据：

1. **L4 整局对拍零分歧**（每组 100 个种子 × 2 个策略）：
   ```
   python3 tests/l4/l4_run.py --seeds M1R0000..M1R0099 --policy-seeds 1,2 --no-neow
   L4 run: 200 runs: 68 identical, 132 stopped at unported content, 0 diverged; 7498/13074 decisions matched
   python3 tests/l4/l4_run.py --seeds M1R0000..M1R0099 --policy-seeds 1,2
   L4 run: 200 runs: 1 identical, 199 stopped at unported content, 0 diverged; 316/13888 decisions matched
   python3 tests/l4/l4_run.py --seeds M1H0000..M1H0099 --policy-seeds 1,2 --no-neow --hp 400
   L4 run: 200 runs: 4 identical, 196 stopped at unported content, 0 diverged; 9249/33735 decisions matched
   ```
   - 最后一组里一致的决策点：map 735、combat 7112、rewards 1080、card_select 256、shop 60、rest 3、relic_select 3。
   - "identical"指整局逐点一致：跑到战死，或者跑到 Boss 前。oracle 的随机策略在真实游戏里多半在第 3～5 层战死。
   - 带 Neow 的一组停在 Neow 遗物上（正在移植）。
2. **冒烟 0 崩溃 0 卡死**（300 个种子）：
   ```
   smoke                                                  death=5  unported=295
   smoke --no-neow 1                                      death=94 unported=206
   smoke --no-neow 1 --greedy 1                           death=79 unported=221
   smoke --no-neow 1 --ported-only 1 --greedy 1 --hp 999  boss=2   unported=298
   ```
   - 每局都以 Boss 前、战死或"遇到未移植内容"（受控停止）结束。
   - 开局加血后，有整局一路走到了 Boss 前，说明每层的流程都走得通。

## 4. 已知限制（M2 的工作）

- 停下的原因都是内容缺口，按次数排：
  - 铁甲战士卡牌 `OnPlay`；
  - 第一幕事件；
  - Neow 遗物；
  - 5 个遭遇（召唤、苦难）；
  - 遗物与药水。
- 这些都已在 Codex 分支上移植（见路线图的分支表）。
- 环境层的动作里还没有使用药水（M3）。
- `CombatHistory` 里苦难等记录目前是空实现。第二幕的 `ChainsOfBindingPower` 会读它，移植时要补上。
