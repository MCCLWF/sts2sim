# 地图生成：重复路段的等价跳过

分支 `perf/map-segment-memo`，基线 `acd5b7e`。小改动，无单独计划；在 NoSL Q2 采样测试中发现。

## 现象与首因

向量采样器的吞吐只有裸 Env 数字的十分之一。`Env::Reset` 单独计时约 20.3 ms（Silent A10，机器空闲，200 次平均）。
系统不允许 `perf`，用 gdb 对重置循环采样 10 次，10 次都落在
`generate_standard_map → Generator::find_matching_segments`，栈顶是字符串拼接、`memcmp` 和 `malloc`。

`find_matching_segments` 先列出起点到 Boss 的全部路径，再对每条路径的每个 (起点, 终点) 子段拼一个字符串键，
放进按键排序的 `std::map`。绝大多数路径共享绝大多数子段，同一段被反复拼键、查找、比较。

## 改动（只改性能，不改结果）

1. **跳过已提供过的点序列。** 同一点序列第二次提供时，键已存在，而且它一定与自己第一次的副本重叠，
   或与当初挡住那份副本的条目重叠，所以永远不会被加入。`seen` 集合在拼键之前把这些重复挡掉；每个列表及其顺序不变。
2. **深度优先边走边处理，不再生成路径列表。** 顺序与原 `find_all_paths` 相同（子节点从前到后）。
   完整落在“与上一条路径的公共前缀”内的子段已由上一条路径提供过，直接跳过。

`std::map` 的键、键的顺序、列表内顺序和随后的 `rng_.shuffle` 调用都没有变。

## 验证

| 检查 | 结果 |
|---|---|
| 改动前后 `Env::Reset` 后的状态 Hash，5 角色 × 1000 种子（`MAPEQ00000`–`00999`；Silent/Defect/Necrobinder A10，Ironclad/Regent A0） | 5000 个 Hash 全部相同 |
| `python3 tests/l2/compare_newrun.py --seeds 300`（与正版对拍开局和各幕地图） | `L2 newrun: 3000 runs, 0 mismatched`，退出 0，8 分 9 秒 |
| `Env::Reset` 耗时，Silent A10，空闲机器 | 20.3 ms → 7.3 ms（改动前 200 次平均；改动后 300 次平均） |
| 同上，五种角色/难度混合 5000 次平均 | 改动后 7.8 ms；改动前那次测量是 50.5 ms，但当时机器负载未确认，不作为对比依据 |
| `STS2_SRC='…/Slay the Spire 2/src/Core' scripts/check.sh` | 退出 0，末行 `all checks passed`，59/59 CTest，L2/L3/L4 全部一致；日志 `artifacts/map-segment-memo/check-full.log`，SHA256 `56ae72f42ba18ba435af3691bfbce8bce588762d9f54d02bbbf39262ee2028cf` |

状态 Hash 的对比只说明改动前后模拟器自身一致；与正版一致的证据是 L2。

## 过程中的一次失误

前两次重建用 `grep " error "` 过滤编译输出，漏掉了 `error:` 行，实际编译失败而动态库没有更新；
当时看到的“耗时下降”和“Hash 一致”都不是新代码的结果。改为检查构建退出码后重新编译、重新测量，上表是重测后的数字。

## 剩余

重置的 7 ms 仍然几乎全在地图生成（gdb 采样 16 次中 15 次），主要是集合与 `std::map` 的节点分配。
`prune_duplicate_segments` 每剪一次就整个重算一遍，增量化是下一步可做的优化，本次没有做。
