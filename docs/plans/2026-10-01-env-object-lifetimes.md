# Env 对象寿命与模板身份

基线9e3fa62，沿用feat/env-rf-complete。完整E0–E5/因果追踪目标不缩减。
另一对话采集main的新1000局，main Git/source/build/Oracle保持冻结。

1. 先区分原版ModelDb不可变模板与ToMutable实例。CanonicalCard/Relic和EventModel.Canonical
   使用scratch承载字段，不能把scratch的pool生命周期当作原版对象生命周期。
   模板身份由原版kind+ID确定，不能暴露scratch槽当永久实例ref；派发需保留模板身份。
2. 对模板/实例的参数、来源、相等性用真实DLL扩大独立探针：null/canonical/mutable，
   bool+decimal out、正常默认/override等。保持原版方法单次执行，不用Postfix __args写回out。
3. 继续实现实例创建/退役/回收/引用重用账本和稳定UID；明确初始对象、pending/附件/事件、
   物理池回收与游戏对象脱离是不同事实。克隆、恢复和前缀重放保持身份；POD不能加入指针/堆分配。
   UID应来自真实创建/复用，不能凭模型ID/槽号猜测。Native观察语义不得假冒.NET GC时刻。
4. 观察关闭路径、完整状态/RNG不变、源与失败合同、全部对象类型/暂存引用/回收根/模板条件，
   DLL独立证据、公共SDK与RF、portable fresh process、容量/性能、完整scripts/check.sh。
5. 只有全生命周期与因果条件通过才能升级完整资格。模板基础增量不代表第3–4项已完成；
   所有旧build、失败原件与main规模证据独立保留。
