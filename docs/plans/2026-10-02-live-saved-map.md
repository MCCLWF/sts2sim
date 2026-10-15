# 读档及原版特殊地图捕获

用户在获得事件遗物恶魔之舌后遇到 `live_map_type_unmapped`。目标是补上原版地图类别的实机捕获，不重生成玩家地图，不把新局夹具当成续玩验证。

前置条件：完整模型字段及实际全解锁档案捕获已合入main。所属工作项为原版单人状态恢复。源码真值：`Map/SavedActMap.cs`、`RunManager.GenerateMap`、四类原版地图；恶魔之舌没有地图修改逻辑，因果以可复现证据为准。

任务：

1. 原版地图保存→`SavedActMap` 恢复，加恶魔之舌，使用未改行为源码建立失败基线。
2. 桥接明确支持 Standard / GoldenPath / Saved / Spoils 的原生图；保持节点、连边、顺序、可修改标志和任务标记。未知类别继续明确拒绝并附类别名。
3. 同一正版夹具逐点核验新生成/读档/特殊/双Boss图的native快照；原生Godot入口使用保存恢复图和恶魔之舌验证搜索。
4. 执行必要门禁、Linux构建部署、提交合入main；记录原始证据与未验证边界。

涉及：`EnvLiveRunContext.CaptureMap`、managed live fixture、Godot `ENV-COMPARISON` 与开发记录。native地图结构和游戏地图生成规则不变。
