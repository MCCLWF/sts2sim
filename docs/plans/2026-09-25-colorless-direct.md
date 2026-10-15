# 无色牌直接效果批次

分支：`feat/m2-completion`。M3 已完成；本批只扩展战士可正常获得的单人无色牌，不扩展其他角色专属内容。

## 范围

`DramaticEntrance`、`EternalArmor`、`Impatience`、`Production`、`Prolong`、`Prowess`、`Shockwave`、`UltimateDefend`、`UltimateStrike`。

保留 `Splash` 跨职业牌池路径为显式未实现。多人专用无色牌不作为单人战士 run 的可获得内容。

## 验收

每张牌通过生成分发接入游戏卡牌管线，按 Oracle 在第一幕 Nibbits 遭遇上逐牌测试 A0/A10、两种子、两策略、基础与升级牌；完整 `scripts/check.sh` 通过。
