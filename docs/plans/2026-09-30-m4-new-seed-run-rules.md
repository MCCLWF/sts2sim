# 新 seed 首分歧规则修复

从 `5266daf` 独立分支验证；基线采集在另一冻结 worktree 继续，不覆盖旧结果。

首批深幕新 seed 已确认：

1. LargeCapsule 硬编码铁甲打击/防御；原版按 Owner.Character.CardPool 选择 Basic + Strike/Defend tag。
2. Dowsing.BeforeRoomEntered 空实现；原版仅主房间的 Unknown 地图点累计，第五次变为 Abundance。
3. Noisebot 生成状态卡误把 Owner 作为 creator；原版 creator=null，会错误触发 RocketPunch/Supermassive。
4. 长战斗 LocalCostModifier 容量溢出，待以源码和实测验证等价压缩；不得靠忽略或过度归一化错误通过。
5. RingingPower 以 CardPlaysStarted 当前回合历史判断；嵌套自动出牌时必须包含尚未完成的外层牌。
6. BingBong 的跳过集合记录卡对象身份，盗走后返还仍跳过一次；独立 clone 不继承集合成员关系。
7. Dampen 的恢复字典记录被降级卡对象身份，独立 clone 不继承恢复资格。

任务：逐条按原版源码修复；保存原版固定 trace 和首分歧，重放受影响样本并扩大至当前所有失败；报告未解决的新首因。模型文件优先，不改 Env/ABI；状态/GC 等共享文件先与 API 聊天协调。进入可合入候选后执行完整 `scripts/check.sh`。

原始批次：`/home/wst/Desktop/MCC/sts2sim/artifacts/legacy/mass-seeds-2026-09-30`。正常 HP 与 HP10000 注入分别保留，不作为正常通关率。
