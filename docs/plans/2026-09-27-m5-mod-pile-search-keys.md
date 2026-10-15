# M5：Mod 牌堆保留键与循环键

分支 `feat/combatsolver-env-backend`，前置 `16c5c4c`。固定原 Beam 的单卡、球与能力键输入已局部对齐，但完整状态键与原评分仍未完成。

1. 从通用 getter v4 的前四个战斗牌堆及 Mod 单卡指纹生成原 `UnorderedPileKey`；保留键必须使用原 `StateFingerprintBuilder` 的 mix、标记与顺序。
2. 从原卡牌身份/升级和相同四堆生成 `CyclePileShapeKey`，逐位对照原 `BuildCyclePileShapeKey`。完整 `CycleShapeKey` 还需存活敌人位图、药水库存和搜索边界，不能把牌堆部分冒充完整键。
3. 保留 `AppendPile` 在四位 `PileOrderInvariantMask` 下的有序/无序完整编码输入接口；不能以这两个部分键宣称 `BuildStateKey` 完成。
4. 原 Beam 16 场实际 owned 分支各自逐位对拍无序牌堆键、循环牌堆形状部分及默认有序 `AppendPile` 片段；查询/API/完整 scripts/check.sh 保持通过；报告事实、范围和下一步。
