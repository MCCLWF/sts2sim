# 通用 Env：局外查询、选择描述与预览结果

日期：2026-09-27。分支：`feat/generic-env-rf-api`，起点 `ac4c77c`。归属 E1/E2，依赖当前普通 Preview/owned 分支与选择前缀。

## 目标

让公共消费者读取当前牌组、遗物/药水、奖励、商店、事件、休息、宝箱、地图动作及选择候选；在普通预览中可显式获取执行前后结果，支持 RF 的变牌、拾取、补货和重掷展示。执行继续使用原版动作/Cmd/Hook，不加入 RF 镜像算法或搜索政策。

## 工作

1. `env/run_queries.*`：普通房间/玩家查询；future 幕事实仅显式特权开启；禁止 RNG、遗物袋和未来队列混入默认查询。私有分支执行 getter，检查无逻辑状态写入。
2. ChoiceRequest 增加当前原版调用生成的描述数据，保留 card/bundle/reward/alternative/proceed 身份与顺序；选择 Hash/快照/恢复纳入描述，不推测丢失的 bundle 边界。nested rewards 的合法答案排除原版已领取/按钮禁用的 reward，描述继续保留完整列表。
3. 普通 Preview 默认成本/合同保持；新增可选的 before/after run 查询结果，明确这些是已观察状态投影，不替代完整因果 trace。
4. C ABI/Python 导出新接口，修复动作 kind/坐标窄化前校验；新 Choice 快照协议版本明确更新。
5. GPT-6 Luna xhigh 独立补 Driftwood、Outmaneuver/Flechettes；已存在的 DaggerThrow 保留原版实现并复核 draw/discard 选择路径。先生成/构建，再执行针对性差分。
6. 公共合同测试、原版字段差分及现有 gate；文档/报告记录实际覆盖与剩余缺口。

## 验收

- 查询前后源/私有分支 Hash 不变，不调用 Populate/CalcCost/生成或抽 RNG；不支持的 getter/Hook 明确失败。
- 商店项索引对应普通 ShopBuy；查询实际 Hook 后价格；奖励及 Choice 保留原版候选与 alternative 顺序。
- bundle 分组、多选 count/index、嵌套奖励和 CardReward Skip/Reroll 可以辨认并恢复后继续。
- 捕获结果的 Preview 与不捕获结果的普通 Predict 后继相同；源不变；失败不交付后继；Answer 比较源部分状态与重放后状态，不冒充整段因果差分。
- 真实 C ABI 拒绝非法 kind/坐标别名。查询及结果均为调用者显式使用的 API，不改变隐藏观测/Determinize。
- 相关内容的正版对拍与零 Unported 结果按实际日志报告；不宣称全 RF/Env 完成。

主要文件：`env/run_queries.*`、`env/preview.*`、`env/c_api.*`、`env/env.cpp`、`engine/sim/sim.h`、选择调用方、`env/snapshot*`、Python、CMake、专门合同测试与独立 `tools/check_run_queries.py`。GameState 保持 POD。容量和 refs 的寿命仍以 owned 状态为界，不宣称跨分支/跨进程全局 UID。
