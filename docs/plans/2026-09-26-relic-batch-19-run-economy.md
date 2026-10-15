# M4 遗物批次 19：战士回复与通用资源规则

## 目标

按游戏源码移植 Black Blood、Darkstone Periapt、Sozu、Ectoplasm 和 Runic Pyramid。范围只包含战士和通用遗物，不扩展其他角色卡牌。

## 工作项

- Black Blood：战斗胜利后存活时按动态变量回复生命。
- Darkstone Periapt：持有者的诅咒进入牌组时增加最大生命。
- Sozu：增加最大能量，并禁止持有者获得药水。
- Ectoplasm：增加最大能量、把持有者获得的金币改成零，并触发相应遗物反馈。
- Runic Pyramid：阻止持有者回合结束时清空手牌。
- 添加覆盖实际命令与 Hook 链路的行为测试，强制持有遗物进行 Oracle L4 对拍，更新进度与报告。

## 验收

- 生成器、构建、`ctest`、定向 L4 和完整 `scripts/check.sh` 通过。
- 定向测试验证五件遗物的实际效果及与现有 run/combat 命令组合。
- M4 保持进行中；其他角色卡牌和跨职业获取继续显式未移植。
