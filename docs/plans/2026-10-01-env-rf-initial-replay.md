# RF 初始操作跨进程差分计划

基线main/API 202afd9；继续完整E0–E5，引用校验8728b09已合入并关闭。现有fresh消费者
只恢复Choice前缀，36无选择变牌和4无选择商店成功仅检查父件，另4禁买请求未fresh执行。

1. 共用RF consumer保存每个主操作的initial local/portable/query/typed request，以及完整
   completed portable/query。商店禁买请求同样保存，setup夹具仍单列，不冒充fresh setup。
2. 两新进程从initial portable恢复，重绑定source_hash，实际执行每个主操作；有选择时
   精确核对第一个前缀及原方法/RNG trace，无选择时完整canonical后继和原版事实全对齐。
   原有125 Choice答案继续全部执行与完整核对，禁买无successor且完整源不变。
3. 新版fresh工具缺initial/漏操作/child oracle不等于原Native原件等均失败，禁止沿用
   旧工具静默跳过无选择路径。保持所有旧原件和资格，不将新工具重放写成新Native capture。
4. 使用TestMode=false新采集57有限主case，完整54CTest/L2/L3/L4门禁；记录全部实际EOF、
   源/runtime/输入SHA首尾、两预热顺序、损坏文件负例，补报告与路线图，再合入及跨树复核。
5. 不改游戏代码、ABI、RNG、POD或容量；完整RF功能/来源/条件、UID/lifecycle/live与规模性能
   继续未验收；57主操作续接与2个Driftwood setup路径的父Native证明分别报告。
