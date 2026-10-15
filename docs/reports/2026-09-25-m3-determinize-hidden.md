# M3 隐藏状态 determinize 报告

分支：`feat/m3-determinize-hidden`  
日期：2026-09-25

## 完成内容

- 战斗中的抽牌堆、弃牌堆继续按 seed 重排。
- 当前及未来 Act 的事件、普通遭遇、精英遭遇队列只重排未访问后缀，已消费前缀保持不变。
- 当前/共享剩余遗物袋及其刷新来源列表重排；活动中的事件、奖励、商店、宝箱、地图和战场对象不被改写。
- 使用命名 determinize RNG 子流，保证同一 seed 得到同一隐藏状态，不影响原始 run RNG 计数。

## 验证

- `tests/test_env.cpp` 验证同 seed 隐藏哈希相同、不同 seed 隐藏哈希不同。
- 两种 seed 都保持固定观测和合法动作完全一致。
- 完整 `scripts/check.sh` 作为合入门禁。

M3 的接口验收至此完成；Clone 微秒级性能和全局可逆 action 编码属于 M5 性能/规模工作。
