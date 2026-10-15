# 通用 Env：移除活动 CombatSolver 桥接接口报告

日期：2026-09-28。分支：`feat/generic-env-rf-api`。

本批移除了活动 Env 的 `sts2_env_export_blank_template`、`sts2_env_export_monster_machines`、`Env::ExportBlankTemplateJson` 以及对应测试夹具。它们是旧 live mapper 的专用协议，不属于普通 run、通用快照或 Random Foreseer 公共操作；真实捕获桥按路线图延后到 Env/RF 合同稳定之后重新设计。

保留的能力包括完整逻辑快照、选择前缀恢复、owned 预测、查询、trace 和未移植模型的 fail-closed 检查。CombatSolver 子模块和 `archive/combatsolver` 未修改，也没有重新加入活动构建。

验收命令：

```bash
cmake --build build -j "$(nproc)"
ctest --test-dir build --output-on-failure
scripts/check.sh
```

结果：完整 `scripts/check.sh` 通过；16/16 CTest、L2 200 局、L3 80 场战斗、默认 L4 80 局及定向二幕/三幕路线均无分歧。共享库导出 27 个 `sts2_` 符号，旧 mapper/prediction/Beam 符号均不存在。
