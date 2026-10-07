// Models/Cards/SovereignBlade.cs
#pragma once

#include "game/Commands/Builders/AttackCommand.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct SovereignBlade : CardModel {
    SIM_MODEL(SovereignBlade, CardModel)

    sts2::TargetType TargetType() const {
        return Owner() && Owner().Creature().HasPower(PowerId::SEEKING_EDGE_POWER)
                   ? sts2::TargetType::AllEnemies
                   : sts2::TargetType::AnyEnemy;
    }

    bool GainsBlock() const {
        CardPile pile = Pile();
        return pile && pile.IsCombatPile() && Owner().Creature().GetPowerAmount(PowerId::PARRY_POWER) > 0;
    }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play) throw GameError("SovereignBlade: cardPlay is null");
        AttackCommand attack = DamageCmd::Attack(DynamicVars().Damage().BaseValue())
                                   .FromCard(*this, play)
                                   .WithHitCount(DynamicVars().Repeat().IntValue());
        if (TargetType() == sts2::TargetType::AllEnemies) {
            attack.TargetingAllOpponents(CombatState()).Execute(ctx);
        } else {
            if (!play->Target) throw GameError("SovereignBlade: cardPlay.Target is null");
            attack.Targeting(play->Target).Execute(ctx);
        }

        const int32_t parry = Owner().Creature().GetPowerAmount(PowerId::PARRY_POWER);
        if (parry > 0) CreatureCmd::GainBlock(Owner().Creature(), Dec(parry), ValueProp::Move, play);
    }

    // SovereignBlade.cs initializes _currentDamage = 10 and _currentRepeats = 1
    // in the field declarations. The slots hold the difference from those
    // initial values, so a blade that was never forged (all slots zero) reads
    // back exactly as the source object does.
    static constexpr int32_t kInitialDamage = 10;
    static constexpr int32_t kInitialRepeats = 1;

    // The source hook only creates/removes the smithing VFX node and plays a
    // sound when the card changes piles.
    void AfterCardChangedPiles(CardModel, PileType, AbstractModel) const {}
    void AfterTransformedFrom() const {}

    void AfterDowngraded() const {
        DynamicVars().Damage().SetBaseValue(LoadDamage());
        DynamicVars().Repeat().SetBaseValue(Dec(kInitialRepeats + extra(4)));
    }

    void AddDamage(Dec amount) const {
        DynamicVars().Damage().UpgradeValueBy(amount);
        SaveCurrentValues();
    }

    void SetRepeats(Dec amount) const {
        DynamicVars().Repeat().SetBaseValue(amount);
        SaveCurrentValues();
    }

private:
    Dec LoadDamage() const {
        return Dec(kInitialDamage) + Dec::from_bits(static_cast<uint32_t>(extra(0)), static_cast<uint32_t>(extra(1)),
                                                    static_cast<uint32_t>(extra(2)), static_cast<uint32_t>(extra(3)));
    }

    void SaveCurrentValues() const {
        const Dec damage = DynamicVars().Damage().BaseValue() - Dec(kInitialDamage);
        extra(0) = static_cast<int32_t>(damage.lo);
        extra(1) = static_cast<int32_t>(damage.mid);
        extra(2) = static_cast<int32_t>(damage.hi);
        extra(3) = static_cast<int32_t>(damage.flags);
        extra(4) = DynamicVars().Repeat().BaseValue().to_int() - kInitialRepeats;
    }
};

}  // namespace sts2
