// Models/EnchantmentModel.cs and the centralized scalar enchantment effects.
#include "game/Models/EnchantmentModel.h"

#include "game/All.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Localization/DynamicVars/DynamicVarSet.h"
#include "game/ValueProps/ValuePropExtensions.h"
#include "game/Models/Enchantments/Goopy.h"
#include "game/Models/Enchantments/Inky.h"
#include "game/Models/Enchantments/SoulsPower.h"

namespace sts2 {

CardModel EnchantmentModel::Card() const { return CardModel(sim_, index_); }

Dec EnchantmentModel::EnchantDamageAdditive(Dec, ValueProp props) const {
    if (EnchantmentId_() == EnchantmentId::VIGOROUS && Status() == EnchantmentStatus::Normal
        && IsPoweredAttack(props)) return Dec(Amount());
    if (EnchantmentId_() == EnchantmentId::SHARP && IsPoweredAttack(props)) return Dec(Amount());
    if (EnchantmentId_() == EnchantmentId::TEZCATARAS_EMBER && IsPoweredAttack(props)) return Dec(3);
    if (EnchantmentId_() == EnchantmentId::MOMENTUM && IsPoweredAttack(props))
        return Dec(sim_->g->cards[index_].enchantment_extra[0]);
    return Dec(0);
}
Dec EnchantmentModel::EnchantDamageMultiplicative(Dec, ValueProp props) const {
    // Corrupted.cs: powered attacks deal 1.5x damage; unpowered attacks and
    // non-attack damage are unchanged. IsPoweredAttack preserves the game's
    // Move/Unpowered check used by all card damage paths.
    if (EnchantmentId_() == EnchantmentId::CORRUPTED && IsPoweredAttack(props)) return Dec::lit(15, 1);
    if (EnchantmentId_() == EnchantmentId::INSTINCT && IsPoweredAttack(props)) return Dec(2);
    return Dec(1);
}
Dec EnchantmentModel::EnchantBlockAdditive(Dec) const {
    if (EnchantmentId_() == EnchantmentId::GOOPY) return Dec(Amount() - 1);
    return EnchantmentId_() == EnchantmentId::NIMBLE ? Dec(Amount()) : Dec(0);
}
Dec EnchantmentModel::EnchantBlockMultiplicative(Dec) const { return Dec(1); }
bool EnchantmentModel::ShouldStartAtBottomOfDrawPile() const {
    return EnchantmentId_() == EnchantmentId::IMBUED;
}
int32_t EnchantmentModel::EnchantPlayCount(int32_t original) const {
    if (EnchantmentId_() == EnchantmentId::GLAM)
        return sim_->g->cards[index_].enchantment_extra[0] ? original : original + 1;
    return EnchantmentId_() == EnchantmentId::SPIRAL ? original + 1 : original;
}
void EnchantmentModel::ModifyCard() const {
    if (!HasCard() || EnchantmentId_() == EnchantmentId::NONE)
        throw GameError("EnchantmentModel.ModifyCard requires an attached enchantment.");

    CardModel card = Card();
    // EnchantmentModel.OnEnchant overrides in v0.111.0. Keep these effects in
    // the same common path for first application and post-downgrade refresh.
    switch (EnchantmentId_()) {
    case EnchantmentId::GOOPY: Goopy::OnEnchant(card); break;
    case EnchantmentId::ROYALLY_APPROVED:
        card.AddKeyword(CardKeyword::Innate);
        card.AddKeyword(CardKeyword::Retain);
        break;
    case EnchantmentId::SOULS_POWER: SoulsPower::OnEnchant(card); break;
    case EnchantmentId::STEADY: card.AddKeyword(CardKeyword::Retain); break;
    case EnchantmentId::TEZCATARAS_EMBER: {
        const int32_t currentCost = card.EnergyCost().GetWithModifiers(CostModifiers::None);
        card.EnergyCost().UpgradeBy(-currentCost);
        card.AddKeyword(CardKeyword::Eternal);
        break;
    }
    default: break;
    }

    RecalculateValues();
}
void EnchantmentModel::OnPlay(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const {
    if (EnchantmentId_() == EnchantmentId::MOMENTUM)
        sim_->g->cards[index_].enchantment_extra[0] += Amount();
    // Inky.cs: after the enchanted card resolves, apply Weak to its target or
    // all currently hittable enemies when the card targets all enemies.
    if (EnchantmentId_() == EnchantmentId::INKY)
        Inky(sim_, index_).OnPlay(choiceContext, cardPlay);
    // Corrupted.cs: every play damages its owner for 2, bypassing block and
    // strength modifiers, while retaining the originating card/play context.
    if (EnchantmentId_() == EnchantmentId::CORRUPTED) {
        CreatureCmd::Damage(choiceContext, Card().Owner().Creature(), Dec(2),
                             ValueProp::Unblockable | ValueProp::Unpowered | ValueProp::Move,
                             Card(), cardPlay);
    }
    if (EnchantmentId_() == EnchantmentId::ADROIT)
        CreatureCmd::GainBlock(Card().Owner().Creature(), Dec(Amount()), ValueProp::Move, cardPlay);
    if (EnchantmentId_() == EnchantmentId::SWIFT && Status() == EnchantmentStatus::Normal) {
        sim_->g->cards[index_].enchantment_status = EnchantmentStatus::Disabled;
        CardPileCmd::Draw(choiceContext, Dec(Amount()), Card().Owner());
    }
    if (EnchantmentId_() == EnchantmentId::SOWN && Status() == EnchantmentStatus::Normal) {
        sim_->g->cards[index_].enchantment_status = EnchantmentStatus::Disabled;
        PlayerCmd::GainEnergy(Dec(Amount()), Card().Owner());
    }
}
void EnchantmentModel::RecalculateValues() const {
    // Adroit.cs sets DynamicVars.Block.BaseValue = Amount. Its single
    // enchantment var is stored in CardState::enchantment_var.
    if (EnchantmentId_() == EnchantmentId::ADROIT)
        DynamicVar(*this, 0).SetBaseValue(Dec(Amount()));
}

}  // namespace sts2
