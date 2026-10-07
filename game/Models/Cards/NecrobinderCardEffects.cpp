#include "game/Models/Cards/NecrobinderCards.h"

#include <algorithm>
#include <cmath>
#include "core/csharp_int.h"

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/OstyCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/PowerCmd.h"
#include "engine/sim/alloc.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/Cards/NecrobinderCards.h"
#include "game/Models/Canonical.h"
#include "game/Models/Powers/DoomPower.h"

namespace sts2::NecrobinderCardEffects {
namespace {

bool snap_selectable(CardModel card) { return !card.Keywords().Contains(CardKeyword::Retain); }

Creature osty(Player player) {
    return player && player.PlayerCombatState() ? player.PlayerCombatState().GetPet(MonsterId::OSTY) : Creature{};
}

bool has_osty(Player player) {
    Creature pet = osty(player);
    return pet && pet.IsAlive();
}

Creature require_target(const CardPlay* play) {
    if (!play || !play->Target) throw GameError("cardPlay.Target is null");
    return play->Target;
}

Dec var(CardModel card, std::string_view key) { return card.DynamicVars()[key].BaseValue(); }
int32_t int_var(CardModel card, std::string_view key) { return card.DynamicVars()[key].IntValue(); }

CreatureList hittable(CardModel card) {
    return card.CombatState() ? card.CombatState().HittableEnemies() : CreatureList{};
}

void apply_power(PlayerChoiceContext& ctx, PowerId power, Creature target, Dec amount, CardModel card) {
    PowerCmd::Apply(power, ctx, target, amount, card.Owner().Creature(), card);
}

void apply_power(PlayerChoiceContext& ctx, PowerId power, const CreatureList& targets, Dec amount, CardModel card) {
    PowerCmd::Apply(power, ctx, targets, amount, card.Owner().Creature(), card);
}

CardList make_souls(Player player, int32_t count, CombatState combat, bool upgraded = false) {
    CardList cards;
    for (int32_t i = 0; i < count; ++i) {
        CardModel soul = combat.CreateCard(CardId::SOUL, player);
        if (upgraded) CardCmd::Upgrade(soul);
        cards.push_back(soul);
    }
    return cards;
}

void generate_souls(PlayerChoiceContext& ctx, Player player, CardModel source, int32_t count,
                    PileType pile, CardPilePosition position, bool upgraded = false) {
    CardList souls = make_souls(player, count, source.CombatState(), upgraded);
    if (!souls.empty()) CardPileCmd::AddGeneratedCardsToCombat(souls, pile, player, position);
}

void summon(PlayerChoiceContext& ctx, CardModel card, Dec amount) {
    OstyCmd::Summon(ctx, card.Owner(), amount, card);
}

void attack(CardModel card, PlayerChoiceContext& ctx, const CardPlay* play, Dec amount) {
    Creature target = require_target(play);
    DamageCmd::Attack(amount).FromCard(card, play).Targeting(target).Execute(ctx);
}

void attack(CardModel card, PlayerChoiceContext& ctx, const CardPlay* play, DynamicVar amount) {
    Creature target = require_target(play);
    DamageCmd::Attack(amount).FromCard(card, play).Targeting(target).Execute(ctx);
}

void osty_attack(CardModel card, PlayerChoiceContext& ctx, const CardPlay* play, Dec amount) {
    Creature target = require_target(play);
    Creature pet = osty(card.Owner());
    if (pet && pet.IsAlive()) DamageCmd::Attack(amount).FromOsty(pet, card, play).Targeting(target).Execute(ctx);
}

void apply_to_hittable(PlayerChoiceContext& ctx, CardModel card, PowerId power, Dec amount) {
    apply_power(ctx, power, hittable(card), amount, card);
}

bool has_local_keyword(CardModel card, CardKeyword keyword) {
    return card.GetKeywordsWithSources(KeywordSources::Local).Contains(keyword);
}

bool not_local_ethereal(CardModel card) {
    return !has_local_keyword(card, CardKeyword::Ethereal);
}

}  // namespace

bool CanBeGeneratedInCombat(CardModel card) {
    return card.CardId_() != CardId::EIDOLON && card.CardId_() != CardId::TRANSFIGURE;
}

bool GainsBlock(CardModel card) {
    switch (card.CardId_()) {
    case CardId::BONE_SHARDS: case CardId::DEATHS_DOOR: case CardId::DEFY: case CardId::DELAY:
    case CardId::GRAVE_WARDEN: case CardId::MELANCHOLY: case CardId::NEGATIVE_PULSE:
    case CardId::PULL_AGGRO: case CardId::SACRIFICE: case CardId::UNDEATH:
        return true;
    default: return false;
    }
}

bool HasEnergyCostX(CardModel card) {
    return card.CardId_() == CardId::DIRGE || card.CardId_() == CardId::ERADICATE;
}

bool IsPlayable(CardModel card) {
    if (card.CardId_() == CardId::HIGH_FIVE) return has_osty(card.Owner());
    return true;
}

void OnPlay(CardModel card, PlayerChoiceContext& ctx, const CardPlay* play) {
    Player owner = card.Owner();
    Creature player = owner.Creature();
    Creature target = play ? play->Target : Creature{};
    CombatState combat = card.CombatState();
    DynamicVarSet vars = card.DynamicVars();
    switch (card.CardId_()) {
    case CardId::BANSHEES_CRY:
        DamageCmd::Attack(vars.Damage().BaseValue()).FromCard(card, play).TargetingAllOpponents(combat).Execute(ctx);
        break;
    case CardId::BLIGHT_STRIKE: {
        target = require_target(play);
        AttackCommand command = DamageCmd::Attack(vars.Damage().BaseValue()).FromCard(card, play).Targeting(target);
        command.Execute(ctx);
        int32_t totalDamage = SumDamageResults(command.Results());
        apply_power(ctx, PowerId::DOOM_POWER, target, Dec(totalDamage), card);
        break;
    }
    case CardId::BONE_SHARDS: {
        Creature pet = osty(owner);
        if (pet && pet.IsAlive()) {
            DamageCmd::Attack(vars.OstyDamage().BaseValue()).FromOsty(pet, card, play).TargetingAllOpponents(combat).Execute(ctx);
            CreatureCmd::GainBlock(player, vars.Block(), play);
            if (pet.IsAlive()) CreatureCmd::Kill(pet);
        }
        break;
    }
    case CardId::BORROWED_TIME:
        PlayerCmd::GainEnergy(var(card, "Energy"), owner);
        apply_power(ctx, PowerId::BORROWED_TIME_POWER, player, var(card, "ExtraCost"), card);
        break;
    case CardId::BURY: attack(card, ctx, play, vars.Damage().BaseValue()); break;
    case CardId::CACOPHONY:
        apply_power(ctx, PowerId::CACOPHONY_POWER, player, vars.Damage().BaseValue(), card);
        break;
    case CardId::CALCIFY:
        apply_power(ctx, PowerId::CALCIFY_POWER, player, var(card, "CalcifyPower"), card);
        break;
    case CardId::CALL_OF_THE_VOID:
        apply_power(ctx, PowerId::CALL_OF_THE_VOID_POWER, player, var(card, "Cards"), card);
        break;
    case CardId::CAPTURE_SPIRIT: {
        target = require_target(play);
        CreatureCmd::Damage(ctx, target, vars.Damage().BaseValue(), ValueProp::Unblockable | ValueProp::Unpowered, card, play);
        generate_souls(ctx, owner, card, int_var(card, "Cards"), PileType::Draw, CardPilePosition::Random);
        break;
    }
    case CardId::CLEANSE: {
        summon(ctx, card, vars.Summon().BaseValue());
        CardList chosen = CardSelectCmd::FromCombatPile(ctx, owner.PlayerCombatState().DrawPile(), owner,
                                                         CardSelectorPrefs(1));
        if (!chosen.empty()) CardCmd::Exhaust(ctx, chosen[0]);
        break;
    }
    case CardId::COUNTDOWN:
        apply_power(ctx, PowerId::COUNTDOWN_POWER, player, var(card, "CountdownPower"), card);
        break;
    case CardId::DANSE_MACABRE:
        apply_power(ctx, PowerId::DANSE_MACABRE_POWER, player, var(card, "DanseMacabrePower"), card);
        break;
    case CardId::DEATH_MARCH:
        attack(card, ctx, play, vars.CalculatedDamage());
        break;
    case CardId::DEATHBRINGER:
        apply_to_hittable(ctx, card, PowerId::DOOM_POWER, var(card, "DoomPower"));
        apply_to_hittable(ctx, card, PowerId::WEAK_POWER, var(card, "WeakPower"));
        break;
    case CardId::DEATHS_DOOR: {
        int32_t repeats = 1 + (card.sim().g->combat.player_applied_doom_this_turn ? vars.Repeat().IntValue() : 0);
        for (int32_t i = 0; i < repeats; ++i) CreatureCmd::GainBlock(player, vars.Block(), play);
        break;
    }
    case CardId::DEFY:
        CreatureCmd::GainBlock(player, vars.Block(), play);
        apply_power(ctx, PowerId::WEAK_POWER, require_target(play), var(card, "WeakPower"), card);
        break;
    case CardId::DELAY:
        CreatureCmd::GainBlock(player, vars.Block(), play);
        apply_power(ctx, PowerId::ENERGY_NEXT_TURN_POWER, player, var(card, "Energy"), card);
        break;
    case CardId::DEMESNE:
        apply_power(ctx, PowerId::DEMESNE_POWER, player, var(card, "Cards"), card);
        break;
    case CardId::DEVOUR_LIFE:
        apply_power(ctx, PowerId::DEVOUR_LIFE_POWER, player, var(card, "DevourLifePower"), card);
        break;
    case CardId::DIRGE: {
        int32_t x = card.ResolveEnergyXValue();
        for (int32_t i = 0; i < x; ++i) summon(ctx, card, vars.Summon().BaseValue());
        generate_souls(ctx, owner, card, x, PileType::Draw, CardPilePosition::Random, card.CurrentUpgradeLevel() > 0);
        break;
    }
    case CardId::DREDGE: {
        CardPile discard = owner.PlayerCombatState().DiscardPile();
        int32_t freeSlots = std::max(0, 10 - static_cast<int32_t>(owner.PlayerCombatState().Hand().Cards().size()));
        int32_t count = std::min(int_var(card, "Cards"), std::min(freeSlots, static_cast<int32_t>(discard.Cards().size())));
        if (count > 0) {
            CardList selected = CardSelectCmd::FromCombatPile(ctx, discard, owner, CardSelectorPrefs(count));
            CardPileCmd::Add(selected, PileType::Hand);
        }
        break;
    }
    case CardId::EIDOLON: {
        CardList selected;
        for (CardModel c : owner.PlayerCombatState().ExhaustPile().Cards())
            if (c.Keywords().Contains(CardKeyword::Ethereal) && !c.Keywords().Contains(CardKeyword::Unplayable)) selected.push_back(c);
        for (CardModel c : selected) CardCmd::AutoPlay(ctx, c, Creature{});
        break;
    }
    case CardId::END_OF_DAYS: {
        for (Creature enemy : hittable(card)) apply_power(ctx, PowerId::DOOM_POWER, enemy, vars.Doom().BaseValue(), card);
        DoomPower::DoomKill(DoomPower::GetDoomedCreatures(combat.HittableEnemies()));
        break;
    }
    case CardId::ERADICATE: {
        target = require_target(play);
        DamageCmd::Attack(vars.Damage().BaseValue()).WithHitCount(card.ResolveEnergyXValue()).FromCard(card, play).Targeting(target).Execute(ctx);
        break;
    }
    case CardId::FETCH: {
        Creature pet = osty(owner);
        if (pet && pet.IsAlive()) {
            osty_attack(card, ctx, play, vars.OstyDamage().BaseValue());
            if (card.st().last_played_turn != owner.PlayerCombatState().TurnNumber())
                CardPileCmd::Draw(ctx, vars.Cards().BaseValue(), owner);
        }
        break;
    }
    case CardId::FLATTEN: osty_attack(card, ctx, play, vars.OstyDamage().BaseValue()); break;
    case CardId::FORBIDDEN_GRIMOIRE:
        apply_power(ctx, PowerId::FORBIDDEN_GRIMOIRE_POWER, player, Dec(1), card);
        break;
    case CardId::FRIENDSHIP:
        apply_power(ctx, PowerId::STRENGTH_POWER, player, -var(card, "StrengthPower"), card);
        apply_power(ctx, PowerId::FRIENDSHIP_POWER, player, var(card, "Energy"), card);
        break;
    case CardId::GLIMPSE_BEYOND:
        generate_souls(ctx, owner, card, int_var(card, "Cards"), PileType::Draw, CardPilePosition::Random);
        break;
    case CardId::GRAVE_WARDEN:
        CreatureCmd::GainBlock(player, vars.Block(), play);
        generate_souls(ctx, owner, card, int_var(card, "Cards"), PileType::Draw, CardPilePosition::Random);
        break;
    case CardId::GRAVEBLAST: {
        attack(card, ctx, play, vars.Damage().BaseValue());
        CardList selected = CardSelectCmd::FromCombatPile(ctx, owner.PlayerCombatState().DiscardPile(), owner,
                                                           CardSelectorPrefs(1));
        if (!selected.empty()) CardPileCmd::Add(selected[0], PileType::Hand);
        break;
    }
    case CardId::HANG: {
        target = require_target(play);
        attack(card, ctx, play, vars.Damage().BaseValue());
        int32_t amount = std::max(2, target.GetPowerAmount(PowerId::HANG_POWER));
        amount = std::min(amount, 999999999 - target.GetPowerAmount(PowerId::HANG_POWER));
        if (amount > 0) apply_power(ctx, PowerId::HANG_POWER, target, Dec(amount), card);
        break;
    }
    case CardId::HAUNT:
        apply_power(ctx, PowerId::HAUNT_POWER, player, vars.HpLoss().BaseValue(), card);
        break;
    case CardId::HIGH_FIVE:
        if (has_osty(owner)) {
            DamageCmd::Attack(vars.OstyDamage().BaseValue()).FromOsty(osty(owner), card, play).TargetingAllOpponents(combat).Execute(ctx);
            apply_to_hittable(ctx, card, PowerId::VULNERABLE_POWER, var(card, "VulnerablePower"));
        }
        break;
    case CardId::INVOKE:
        apply_power(ctx, PowerId::SUMMON_NEXT_TURN_POWER, player, var(card, "Summon"), card);
        apply_power(ctx, PowerId::ENERGY_NEXT_TURN_POWER, player, var(card, "Energy"), card);
        break;
    case CardId::LEGION_OF_BONE:
        summon(ctx, card, vars.Summon().BaseValue());
        break;
    case CardId::LETHALITY:
        apply_power(ctx, PowerId::LETHALITY_POWER, player, var(card, "LethalityPower"), card);
        break;
    case CardId::MELANCHOLY:
        CreatureCmd::GainBlock(player, vars.Block(), play);
        break;
    case CardId::MISERY: {
        target = require_target(play);
        struct DebuffSnapshot {
            PowerState state;
            PowerRef source;
            int32_t amount;
        };
        StaticVec<DebuffSnapshot, kMaxPowersPerCreature> debuffs;
        for (PowerModel power : target.Powers()) {
            if (power.TypeForCurrentAmount() == PowerType::Debuff)
                debuffs.push_back(DebuffSnapshot{power.st(), power.Index(), power.Amount()});
        }
        // Temporary debuffs carry a duration while their internally applied
        // stat power carries the real magnitude. Misery copies both and folds
        // the duration amount into the underlying debuff, as the source does.
        for (size_t i = 0; i < debuffs.size(); ++i) {
            PowerModel temporary(&card.sim(), debuffs[i].source);
            PowerId underlying = temporary.InternallyAppliedPowerId();
            if (underlying == PowerId::NONE) continue;
            for (DebuffSnapshot& base : debuffs) {
                if (base.state.id == underlying) {
                    base.amount += debuffs[i].amount;
                    break;
                }
            }
        }
        attack(card, ctx, play, vars.Damage().BaseValue());
        for (Creature enemy : hittable(card)) {
            if (enemy == target) continue;
            for (const DebuffSnapshot& snapshot : debuffs) {
                if (snapshot.amount == 0) continue;
                const PowerId id = snapshot.state.id;
                const Creature applier(&card.sim(), snapshot.state.applier);
                PowerModel existing = PowerCmd::FindExistingInstanceForStacking(id, enemy, applier);
                if (existing) {
                    PowerCmd::ModifyAmount(ctx, existing, Dec(snapshot.amount), applier, card);
                    continue;
                }
                PowerRef cloneRef = alloc_power(*card.sim().g, id);
                card.sim().g->powers[cloneRef] = snapshot.state;
                PowerState& cloneState = card.sim().g->powers[cloneRef];
                cloneState.owner = 0;
                cloneState.amount = 0;
                PowerModel clone(&card.sim(), cloneRef);
                PowerCmd::Apply(ctx, clone, enemy, Dec(snapshot.amount), applier, card);
            }
        }
        break;
    }
    case CardId::NECRO_MASTERY:
        summon(ctx, card, vars.Summon().BaseValue());
        apply_power(ctx, PowerId::NECRO_MASTERY_POWER, player, Dec(1), card);
        break;
    case CardId::NEGATIVE_PULSE:
        CreatureCmd::GainBlock(player, vars.Block(), play);
        apply_to_hittable(ctx, card, PowerId::DOOM_POWER, var(card, "DoomPower"));
        break;
    case CardId::NEUROSURGE:
        PlayerCmd::GainEnergy(var(card, "Energy"), owner);
        CardPileCmd::Draw(ctx, vars.Cards().BaseValue(), owner);
        apply_power(ctx, PowerId::NEUROSURGE_POWER, player, var(card, "NeurosurgePower"), card);
        break;
    case CardId::NO_ESCAPE: {
        target = require_target(play);
        apply_power(ctx, PowerId::DOOM_POWER, target, vars["CalculatedDoom"].Calculate(target), card);
        break;
    }
    case CardId::OBLIVION:
        apply_power(ctx, PowerId::OBLIVION_POWER, require_target(play), var(card, "DoomPower"), card);
        break;
    case CardId::PAGESTORM:
        apply_power(ctx, PowerId::PAGESTORM_POWER, player, var(card, "Cards"), card);
        break;
    case CardId::PARSE:
        CardPileCmd::Draw(ctx, vars.Cards().BaseValue(), owner);
        break;
    case CardId::POKE: osty_attack(card, ctx, play, vars.OstyDamage().BaseValue()); break;
    case CardId::PROTECTOR: {
        target = require_target(play);
        Creature pet = osty(owner);
        if (pet && pet.IsAlive()) {
            Dec damage = vars.CalculatedDamage().Calculate(target);
            DamageCmd::Attack(damage).FromOsty(pet, card, play).Targeting(target).Execute(ctx);
        }
        break;
    }
    case CardId::PULL_AGGRO:
        summon(ctx, card, vars.Summon().BaseValue());
        CreatureCmd::GainBlock(player, vars.Block(), play);
        break;
    case CardId::PULL_FROM_BELOW: {
        target = require_target(play);
        int32_t hits = card.sim().g->combat.player_ethereal_cards_played_this_combat;
        DamageCmd::Attack(vars.Damage().BaseValue()).WithHitCount(hits).FromCard(card, play).Targeting(target).Execute(ctx);
        break;
    }
    case CardId::PUTREFY:
        target = require_target(play);
        apply_power(ctx, PowerId::WEAK_POWER, target, var(card, "Power"), card);
        apply_power(ctx, PowerId::VULNERABLE_POWER, target, var(card, "Power"), card);
        break;
    case CardId::RATTLE: {
        target = require_target(play);
        Creature pet = osty(owner);
        if (pet && pet.IsAlive()) {
            int32_t hits = 1 + card.sim().g->creatures[pet.Index()].attacks_this_turn;
            DamageCmd::Attack(vars.OstyDamage().BaseValue()).WithHitCount(hits).FromOsty(pet, card, play).Targeting(target).Execute(ctx);
        }
        break;
    }
    case CardId::REANIMATE: summon(ctx, card, vars.Summon().BaseValue()); break;
    case CardId::REAP: attack(card, ctx, play, vars.Damage().BaseValue()); break;
    case CardId::REAPER_FORM:
        apply_power(ctx, PowerId::REAPER_FORM_POWER, player, Dec(1), card);
        break;
    case CardId::REAVE:
        attack(card, ctx, play, vars.Damage().BaseValue());
        generate_souls(ctx, owner, card, int_var(card, "Cards"), PileType::Draw, CardPilePosition::Random,
                       card.CurrentUpgradeLevel() > 0);
        break;
    case CardId::RIGHT_HAND_HAND: osty_attack(card, ctx, play, vars.OstyDamage().BaseValue()); break;
    case CardId::SACRIFICE: {
        Creature pet = osty(owner);
        if (pet && pet.IsAlive()) {
            Dec block = vars.CalculatedBlock().Calculate(player);
            CreatureCmd::Kill(pet);
            CreatureCmd::GainBlock(player, block, vars.CalculatedBlock().Props(), play);
        }
        break;
    }
    case CardId::SCOURGE:
        target = require_target(play);
        apply_power(ctx, PowerId::DOOM_POWER, target, vars.Doom().BaseValue(), card);
        CardPileCmd::Draw(ctx, vars.Cards().BaseValue(), owner);
        break;
    case CardId::SCULPTING_STRIKE: {
        attack(card, ctx, play, vars.Damage().BaseValue());
        CardList chosen = CardSelectCmd::FromHand(ctx, owner, CardSelectorPrefs(1), not_local_ethereal, card);
        if (!chosen.empty()) CardCmd::ApplyKeyword(chosen[0], {CardKeyword::Ethereal});
        break;
    }
    case CardId::SEANCE: {
        CardList chosen = CardSelectCmd::FromCombatPile(ctx, owner.PlayerCombatState().DrawPile(), owner,
                                                        CardSelectorPrefs(int_var(card, "Cards")));
        for (CardModel selected : chosen) {
            CardModel soul = combat.CreateCard(CardId::SOUL, owner);
            CardCmd::Transform(selected, soul);
        }
        break;
    }
    case CardId::SENTRY_MODE:
        apply_power(ctx, PowerId::SENTRY_MODE_POWER, player, var(card, "SentryModePower"), card);
        break;
    case CardId::SEVERANCE: {
        attack(card, ctx, play, vars.Damage().BaseValue());
        CardList souls = make_souls(owner, 3, combat);
        if (souls.size() == 3) {
            CardPileCmd::AddGeneratedCardToCombat(souls[0], PileType::Draw, owner, CardPilePosition::Random);
            CardPileCmd::AddGeneratedCardToCombat(souls[1], PileType::Discard, owner);
            CardPileCmd::AddGeneratedCardToCombat(souls[2], PileType::Hand, owner);
        }
        break;
    }
    case CardId::SHROUD:
        apply_power(ctx, PowerId::SHROUD_POWER, player, vars.Block().BaseValue(), card);
        break;
    case CardId::SIC_EM:
        osty_attack(card, ctx, play, vars.OstyDamage().BaseValue());
        apply_power(ctx, PowerId::SIC_EM_POWER, require_target(play), var(card, "SicEmPower"), card);
        break;
    case CardId::SLEIGHT_OF_FLESH:
        apply_power(ctx, PowerId::SLEIGHT_OF_FLESH_POWER, player, var(card, "SleightOfFleshPower"), card);
        break;
    case CardId::SNAP: {
        osty_attack(card, ctx, play, vars.OstyDamage().BaseValue());
        CardList chosen = CardSelectCmd::FromHand(ctx, owner, CardSelectorPrefs(1), snap_selectable, card);
        if (!chosen.empty() && !chosen[0].Keywords().Contains(CardKeyword::Retain))
            CardCmd::ApplyKeyword(chosen[0], {CardKeyword::Retain});
        break;
    }
    case CardId::SOUL_STORM: attack(card, ctx, play, vars.CalculatedDamage()); break;
    case CardId::SOULBOUND:
        if (target) apply_power(ctx, PowerId::SOULBOUND_POWER, target, Dec(1), card);
        break;
    case CardId::SOW:
        DamageCmd::Attack(vars.Damage().BaseValue()).FromCard(card, play).TargetingAllOpponents(combat).Execute(ctx);
        break;
    case CardId::SPIRIT_OF_ASH:
        apply_power(ctx, PowerId::SPIRIT_OF_ASH_POWER, player, var(card, "BlockOnExhaust"), card);
        break;
    case CardId::SPUR:
    {
        summon(ctx, card, vars.Summon().BaseValue());
        Creature pet = osty(owner);
        if (pet) CreatureCmd::Heal(pet, vars.Heal().BaseValue());
        break;
    }
    case CardId::SQUEEZE: {
        target = require_target(play);
        Creature pet = osty(owner);
        if (pet && pet.IsAlive()) DamageCmd::Attack(vars.CalculatedDamage()).FromOsty(pet, card, play).Targeting(target).Execute(ctx);
        break;
    }
    case CardId::THE_SCYTHE: {
        int32_t bonus = card.st().extra[0];
        vars.Damage().SetBaseValue(Dec(13 + bonus));
        attack(card, ctx, play, vars.Damage().BaseValue());
        int32_t increase = int_var(card, "Increase");
        card.st().extra[0] += increase;
        vars.Damage().SetBaseValue(Dec(13 + card.st().extra[0]));
        CardModel deck = card.DeckVersion();
        if (deck && deck != card) {
            deck.st().extra[0] += increase;
            deck.DynamicVars().Damage().SetBaseValue(Dec(13 + deck.st().extra[0]));
        }
        break;
    }
    case CardId::TIMES_UP: attack(card, ctx, play, vars.CalculatedDamage()); break;
    case CardId::TRANSFIGURE: {
        CardList chosen = CardSelectCmd::FromHand(ctx, owner, CardSelectorPrefs(1), nullptr, card);
        for (CardModel selected : chosen) {
            if (!selected.EnergyCost().CostsX() && selected.EnergyCost().GetWithModifiers(CostModifiers::None) >= 0)
                selected.EnergyCost().AddThisCombat(1);
            selected.SetBaseReplayCount(selected.BaseReplayCount() + 1);
        }
        break;
    }
    case CardId::UNDEATH: {
        CreatureCmd::GainBlock(player, vars.Block(), play);
        CardModel clone = card.CreateClone();
        CardPileCmd::AddGeneratedCardToCombat(clone, PileType::Discard, owner);
        break;
    }
    case CardId::UNDERWORLD:
        apply_power(ctx, PowerId::UNDERWORLD_POWER, player, Dec(1), card);
        break;
    case CardId::VEILPIERCER:
        attack(card, ctx, play, vars.Damage().BaseValue());
        apply_power(ctx, PowerId::VEILPIERCER_POWER, player, Dec(1), card);
        break;
    case CardId::WISP:
        PlayerCmd::GainEnergy(vars.Energy().IntValue(), owner);
        break;
    case CardId::SOUL:
        CardPileCmd::Draw(ctx, vars.Cards().BaseValue(), owner);
        break;
    case CardId::SWEEPING_GAZE: {
        Creature pet = osty(owner);
        if (pet && pet.IsAlive()) DamageCmd::Attack(vars.OstyDamage().BaseValue()).FromOsty(pet, card, play).TargetingRandomOpponents(combat).Execute(ctx);
        break;
    }
    default:
        throw GameError("Missing Necrobinder OnPlay implementation");
    }
}

void AfterCardEnteredCombat(CardModel card, CardModel entered) {
    if (!entered || entered != card) return;
    if (card.CardId_() == CardId::BANSHEES_CRY && !card.CloneOf()) {
        int32_t count = card.sim().g->combat.player_ethereal_cards_played_this_combat;
        card.EnergyCost().AddThisCombat(-count * card.DynamicVars()["Energy"].IntValue());
    } else if (card.CardId_() == CardId::FLATTEN) {
        Creature pet = osty(card.Owner());
        if (pet && card.sim().g->creatures[pet.Index()].attacks_this_turn > 0) card.EnergyCost().SetThisTurn(0);
    }
}

void AfterCardPlayed(CardModel card, PlayerChoiceContext&, const CardPlay* play) {
    if (!play) return;
    if (card.CardId_() == CardId::BANSHEES_CRY && play->Card.Owner() == card.Owner() &&
        play->Card.Keywords().Contains(CardKeyword::Ethereal)) {
        card.EnergyCost().AddThisCombat(-card.DynamicVars()["Energy"].IntValue());
    }
}

void AfterCardPlayedLate(CardModel card, PlayerChoiceContext&, const CardPlay* play) {
    if (card.CardId_() != CardId::RIGHT_HAND_HAND || !play || play->Card.Owner() != card.Owner() ||
        play->Resources.EnergyValue < card.DynamicVars()["Energy"].IntValue()) return;
    CardPile pile = card.Pile();
    if (pile && pile.Type() == PileType::Discard) CardPileCmd::Add(card, PileType::Hand);
}

void AfterAttack(CardModel card, PlayerChoiceContext&, AttackCommand& command) {
    if (card.CardId_() != CardId::FLATTEN) return;
    Creature pet = osty(card.Owner());
    if (pet && command.Attacker() == pet) card.EnergyCost().SetThisTurn(0);
}

void AfterDeath(CardModel card, PlayerChoiceContext&, Creature, bool removalPrevented, float) {
    if (card.CardId_() != CardId::MELANCHOLY || removalPrevented) return;
    CardPile pile = card.Pile();
    if (pile && pile.IsCombatPile()) card.EnergyCost().AddThisCombat(-card.DynamicVars()["Energy"].IntValue());
}

void AfterDowngraded(CardModel card) {
    if (card.CardId_() == CardId::THE_SCYTHE)
        card.DynamicVars().Damage().SetBaseValue(Dec(13 + card.st().extra[0]));
}

}  // namespace sts2::NecrobinderCardEffects
