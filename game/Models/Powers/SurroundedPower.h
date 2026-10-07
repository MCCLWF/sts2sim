// Models/Powers/SurroundedPower.cs
#pragma once
#include "game/Entities/Cards/CardPlay.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct SurroundedPower : PowerModel {
    SIM_MODEL(SurroundedPower, PowerModel)
    // Direction.Right = 0, Direction.Left = 1.
    bool FacesLeft() const { return extra(0) != 0; }
    Dec ModifyDamageMultiplicative(Creature target, Dec, ValueProp, Creature dealer, CardModel, const CardPlay*) const {
        if (!dealer || target != Owner()) return Dec(1);
        return (FacesLeft() ? dealer.HasPower(PowerId::BACK_ATTACK_RIGHT_POWER) : dealer.HasPower(PowerId::BACK_ATTACK_LEFT_POWER))
            ? Dec(3) / Dec(2) : Dec(1);
    }
    void UpdateDirection(Creature target) const {
        if ((!FacesLeft() && target.HasPower(PowerId::BACK_ATTACK_LEFT_POWER)) ||
            (FacesLeft() && target.HasPower(PowerId::BACK_ATTACK_RIGHT_POWER))) extra(0) = !FacesLeft();
    }
    void BeforeCardPlayed(const CardPlay* play) const {
        if (play && play->Target && play->Card.Owner() == Owner().Player()) UpdateDirection(play->Target);
    }
    void BeforePotionUsed(PotionModel potion, Creature target) const {
        if (target && potion.Owner() == Owner().Player()) UpdateDirection(target);
    }
    void AfterDeath(PlayerChoiceContext&, Creature creature, bool wasRemovalPrevented, float) const {
        if (wasRemovalPrevented || creature.Side() == Owner().Side()) return;
        auto enemies = CombatState().HittableEnemies();
        if (enemies.empty()) return;
        bool allLeft = true, allRight = true;
        for (auto enemy : enemies) {
            allLeft &= enemy.HasPower(PowerId::BACK_ATTACK_LEFT_POWER);
            allRight &= enemy.HasPower(PowerId::BACK_ATTACK_RIGHT_POWER);
        }
        if (allLeft || allRight) UpdateDirection(enemies[0]);
    }
};
}  // namespace sts2
