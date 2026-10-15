# Crystal Sphere 公共 API 验收

沿用完整 Env/RF API 分支；对应 RF 功能清单第 8 项。已有游戏逻辑和黄金样本不等于
公共 API 消费者可用，当前缺少棋盘的结构化查询与逐个 Choice 前缀的独立原版差分。

1. 在 run query 的 event 中输出 `crystal_sphere`：活动状态、尺寸、工具、剩余次数、
   原始棋盘、物品位置/类型和揭示顺序。隐藏格标记 `visibility=privileged`，不加入 Observe。
2. 操作沿用源 hash 绑定的 EventOption action，以及真实 Choice 的 cell/tool Answer；
   每次操作不自动选择下一个点击或奖励。保留 stationary base 与前缀恢复。
3. 新 test-only fixture 通过同一共享 runtime 导入完整事件根；独立正版 DLL oracle
   执行原始 EventOption.Chosen、CellClicked、CompleteMinigame 和奖励同步器。
   只替换 UI 等待及输入，TestMode=false 和注入资格单独记录。
4. 比较棋盘、物品、点击/揭示顺序、全部玩家/对局/event RNG、每项奖励与最终状态；
   trace on/off、clone/restore、失效引用和非法输入均须检查。覆盖双选项、大小工具、
   边缘/重复点击、揭示诅咒和最后一击的奖励选择；五角色/A0/A10 扩展矩阵。
5. 运行 API 差分、CTest 与完整 check.sh，记录未覆盖条件；稀有 placement retry
   的既有 Unported 不以固定棋盘成功掩盖，完整 API 目标仍保留。

该矩阵首轮暴露普通 typed Action 的上下文捷径：`Execute`/`PreviewOperation` 在
`stop=operation_done` 时转成普通 Step，Choice 的 snapshot 与 Answer result 丢失原请求。
修复新 typed Action 始终保留 Operation；typed Answer/Resume 仍延续原 root，不能覆盖
source identity 或 stop policy。增加完整选择、恢复及 typed/ordinary continuation 等价合同。
