# 共享遗物批次 13：Unsettling Lamp

## 前置状态

- 基于游戏 v0.111.0 源码 `Models/Relics/UnsettlingLamp.cs` 与 `ITemporaryPower` 实现移植。
- 本批次开始前，共享遗物模型头文件为 117/118，直接行为 hook 覆盖数为 48；唯一缺少模型头文件的是 `UnsettlingLamp`。
- M4 仍在进行中。铁甲战士获取其他角色牌、The Courier 的商店补货回调，以及其他尚未移植内容仍可能造成模拟在后续内容停止。

## 实现内容

- 新增 `UnsettlingLamp`：战斗开始时激活；记录首个由持有者卡牌施加、目标没有 Artifact 的可见减益；对这张卡持续翻倍减益；卡牌打出后停止；战斗结束时清空状态。
- 对 21 个生成内容中的 `ITemporaryPower` 模型映射内部 Strength、Dexterity 或 Focus。若已翻倍的临时力量牌继续施加其内部力量，则跳过内部力量的二次翻倍。
- 测试覆盖普通减益、同卡与异卡来源、打出卡牌后的停止、战斗结束与下场战斗重置、Dark Shackles 的二次翻倍保护，以及 Artifact 拦截后保留遗物触发资格。
- 生成器已将模型接入 `AllModels.h` 与相应 dispatch。共享遗物模型头文件达到 118/118，直接行为 hook 达到 49。

## 验收

命令：`./build/test_relics`

结果：退出码 0。

命令：`git diff --check`

结果：通过，无空白错误。

命令：`./scripts/check.sh`

结果：退出码 0，末尾为 `all checks passed`。原始结果摘要：

```text
100% tests passed, 0 tests failed out of 8
smoke: death=93 unported=7
smoke: death=99 unported=1
smoke: boss=4 death=96
Build succeeded.
    0 Warning(s)
    0 Error(s)
L2 newrun: 200 runs, 0 mismatched
L3 combat: 40/40 combats identical, 0 unported, 0 diverged, 784/784 states matched
L3 combat: 40/40 combats identical, 0 unported, 0 diverged, 827/827 states matched
L4 run: 40 runs: 40 identical, 0 stopped at unported content, 0 diverged; 1958/1958 decisions matched
L4 run: 40 runs: 40 identical, 0 stopped at unported content, 0 diverged; 2131/2131 decisions matched
all checks passed
```

## 范围说明

118/118 表示共享遗物均有 C++ 模型头文件，不等同于所有模型成员都已实现，也不表示 M4 完成。跨角色牌池与其他未移植内容仍按路线图继续推进。
