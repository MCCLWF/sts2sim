# Orobas / Archaic Tooth 附魔起始牌

日期：2026-09-28。分支：`feat/generic-env-rf-api`。游戏 v0.111.0。此批处理先古事件中可以取得的 Archaic Tooth，不更改其它角色并行工作或 CombatSolver。

## 实现

`CardCmd::CloneEnchantment` 将原牌的附魔类别、数量与 POD 保存状态复制到新牌。`Orobas.SetupTooth` 生成附魔预览时调用它；`ArchaicTooth.AfterObtained` 实际把起始牌变为超越牌时也调用它。去掉两处已附魔起始牌的 `Unported`。生成分发无需变化；地图源码清单中 Orobas 的未移植标记已消除。

## 验收

| 命令 | 结果 |
|---|---|
| `./build/test_orobas_enchanted` | 通过：升级且带 Sharp(2) 的 Bash 在事件预览与领取后成为升级 Break，附魔数值与 POD 附加状态保留 |
| `DOTNET=/home/wst/.dotnet/dotnet python3 tests/l4/orobas_enchanted.py` | 2/2 原版直接探针一致：预览和领取后的卡牌 ID、升级级别、附魔 ID/数量 |
| `python3 tools/coverage/map_source_inventory.py --check docs/audits/map-source-inventory.json` | 通过 |
| `scripts/check.sh` | 通过：22/22 CTest；L2 200 局；L3 80/80 战、1611/1611 状态；默认 L4 80/80 局、4089/4089 决策；全部定向完整路线零分歧、零 Unported |
| `git diff --check` | 通过，无输出 |

## 限制

直接原版探针覆盖 Sharp 附魔的 Ironclad Bash。其它职业起始牌及附魔组合、Orobas 整局路径尚未逐项原版对拍；两个探针不代表全部组合完成。附魔的展示字串和悬停提示不在此批的 Env 状态合同内。
