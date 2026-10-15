# Env active capture 引用验证

基线API组合bd4af26（模板317054f、HistoryCourse30e6f94与b2fddca关闭证据）。继续完整E0–E5目标；
根30e全1000原件重放冻结，全部修改仅API工作树。

1. 复现公共local v4在state_hash=null时接受越池/零引用/重复capture pair的实际请求；
   原始三请求与失败证据保留，源Env应始终不变。
2. 验证active_card_play_power_captures每项power/card非零、池内live、pair唯一；
   不清空合法pending，不限制任意合法signed amount，不改变原版capture执行和RNG。
   current与replay_base同用完整状态校验；portable包重算有效wire hash后也不能绕过。
3. 正例使用原版Monologue.BeforeCardPlayed语义建立真实capture，local/portable恢复精确保留，
   后续AfterCardPlayed正常消费并与未恢复分支完全一致。负例覆盖越界/零/dead/duplicate-live
   以及replay base，公共C ABI失败无successor且源snapshot保持不变。
4. 源规则按原版Dictionary对象键和配套capture helper核对；运行完整check.sh、DLL方法组件、
   更新RF/portable冻结版本证据。联合HistoryCourse的schema变化单独记录；旧包显式拒绝。
5. 正式报告如实区分：引用校验修复不解决capture容量、稳定UID、生命周期或完整API/RF。
