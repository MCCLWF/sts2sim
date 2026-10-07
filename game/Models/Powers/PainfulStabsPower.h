// Models/Powers/PainfulStabsPower.cs
#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/Builders/AttackCommand.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"
namespace sts2 {
struct PainfulStabsPower : PowerModel {
    SIM_MODEL(PainfulStabsPower, PowerModel)
    void AfterAttack(PlayerChoiceContext&, AttackCommand& command) const {
        if (command.Attacker() != Owner() || command.TargetSide() == Owner().Side() || !IsPoweredAttack(command.DamageProps())) return;
        for (const auto& results : command.Results()) {
            for (const DamageResult& result : results) {
                if (result.Receiver.IsPlayer() && result.UnblockedDamage > 0)
                    CardPileCmd::AddToCombatAndPreview(CardId::WOUND, result.Receiver, PileType::Discard, Amount(), Player{});
            }
        }
    }
    bool ShouldPowerBeRemovedAfterOwnerDeath() const { return false; }
    bool ShouldCreatureBeRemovedFromCombatAfterDeath(Creature creature) const { return creature != Owner(); }
};
}  // namespace sts2
