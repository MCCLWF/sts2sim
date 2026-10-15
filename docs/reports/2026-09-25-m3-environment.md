# M3 环境层验收报告

> 状态修订：接口、药水动作、动作 mask 与完整隐藏信息 determinize 已完成。M3 最终验收见 [`2026-09-25-m3-closeout.md`](2026-09-25-m3-closeout.md)。Clone <1µs 是设计文档列出的 M5 性能目标。

分支：`feat/m3-environment`  
日期：2026-09-25

## 完成内容

- `Env::LegalActions()` 覆盖地图、战斗、奖励、事件、休息点、商店、宝箱和嵌套 `Choice`，动作顺序稳定。
- `Env::Observe()` 提供固定长度 2048 的整数观测；只编码当前可见状态，隐藏牌堆只给数量，不暴露 RNG 和未来房间队列。
- `Env::Clone()` 深拷贝整局与选择前缀；`Hash()` 覆盖决策点状态；`Determinize()` 只洗牌战斗中的抽牌堆和弃牌堆。
- `VectorEnv` 管理独立环境；`env/c_api.h` 和 `python/sts2sim_env.py` 提供无 Python 开发头依赖的 C ABI/ctypes 接口。
- `search/reference_ismcts.*` 提供小型、确定性、无外部依赖的 ISMCTS 风格参考搜索，用于环境契约验收。

## 验证

```text
ctest --test-dir build --output-on-failure
5/5 tests passed
```

专用测试覆盖 reset、合法动作、固定观测、clone 隔离、determinize 可见性、批量环境、Python ctypes 调用和参考搜索的同种子确定性。完整 `scripts/check.sh` 作为合入前门禁执行。

## 范围边界

当前阶段暂停其他角色的继续扩充，后续工作只推进铁甲战士、通用 run 内容和无色牌。跨职业卡牌获取路径仍显式未实现：铁甲战士的 `InfernalBlade`/`Stoke` 使用战士牌池；无色 `Splash` 查询其他角色牌池的选择流程保持 `Unported`，不会静默替换为战士牌。
