# 通用遗物批次 14：Girya 与 Shovel 休息点

## 来源与实现

依据游戏 v0.111.0 `Models/Relics/Girya.cs`、`Models/Relics/Shovel.cs`、`Entities/RestSite/LiftRestSiteOption.cs` 和 `DigRestSiteOption.cs`：

- 单人休息点生成按 run hook listener 顺序添加选项。持有 Girya 且举重次数小于 3 时添加 Lift；持有 Shovel 时添加 Dig。
- Lift 每次将 Girya 的计数加一，第三次后不再生成选项；后续每次进入战斗房间时，按计数施加 Strength，并对外显示计数器。
- Dig 按玩家奖励 RNG 抽取当前遗物袋的下一件遗物，再通过 `RelicCmd::Obtain` 获得，与游戏命令顺序一致。
- 其他仍未实现的自定义休息点选项继续走 `Unported`；多人 Mend 不属于本模拟器的单人范围。

## 验证

- `python3 tools/gen/gen_dispatch.py`：通过，670 个已移植模型；RelicModel 分发包含 155 个实现分支。
- `cmake --build build -j2 --target test_relics && ./build/test_relics`：通过，退出码 0。
- 新增用例验证 Girya 可举重三次、计数显示、上限后选项消失，以及下一场战斗获得 3 Strength；Shovel 的 Dig 选项成功抽取并获得一件遗物。
- `./scripts/check.sh`：通过。ctest 8/8，冒烟无崩溃或卡死，L2 200/200；L3 两组各 40/40（784/784、827/827 状态）；L4 两组各 40/40（1958/1958、2131/2131 决策）；0 分歧、0 `Unported` 停止。Oracle 构建 0 错误，保留一条既存 CS8604 可空性警告。

## 限制

该批只完成 Girya 和 Shovel 的单人休息点路径，不代表所有遗物行为钩子或所有自定义休息点选项已完成。铁甲战士获取其他角色卡牌仍未实现。
