# 共享药水批次 9

- 分支：`feat/m2-completion`。
- 范围：接入 ColorlessPotion，并复用 CardFactory 的可战斗生成过滤及去重选择逻辑。
- 真值：游戏 v0.111.0 `Models/Potions/ColorlessPotion.cs` 与 `Factories/CardFactory.cs`。
- 验收：药水专项测试覆盖展示选择、生成牌所属无色池、本回合免费；无色牌生成测试和完整 `scripts/check.sh` 通过。
- 边界：无色池仍有未移植卡牌，药水可能提供它们；选择该牌后打出仍受控 `Unported`。战士取得其他角色牌仍未实现。
