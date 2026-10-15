# Q4-ag：固定标签下有上限的EndTurn更新

- 前置main `b5ed355d34f20028c6f20f6325a717e479f2f3d2`，Q4-af诊断已封存。C1/C3旧动作上下文头比X1差约8.7–8.9层；其训练root Bernoulli KL约.63/.62，并非小更新。早期覆盖缺失存在，但恢复一个旧动作没有跨两C稳定收益，不把它当已证实主因。
- 问题：不增加标签、不改变网络/采样分布，只限制初始E头→新头更新的实际Bernoulli KL，是否避免无上限更新的整局退步，并带来超过E/X1的净收益？完整Silent A10胜率和Linux NoSL仍是最终目标，本项层数门槛只决定研究是否继续。

## 冻结输入与学习

1. 全部离线/本地CPU4、nice19、Torch/OMP/MKL/OpenBLAS1线程，云不power_on。X1/Q4-ac E1/E3/Q4-ad450root/Q4-ae缓存与Q4-af证据只读并验证manifest/文件SHA；不用Q4-af旧评价种子训练。
2. 两初始化seed1/3，每seed两个同容量27138参数、action_context=true的头U/T。从对应原E开始，完整X1冻结；只有原head+残差更新。U调用原Q4-ae学习代码，必须重现旧C的全部学习tensor与完整Adam数值，否则停止结论/评价。
3. 两臂同450root、352训练anchor/177方向标签/49开发留出，原margin权重、同seed minibatch顺序、fresh Adam .0003、gradnorm1、128epochs/minibatch128、256次优化器尝试。目标仍原weighted BCE+.1初始Bernoulli KL。保存最终epoch128和完整Adam，不用留出/中途权重挑选。
4. T仅加全352训练root的**mean KL≤.02、max KL≤.2 nats**硬上限，方向initial||new。每Adam proposal后从上个参数向proposal以alpha1,1/2,...,2^-20回溯；失败恢复旧参数alpha0，Adam moments仍累计原proposal梯度（投影Adam，完整保存）。使用.0199/.1999数值余量，逐步记录proposal/accepted KL、alpha和实际参数变化；不改部署推理或近tie规则。
5. 只声称训练root的Bernoulli限额，不当作整个贪心策略/未见状态KL上限。训练/缓存/合同合计上限180秒。T拟合降低原样报，不沿用旧“.8拟合才准评价”要求逼过拟合；进入评价需完整学习、有限参数、所有约束逐步成立、最终weighted BCE至少降低.001且有实际参数改变。未学习的恒等头不扩评。

## 资格与实际整局评价

- 四最终U/T在3209现有公共状态核验原tensor/value、非适用输出、非EndTurn argmax/mask不变，格式错误和generic-loader明确拒绝；原型尚无C++/Linux加载适配，不宣称部署就绪。
- 本地pilot E1/U1/T1/E3/U3/T3各4新`NQ4AGPILOT000000–000003`正常Silent A10，不注入，每臂120秒/30000步上限。全部24实际终局须原版TestMode=false整局快照/全部终局零差分；独立学习/Adam/原始标签/缓存核验及完整`scripts/check.sh`通过后正式评价。
- 正式X1/E1/U1/T1/E3/U3/T3各1024新`NQ4AGEVAL000000–001023`、CPU float32、原64槽采样；全局1800秒/30000步，全部尝试与终局/故障保留，不补seed。冻结最终权重、规则、代码和解析配置；按原版实际胜利/死亡计完整跑局，A10第二最终Boss胜利才能计成功。
- 预设机制门：两T相对对应U平均层数改善≥2且配对bootstrap95%下界>0。预设净收益门：两T各自相对E及X1平均层数改善≥.5、95%下界>0，且第三幕到达不少于X1。bootstrap10000、rng20261006、同seed配对。所有模型完整胜率及区间另报；机制门通过只证明此固定上限协议减轻退步，不自动换X1/称高水平。
- 每个E/U/T臂正式首2局+最深终局去重+全部胜场原版TestMode=false完整回放；独立复算raw统计/选择/区间。任何合格故障不得默认死亡，负结果不加样本或再调上限。

## 交付和范围

本计划/独立分支/如实报告，新工具`tools/nosl/endturn_trust_bound.py`、评价工具、独立审计工具；README/roadmap只stage自有进度前缀，保留外来未提交工作。生产actor、游戏、奖励、原生库及旧工具不修改；新证据仅`artifacts/nosl-q4ag/`，完成后封存。完整门禁通过后合入main；失败保留证据，最终高水平目标继续保持未完成。
