# 原版奖励合法选项采集与执行覆盖

属于 E0/E3 采集资格和执行覆盖，基底 main 575e099。复用已有 seed-capacity 工作区，现有分支改名 wip/coverage-v3-legal-rewards，不增加树或分支数量；API开发由另一聊天继续。

2000旧深层captures完整复验在API合法性集成后有5条连续第二次REROLL。原版CardReward.OnSelect为headless选择器保留首次alternatives，而真实UI由Populate后Generate刷新并移除不可再重抽的按钮；O-017已登记。保留原采集及legacy-v1/coverage-v2策略，不改游戏规则或旧轨迹。

新增coverage-v3作为新采集默认：选择时绑定正在由原生同步器领取的CardReward实例，读取其CanReroll，只在真正可用的原始索引中采样，记录legal_reward_indices供输入审计。仅使用独立policy RNG；游戏所有RNG/执行调用不变。新增版本进入L4、批次与冻结身份协议；旧版本继续支持。验证5个原故障身份的新v3采集与完整快照后继重放，核实至少发生首轮重抽且下一次没有禁用选项；旧版本身份、旧输入拒绝/冻结合同继续保留。

追加卡牌执行清单：0c6c596下1993完整通过轨迹有526种实际CardPlay首证；将6种未执行的可打模型的20个bounded probes、KnowledgeDemon选择回调和DeprecatedCard牌组移除边界按真实资格记录。类型为Status不能直接当作不可打。未通过的原版注入DeprecatedCard+NewLeaf样本保持oracle_error，不伪称通过。

验收：采集/冻结协议测试；上述真实原版差分；最终完整taskset -c0-2 scripts/check.sh。报告记录源/CLI/Oracle/manifest指纹与EOF结果。全原生合法集合、全条件覆盖、TestMode=false整局和双端逐抽RNG仍待完成。
