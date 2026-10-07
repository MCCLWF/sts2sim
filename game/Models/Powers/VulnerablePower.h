// Models/Powers/VulnerablePower.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
#include "game/Models/Relics/PaperPhrog.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct VulnerablePower : PowerModel {
    SIM_MODEL(VulnerablePower, PowerModel)

    Dec ModifyDamageMultiplicative(Creature target, Dec amount, ValueProp props, Creature dealer, CardModel cardSource,
                                   const CardPlay* cardPlay) const {
        if (target != Owner()) return Dec(1);
        if (!IsPoweredAttack(props)) return Dec(1);
        Dec num = DynamicVars()["DamageIncrease"].BaseValue();
        if (dealer) {
            // PaperPhrog.ModifyVulnerableMultiplier / CrueltyPower.ModifyVulnerableMultiplier
            sts2::Player player = dealer.Player();
            if (player) {
                RelicModel relic = player.GetRelic(RelicId::PAPER_PHROG);
                if (relic) num = PaperPhrog(sim_, relic.Index()).ModifyVulnerableMultiplier(target, num, props, dealer, cardSource);
            }
            Creature petOwner = dealer.PetOwner() ? dealer.PetOwner().Creature() : Creature{};
            PowerModel cruelty = dealer.GetPower(PowerId::CRUELTY_POWER);
            if (cruelty) num = CrueltyPower(sim_, cruelty.Index()).ModifyVulnerableMultiplier(target, num, props, dealer, cardSource);
            if (petOwner) {
                PowerModel petCruelty = petOwner.GetPower(PowerId::CRUELTY_POWER);
                if (petCruelty) num = CrueltyPower(sim_, petCruelty.Index()).ModifyVulnerableMultiplier(target, num, props, dealer, cardSource);
            }
        }
        if (PowerModel debilitate = target.GetPower(PowerId::DEBILITATE_POWER))
            num = DebilitatePower(sim_, debilitate.Index()).ModifyVulnerableMultiplier(target, num, props, dealer, cardSource);
        return num;
    }

    void AfterSideTurnEnd(PlayerChoiceContext& choiceContext, CombatSide side, const CreatureList& participants) const {
        if (side == CombatSide::Enemy) PowerCmd::TickDownDuration(*this);
    }
};

}  // namespace sts2
