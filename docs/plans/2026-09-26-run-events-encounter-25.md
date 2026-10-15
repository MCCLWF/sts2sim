# M4 run 内容批次 25：Bugslayer 与事件奖励牌

## 目标

补齐第三幕 Bugslayer 事件的两条奖励路径，并实现其依赖的事件牌 Exterminate、Squash 与诅咒牌 Bad Luck。

## 工作项

- Bugslayer：两项选择分别将 Exterminate 或 Squash 加入牌组并结束事件。
- Exterminate：对所有敌人造成 3 点伤害，重复 4 次；升级每次伤害 +1。
- Squash：对目标造成 10 点伤害并施加 2 层易伤；升级伤害 +2、易伤 +1。
- Bad Luck：不可生成、不可升级、永恒且不可打出；手中回合结束时造成 13 点无视格挡/无力量伤害。
- 添加事件、卡牌行为测试；不扩展其他角色卡牌。

## 边界

不实现跨职业卡池获取；未覆盖的后续事件、遭遇仍按 `Unported` 处理。
