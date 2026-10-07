// Models/EnchantmentModel.cs. It lives inside the card that carries it, so
// the handle index is the card's.
#pragma once

#include <cstdint>

#include "game/Models/AbstractModel.h"
#include "game/Models/AfflictionModel.h"

namespace sts2 {

class EnchantmentModel : public AbstractModel {
public:
    EnchantmentModel() = default;
    EnchantmentModel(Sim* sim, CardRef card) : AbstractModel(sim, ModelKind::Enchantment, card) {}

    EnchantmentId EnchantmentId_() const { return sim_->g->cards[index_].enchantment; }
    CardModel Card() const;
    bool HasCard() const { return index_ != 0; }
    int32_t Amount() const { return sim_->g->cards[index_].enchantment_amount; }
    EnchantmentStatus Status() const { return sim_->g->cards[index_].enchantment_status; }

    // Models/EnchantmentModel.cs virtuals used by the damage and block
    // pipelines. Concrete behavior is dispatched by EnchantmentId in
    // EnchantmentModel.cpp; unsupported hooks still retain their default.
    Dec EnchantDamageAdditive(Dec originalDamage, ValueProp props) const;
    Dec EnchantDamageMultiplicative(Dec originalDamage, ValueProp props) const;
    Dec EnchantBlockAdditive(Dec originalBlock) const;
    Dec EnchantBlockMultiplicative(Dec originalBlock) const;
    bool ShouldStartAtBottomOfDrawPile() const;
    int32_t EnchantPlayCount(int32_t originalPlayCount) const;
    // EnchantmentModel.ModifyCard: reapply OnEnchant and value changes after
    // card values are reset. Card dynamic vars are evaluated from current
    // card state on demand in C++, so there is no cached set to refresh here.
    void ModifyCard() const;
    void OnPlay(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const;
    void RecalculateValues() const;
};


}  // namespace sts2
