# 商店公共 API 原版矩阵

前置：完整 Env/RF 计划 E0–E5 未完成；aaff4d9 已整合六类新 seed 修复，51 CTest 及完整 L2/L3/L4 门禁通过。当前工作沿用 feat/env-rf-complete，不缩小完整 API 目标。

商店 typed operation 已调用 MerchantEntry.OnTryPurchaseWrapper 对应实现，但丢弃 bool 结果；药水空间不足可能完成操作却没有成功购买。将原生返回值作为 operation_result 输出，保留 pending Choice 期间未完成状态，前缀恢复后重新执行取得结果。事实不参与游戏状态/RNG/hash，不根据金币差猜测购买成功。

增加每个查询商店条目的 entry_index（AllEntries 顺序），区别普通 ShopBuy 使用的 stocked action_index。原版探针依次执行同一槽位采购，比较完整物品/价格/库存/折扣/金币/全部 run、player RNG。覆盖普通店及 Courier、重复补货、满药水槽和删牌选择；卡牌选择须保存全部前缀并用公共 C ABI clone/snapshot/Answer 续接。

测试 fixture 明确使用全解锁新局、不进 Neow、金币注入、把起点类型覆盖为 Shop，然后走双方原版地图入房流程。C++ 单独测试共享库生成 fixture，并经同一已加载 runtime 的公共 snapshot import 返回 owned Env；之后消费者只能用公共 C ABI。fixture 不加入产品 ABI，不当自然可达路线或实机采集证据。

验收：固定 source/runtime 的严格 DLL 矩阵、源状态不变、trace 开关等价、购买原生失败和请求非法的区分、已售/不足金拒绝、全部选择前缀恢复；完整 scripts/check.sh。记录实际通过/失败和 TestMode 适配资格；trace_complete 继续 false，profile/live capture/完整 RF 等未完成项不撤销。

范围：env/operation、env/env、env/preview、env/run_queries，独立 OracleEnvOperationProbe，tests/api fixture 与差分消费者，CMake 及本计划/报告/路线图。另一个对话的 OracleRun/replay/policy 和冻结大批不修改。
