# 通用遗物直接钩子批次执行报告

分支：`feat/m2-completion`  
日期：2026-09-25

## 已接通

- **Dragon Fruit**：持有者获得金币后最大生命增加 1。
- **Bowler Hat**：持有者获得金币按 1.25 倍计算，并保留修改后钩子。
- **Planisphere**：首次房间且当前地图点为未知房间时回复 5 点生命。

三者都通过 `AbstractModel` 生成分发接入现有 Hook 顺序，没有绕过 run 管线。

## 验证

`tests/test_relics.cpp` 覆盖三种真实状态变化；`relics_common_m4` 通过。完整门禁随后通过：6 个原有测试加遗物测试共 7/7，烟测、L2、L3、L4 均无分歧。

## 边界

Girya/Shovel 的休息点自定义选项仍未实现，已知 `Unported` 边界保持不变；其他角色内容没有扩展。
