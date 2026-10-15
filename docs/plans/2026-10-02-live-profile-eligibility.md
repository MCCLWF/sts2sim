# 实机全解锁档案误拒绝

目标：修复已解锁、已完成游玩的真实档案被 `UnlockState.all` 遭遇全集门槛拒绝的问题；保留输入真实值，不伪造全解锁或扩大到部分解锁/初次游玩。

前置条件：通用 Env 的 profile 字段、实机根捕获和 Linux 本地部署已完成。所属工作项为真实游戏状态恢复；D-001 限制继续生效。

任务：

1. 用实际对局保存的 `unlock_state` 建立未修改行为源码的失败基线。
2. 对照 `UnlockState.cs` 与 `ActModel.ApplyDiscoveryOrderModifications`：遭遇历史只有 boss discovery 顺序影响当前 veteran 假设，普通/事件遭遇未见过不等于内容未解锁。
3. 桥接、组装器、native 一致要求全部时代、正跑局数，以及各幕 discovery boss 已见过；原始遭遇位图继续完整存入快照。
4. 增加真实档案导入/搜索和负例验证，执行合入门槛、构建部署 Linux Mod，合入 main 并删除任务分支。

涉及文件：`EnvLiveRunContext.cs`、`EnvLiveRootAssembler.cs`、`env/snapshot_validation.cpp`、snapshot 与 managed/native live fixture 测试、相关开发记录。状态字段和 schema 格式不变。

验收：真实 57 个时代、46 局、75 个遭遇的对局档案可捕获/导入/搜索；遭遇历史位图及跑局数不被覆盖；缺少时代、discovery boss 或零跑局数仍拒绝；`scripts/check.sh` 通过。
