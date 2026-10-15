# 新自然种子 v2 规则缺口

基底 `8d94dfa`，分支 `fix/unmovable-block-history`；已接入 API 聊天的 `15e3d84` 自动药水丢弃最小补丁（本树 `b6a8893`）。属于 E3/M4 条件补覆盖，前置 shared reward options、free-star、coverage-v2/harness 与 custom reward completion 已集成。已读 AGENTS、design-engine、roadmap、deviations 与 pitfalls。冻结 8000 身份批次另树运行，本工作不修改其程序或 Oracle。

原始批次采用 800 个新 run seed × 5 职业 × A0/A10，每个身份独立 policy seed。正常 HP、Neow、coverage-v2；原件保留全部失败和终局。无头 Oracle/导出投影资格不能当作真实 UI 全动作集合认证。

## 目标和源首因

- Unmovable：原版按本回合其它 CardPlay 的 BlockGainedEntry 数与 Amount 比较；旧移植始终翻倍。使用 POD entry 总数及 round/side/player-turn stamps；Clear epoch 排除被清理的旧记录；临时 CardPlay 记录自身同 stamp/epoch entry。不能以 card uid 或出牌数代替对象身份，也不能在 extra-turn player-turn 尚未递增前盲清源历史。
- TouchOfInsanity：照 `CostsEnergyOrStars(false) || CostsEnergyOrStars(true)` 筛选，排除零/负费用；单个选项由原版直接选取，不制造选择点。
- SneckoOil：照原版排除 X 费用和当前本地负费用，再抽 CombatEnergyCosts；不补抽 RNG。
- Glitterstream：先经 Hook.ModifyBlock 计算下回合 block，再 GainBlock，保留 Frail 等修改和顺序。
- Gigantification：归属判断是来源卡牌所有者而非 attacker，Osty 可用；无捕获时合法 damage query 也是 ×3。消费按实际 AttackCommand 对象身份，嵌套同卡/异卡均不消费外层。
- Inky：附魔加载 canonical DynamicVars，而非把唯一变量归零；保持 Adroit 后续按 Amount 重算。

## 任务和验收

1. 原版 v0.111.0 DLL、TestMode=false primitive：16 格挡历史、10 宠物/嵌套攻击身份、1 Inky canonical variable；期望来自原版。
2. 新字段同步 generated、snapshot validation；POD/trivial-copy 保持。静止点快照 successor 对照；不保存临时对象地址。
3. 重放全部 66 条原始失败，严格比较完整终局、选择、导出状态、RNG counters 和逐点 snapshot successor；保留失败修复过程。
4. 对新版本重放全部 8000 身份，再复验旧失败/跨幕定向。执行完整 scripts/check.sh；不得把自然首幕死亡当作深层覆盖。
5. 报告实际数量、首因、命令、hash 与资格；同步 README/roadmap/deviation。可 cherry 提交交 API 聊天集成，不宣称 API 完成。

## 涉及文件

状态/generated/snapshot validation、CombatHistory/CardPlay、AttackCommand/Gigantification、CardModel cost predicate、三个 potion/card 模型、CardCmd enchantment init；原版 probe 工具和数据驱动 primitive CTest。原始大体积产物保留本地，不提交游戏 DLL/源码。
