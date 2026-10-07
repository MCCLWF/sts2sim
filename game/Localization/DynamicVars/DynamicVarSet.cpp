// Localization/DynamicVars/DynamicVar.cs and DynamicVarSet.cs: BaseValue
// storage per owner kind (see DynamicVarSet.h).
#include "game/Localization/DynamicVars/DynamicVarSet.h"

#include <string>

#include "game/All.h"
#include "game/Combat/CombatManager.h"
#include "core/csharp_int.h"

namespace sts2 {

namespace {
std::span<const VarDef> var_defs(const AbstractModel& m) {
    const GameState& g = *m.sim().g;
    switch (m.Kind()) {
    case ModelKind::Card: return data(g.cards[m.Index()].id).vars;
    case ModelKind::Power: return data(g.powers[m.Index()].id).vars;
    case ModelKind::Relic: return data(g.relics[m.Index()].id).vars;
    case ModelKind::Potion: return data(g.potions[m.Index()].id).vars;
    case ModelKind::Enchantment: return data(g.cards[m.Index()].enchantment).vars;
    case ModelKind::Event: return data(g.run.events[m.Index() - 1].id).vars;
    default: return {};
    }
}
}  // namespace

std::string_view DynamicVar::Name() const { return var_defs(owner_)[static_cast<size_t>(index_)].name; }
ValueProp DynamicVar::Props() const { return var_defs(owner_)[static_cast<size_t>(index_)].props; }
PowerId DynamicVar::Power() const { return var_defs(owner_)[static_cast<size_t>(index_)].power; }
VarType DynamicVar::Type() const { return var_defs(owner_)[static_cast<size_t>(index_)].type; }
bool DynamicVar::IsCalculated() const {
    return Type() == VarType::CalculatedVar || Type() == VarType::CalculatedDamageVar || Type() == VarType::CalculatedBlockVar;
}

Dec DynamicVar::Calculate(Creature target) const {
    if (!IsCalculated()) return BaseValue();
    if (owner_.Kind() != ModelKind::Card) throw Unported("CalculatedVar: non-card owner");
    CardModel card(&owner_.sim(), owner_.Index());
    const DynamicVarSet vars = card.DynamicVars();
    Dec multiplier(0);
    // CalculatedVar.Calculate calls its static formula only in an active
    // combat while this card is attached.
    if (CombatManager(&owner_.sim()).IsInProgress() && card.CombatState())
        multiplier = RawCalculationMultiplier(target);
    const DynamicVar extra = Type() == VarType::CalculatedDamageVar ? vars.ExtraDamage() : vars.CalculationExtra();
    return vars.CalculationBase().BaseValue() + extra.BaseValue() * multiplier;
}

Dec DynamicVar::RawCalculationMultiplier(Creature target) const {
    if (!IsCalculated() || owner_.Kind() != ModelKind::Card)
        throw GameError("CalculatedVar: raw formula requires a calculated card variable");
    CardModel card(&owner_.sim(), owner_.Index());
    const auto pcs = card.Owner().PlayerCombatState();
    Dec multiplier(0);
    switch (card.CardId_()) {
        case CardId::BODY_SLAM: case CardId::DEMONIC_SHIELD:
            multiplier = Dec(card.Owner().Creature().Block()); break;
        case CardId::PERFECTED_STRIKE: {
            int32_t count = 0;
            for (CardModel candidate : pcs.AllCards()) if (candidate.HasTag(CardTag::Strike)) ++count;
            multiplier = Dec(count); break;
        }
        case CardId::ASHEN_STRIKE: multiplier = Dec(pcs.ExhaustPile().Count()); break;
        case CardId::MIND_BLAST: multiplier = Dec(pcs.DrawPile().Count()); break;
        case CardId::BULLY: multiplier = Dec(target ? target.GetPowerAmount(PowerId::VULNERABLE_POWER) : 0); break;
        case CardId::REND: {
            int32_t count = 0;
            if (target) for (PowerModel power : target.Powers())
                if (power.TypeForCurrentAmount() == PowerType::Debuff && !power.IsTemporaryPower()) ++count;
            multiplier = Dec(count); break;
        }
        case CardId::GOLD_AXE: multiplier = Dec(owner_.sim().g->combat.cards_played_this_combat); break;
        case CardId::UNLEASH: {
            const Creature osty = pcs.GetPet(MonsterId::OSTY);
            multiplier = Dec(osty && osty.IsAlive() ? osty.CurrentHp() : 0); break;
        }
        case CardId::EXPECT_A_FIGHT:
            multiplier = Dec(std::max(0, card.Owner().Creature().GetPowerAmount(PowerId::STRENGTH_POWER))); break;
        case CardId::BARRAGE: multiplier = Dec(static_cast<int32_t>(pcs.st().orbs.size())); break;
        case CardId::FLECHETTES: {
            int32_t count = 0;
            for (CardModel candidate : pcs.Hand().Cards())
                if (candidate.Type() == CardType::Skill) ++count;
            multiplier = Dec(count); break;
        }
        case CardId::COMPILE_DRIVER: {
            StaticVec<OrbId, 10> distinct;
            for (OrbRef ref : pcs.st().orbs) {
                OrbId id = owner_.sim().g->orbs[ref].id;
                if (!distinct.contains(id)) distinct.push_back(id);
            }
            multiplier = Dec(static_cast<int32_t>(distinct.size())); break;
        }
        case CardId::SYNCHRONIZE: {
            StaticVec<OrbId, 10> distinct;
            for (OrbRef ref : pcs.st().orbs) {
                OrbId id = owner_.sim().g->orbs[ref].id;
                if (!distinct.contains(id)) distinct.push_back(id);
            }
            multiplier = Dec(static_cast<int32_t>(distinct.size())); break;
        }
        case CardId::VOLTAIC:
            multiplier = Dec(owner_.sim().g->combat.player_lightning_orbs_channeled_this_combat); break;
        case CardId::HELIX_DRILL: {
            int32_t amount = checked_int32(owner_.sim().g->combat.player_energy_spent_this_turn);
            CardPile pile = card.Pile();
            if (pile && pile.Type() == PileType::Play)
                amount -= card.EnergyCost().GetWithModifiers(CostModifiers::All);
            multiplier = Dec(amount);
            break;
        }
        case CardId::FINISHER:
            multiplier = Dec(owner_.sim().g->combat.player_attack_cards_this_turn); break;
        case CardId::MEMENTO_MORI:
            multiplier = Dec(owner_.sim().g->combat.player_cards_discarded_this_turn); break;
        case CardId::MURDER:
            multiplier = Dec(owner_.sim().g->combat.player_cards_drawn_this_combat); break;
        case CardId::MIRAGE: {
            int32_t amount = 0;
            CombatState combatState = card.CombatState();
            if (combatState) for (Creature enemy : combatState.Enemies())
                if (enemy.IsAlive()) amount = checked_int_add(amount, enemy.GetPowerAmount(PowerId::POISON_POWER));
            multiplier = Dec(amount);
            break;
        }
        case CardId::KNIFE_TRAP: {
            int32_t shivs = 0;
            for (CardModel candidate : pcs.ExhaustPile().Cards())
                if (candidate.HasTag(CardTag::Shiv)) ++shivs;
            multiplier = Dec(shivs);
            break;
        }
        case CardId::LUNAR_BLAST:
            multiplier = Dec(owner_.sim().g->combat.player_skill_cards_finished_this_turn); break;
        case CardId::RADIATE:
            multiplier = Dec(checked_int32(owner_.sim().g->combat.player_stars_gained_this_turn)); break;
        case CardId::SUPERMASSIVE:
            multiplier = Dec(owner_.sim().g->combat.player_cards_generated_this_combat); break;
        case CardId::BEAT_INTO_SHAPE: {
            const Creature dealer = card.Owner().Creature();
            multiplier = target && dealer
                             ? Dec(owner_.sim().g->creatures[target.Index()].PoweredDamageFrom(dealer.Index()))
                             : Dec(0);
            break;
        }
        case CardId::PRECISE_CUT: {
            int32_t handSize = pcs.Hand().Count();
            CardPile pile = card.Pile();
            if (pile && pile.Type() == PileType::Hand) --handSize;
            multiplier = Dec(-handSize);
            break;
        }
        case CardId::FLAK_CANNON: {
            int32_t statuses = 0;
            for (CardModel candidate : pcs.AllCards())
                if (candidate.Type() == CardType::Status && candidate.Pile().Type() != PileType::Exhaust) ++statuses;
            multiplier = Dec(statuses);
            break;
        }
        case CardId::CRESCENT_SPEAR: {
            int32_t starCards = 0;
            for (CardModel candidate : pcs.AllCards())
                if (candidate.CanonicalStarCost() >= 0 || candidate.HasStarCostX()) ++starCards;
            multiplier = Dec(starCards);
            break;
        }
        case CardId::TEAR_ASUNDER:
            multiplier = Dec(1 + owner_.sim().g->combat.player_damage_received_entries_this_combat); break;
        case CardId::DEATH_MARCH:
            multiplier = Dec(owner_.sim().g->combat.player_cards_drawn_this_turn); break;
        case CardId::PULL_FROM_BELOW:
            multiplier = Dec(owner_.sim().g->combat.player_ethereal_cards_played_this_combat); break;
        case CardId::RATTLE: {
            const Creature osty = pcs.GetPet(MonsterId::OSTY);
            multiplier = Dec(1 + (osty ? owner_.sim().g->creatures[osty.Index()].attacks_this_turn : 0));
            break;
        }
        case CardId::PROTECTOR: {
            const Creature osty = pcs.GetPet(MonsterId::OSTY);
            multiplier = Dec(osty && osty.IsAlive() ? osty.MaxHp() : 0);
            break;
        }
        case CardId::SACRIFICE: {
            const Creature osty = pcs.GetPet(MonsterId::OSTY);
            multiplier = Dec(osty && osty.IsAlive() ? osty.MaxHp() * 3 : 0);
            break;
        }
        case CardId::SOUL_STORM: {
            int32_t count = 0;
            for (CardModel candidate : pcs.ExhaustPile().Cards())
                if (candidate.CardId_() == CardId::SOUL) ++count;
            multiplier = Dec(count);
            break;
        }
        case CardId::SQUEEZE: {
            int32_t count = 0;
            for (CardModel candidate : pcs.AllCards())
                if (candidate.HasTag(CardTag::OstyAttack) && candidate != card) ++count;
            multiplier = Dec(count);
            break;
        }
        case CardId::TIMES_UP:
            multiplier = Dec(target ? target.GetPowerAmount(PowerId::DOOM_POWER) : 0); break;
        case CardId::NO_ESCAPE: {
            const int32_t threshold = std::max(1, card.DynamicVars()["DoomThreshold"].IntValue());
            multiplier = Dec(target ? target.GetPowerAmount(PowerId::DOOM_POWER) / threshold : 0);
            break;
        }
        default: throw Unported("CalculatedVar: formula not ported for " + std::string(name(card.CardId_())));
    }
    return multiplier;
}

Dec DynamicVar::BaseValue() const {
    GameState& g = *owner_.sim().g;
    size_t i = static_cast<size_t>(index_);
    switch (owner_.Kind()) {
    case ModelKind::Card: {
        const CardState& c = g.cards[owner_.Index()];
        for (const VarOverride& o : c.var_overrides) {
            if (o.index == i) return o.value;
        }
        const CardData& d = data(c.id);
        size_t level = std::min<size_t>(c.upgrade, d.levels.size() - 1);
        return d.levels[level].vars[i];
    }
    case ModelKind::Power: return g.powers[owner_.Index()].vars[i];
    case ModelKind::Relic: return g.relics[owner_.Index()].vars[i];
    case ModelKind::Potion: return data(g.potions[owner_.Index()].id).vars[i].base;
    case ModelKind::Enchantment: return g.cards[owner_.Index()].enchantment_var;
    case ModelKind::Event: return g.run.events[owner_.Index() - 1].vars[i];
    default: throw GameError("DynamicVar: model kind without vars");
    }
}

void DynamicVar::SetBaseValue(Dec v) const {
    GameState& g = *owner_.sim().g;
    size_t i = static_cast<size_t>(index_);
    switch (owner_.Kind()) {
    case ModelKind::Card: {
        // An override records a value that differs from the table value of
        // the card's current upgrade level; a value equal to it is stored as
        // no override, so equal cards have one representation. Every later
        // read and upgrade sees the same value either way: the table level
        // L+1 is the source's upgrade applied to level L.
        CardState& c = g.cards[owner_.Index()];
        const CardData& d = data(c.id);
        const Dec& base = d.levels[std::min<size_t>(c.upgrade, d.levels.size() - 1)].vars[i];
        // Same bits, not merely the same number: the scale of a decimal is
        // observable (10.0 and 10 format and compute differently).
        const bool table = v.lo == base.lo && v.mid == base.mid && v.hi == base.hi && v.flags == base.flags;
        for (size_t k = 0; k < c.var_overrides.size(); ++k) {
            VarOverride& o = c.var_overrides[k];
            if (o.index != i) continue;
            if (table) c.var_overrides.erase_at(k);
            else o.value = v;
            return;
        }
        if (!table) c.var_overrides.push_back(VarOverride{v, static_cast<uint8_t>(i)});
        return;
    }
    case ModelKind::Power: g.powers[owner_.Index()].vars[i] = v; return;
    case ModelKind::Relic: g.relics[owner_.Index()].vars[i] = v; return;
    case ModelKind::Enchantment: g.cards[owner_.Index()].enchantment_var = v; return;
    case ModelKind::Event: g.run.events[owner_.Index() - 1].vars[i] = v; return;
    default: throw GameError("DynamicVar: this model kind's vars are read-only");
    }
}

int32_t DynamicVarSet::Count() const { return static_cast<int32_t>(var_defs(owner_).size()); }

bool DynamicVarSet::ContainsKey(std::string_view key) const {
    for (const VarDef& d : var_defs(owner_)) {
        if (d.name == key) return true;
    }
    return false;
}

DynamicVar DynamicVarSet::operator[](std::string_view key) const {
    auto defs = var_defs(owner_);
    for (size_t i = 0; i < defs.size(); ++i) {
        if (defs[i].name == key) return DynamicVar(owner_, static_cast<int8_t>(i));
    }
    throw GameError("DynamicVarSet has no var " + std::string(key));
}

}  // namespace sts2
