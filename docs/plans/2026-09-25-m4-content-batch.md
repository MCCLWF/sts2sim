# M4 共享内容批量移植计划

分支：`feat/m2-completion`。M3 暂跳过；本计划先把 M4 中不依赖新环境 API 的跨角色内容批量接入，并以 Oracle L3/L4 逐批验收。

## 范围

1. Silent 起始牌与第一批常规牌：Strike/Defend/Neutralize/Survivor、Backflip、Acrobatics、Leg Sweep、Sucker Punch、Deflect、Dagger Throw、Deadly Poison、Poisoned Stab、Dodge and Roll、Shiv、Dagger Spray、Blade Dance、Cloak and Dagger。
2. 共享机制：Poison、BlockNextTurn、Pen Nib、Ring of the Snake，以及卡牌耗尽的回合历史。
3. 保持 Ironclad M2 的严格 L4 门槛，每批用自定义牌组和真实 Oracle 轨迹验证。
4. 后续批次继续补齐 Defect OrbQueue、Regent 星星、Necrobinder/Osty、第二/三幕怪物、事件和先古；这些依赖范围外引擎扩展，不能在本批次静默标记完成。

## 验收

- 每批 `cmake --build`、`ctest`、目标角色 L3 自定义牌组对拍。
- M2 四组严格 L4 不回归。
- M4 每个批次的实际种子、动作数、未移植项和首个分歧写入执行报告。
