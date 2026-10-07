#include "env/game_queries.h"

#include <algorithm>

#include "engine/json.h"
#include "engine/sim/state_hash.h"
#include "game/All.h"
#include "game/Hooks/Hook.h"
#include "game/MonsterMoves/Intents/AttackIntent.h"

namespace sts2 {
namespace {

void require_model(AbstractModel model) {
    if (!IsUsable(model.Kind(), model.DenseId()))
        throw Unported("game queries: unported model kind=" + std::to_string(static_cast<int>(model.Kind()))
            + " id=" + std::to_string(model.DenseId()));
}

void require_query_coverage(Env& scratch) {
    // This also covers choices: their generated cards may not have reached a
    // completed Predict transition's normal conservative coverage check yet.
    if (!scratch.state().combat.is_in_progress)
        throw GameError("combat queries require active combat state");
    Sim* sim = &scratch.sim();
    CombatState combat(sim);
    if (!IsUsable(ModelKind::Encounter, static_cast<uint16_t>(combat.Encounter())))
        throw Unported("game queries: unported encounter");
    for (AbstractModel model : combat.IterateHookListeners()) require_model(model);
    for (CardModel card : Player(sim, 1).Deck().Cards()) require_model(card);
    for (CardPile pile : Player(sim, 1).PlayerCombatState().AllPiles())
        for (CardModel card : pile.Cards()) require_model(card);
}

void keywords(JsonWriter& writer, std::string_view key, CardKeywordSet set) {
    constexpr uint64_t known = (uint64_t{1} << (static_cast<unsigned>(CardKeyword::Eternal) + 1)) - 1;
    if (set.bits & ~known) throw Unported("game queries: unknown keyword bits");
    writer.key(key).begin_array();
    for (unsigned value = 0; value <= static_cast<unsigned>(CardKeyword::Eternal); ++value)
        if (set.Contains(static_cast<CardKeyword>(value))) writer.value(static_cast<int32_t>(value));
    writer.end_array();
}

void decimal(JsonWriter& writer, Dec value) {
    writer.begin_object().key("decimal_bits").begin_array();
    for (uint32_t word : {value.lo, value.mid, value.hi, value.flags}) writer.value(uint64_t(word));
    writer.end_array().end_object();
}

void target(JsonWriter& writer, CardModel card, Creature creature) {
    writer.begin_object().field("creature_ref", static_cast<int32_t>(creature.Index())).key("combat_id");
    if (creature) writer.value(creature.CombatId()); else writer.null();
    writer.field("is_valid_target", card.IsValidTarget(creature))
        .field("can_play_targeting", card.CanPlayTargeting(creature)).end_object();
}

void card_state_queries(JsonWriter& writer, CardModel card) {
    // Unfiltered game fields. Callers choose their own semantic fields,
    // fingerprint encoding and supported subclass/attachment projections.
    writer.key("card_state").begin_object()
        .field("upgrade", card.CurrentUpgradeLevel()).field("base_replay_count", card.BaseReplayCount())
        .field("exhaust_on_next_play", card.ExhaustOnNextPlay()).field("sly", card.IsSlyThisTurn())
        .field("retain", card.ShouldRetainThisTurn()).field("deck_version", bool(card.DeckVersion()))
        .field("removed", card.HasBeenRemovedFromState()).key("enchantment_id");
    if (card.Enchantment()) writer.value(name(card.Enchantment().EnchantmentId_())); else writer.null();
    writer.key("affliction_id");
    if (card.Affliction()) writer.value(name(card.Affliction().AfflictionId_())); else writer.null();
    writer.key("dynamic_vars").begin_array();
    const auto definitions = data(card.CardId_()).vars;
    for (size_t index = 0; index < definitions.size(); ++index) {
        const VarDef& definition = definitions[index];
        DynamicVar variable(card, static_cast<int8_t>(index));
        writer.begin_object().field("name", definition.name).field("is_calculated", variable.IsCalculated())
            .field("is_string", definition.type == VarType::StringVar).key("base_value");
        decimal(writer, variable.BaseValue());
        writer.end_object();
    }
    writer.end_array().key("subclass");
    if (card.CardId_() == CardId::RAMPAGE || card.CardId_() == CardId::THRASH) {
        writer.begin_object().key("growth_damage");
        decimal(writer, card.GrowthDamage());
        writer.end_object();
    } else writer.null();
    writer.end_object();
}

void card_queries(JsonWriter& writer, CardModel card, int32_t pile_index, int32_t index, CombatState combat) {
    CardEnergyCost energy = card.EnergyCost();
    UnplayableReason reason;
    AbstractModel preventer;
    const bool playable = card.CanPlay(reason, preventer);
    writer.begin_object().field("card_ref", static_cast<int32_t>(card.Index()))
        .field("pile_index", pile_index).field("index", index).field("hand_index", pile_index == 0 ? index : -1)
        .key("model_id").begin_object().field("category", "CARD").field("entry", name(card.CardId_()))
        .field("dense_id", static_cast<int32_t>(card.DenseId())).end_object()
        .field("card_type", static_cast<int32_t>(card.Type())).field("target_type", static_cast<int32_t>(card.TargetType()));
    keywords(writer, "local_keywords", card.GetKeywordsWithSources(KeywordSources::Local));
    keywords(writer, "effective_keywords", card.Keywords());
    card_state_queries(writer, card);
    writer.key("energy_cost").begin_object().field("raw", energy.GetWithModifiers(CostModifiers::None))
        .field("local", energy.GetWithModifiers(CostModifiers::Local))
        .field("effective", energy.GetWithModifiers(CostModifiers::All))
        .field("amount_to_spend", energy.GetAmountToSpend()).field("resolved", energy.GetResolved())
        .field("costs_x", energy.CostsX()).key("captured_x");
    if (energy.CostsX()) writer.value(energy.CapturedXValue()); else writer.null();
    writer.key("x_source");
    if (energy.CostsX()) writer.value("owner_energy"); else writer.null();
    writer.end_object();
    const int32_t effective_stars = card.GetStarCostWithModifiers();
    writer.key("star_cost").begin_object().field("base", card.BaseStarCost()).field("local", card.CurrentStarCost())
        .field("effective", effective_stars).field("amount_to_spend", std::max(0, effective_stars))
        .field("costs_x", card.HasStarCostX()).field("last_stars_spent", card.LastStarsSpent()).key("x_source");
    if (card.HasStarCostX()) writer.value("owner_stars"); else writer.null();
    writer.end_object().field("can_play", playable).field("unplayable_reason", static_cast<int32_t>(reason))
        .key("preventer");
    if (!preventer) writer.null();
    else writer.begin_object().field("kind", static_cast<int32_t>(preventer.Kind()))
        .field("dense_id", static_cast<int32_t>(preventer.DenseId())).field("ref", static_cast<int32_t>(preventer.Index())).end_object();
    writer.key("targets").begin_array();
    target(writer, card, {});
    for (Creature creature : combat.Creatures()) target(writer, card, creature);
    writer.end_array().end_object();
}

void roster_queries(JsonWriter& writer, CombatState combat) {
    writer.key("roster").begin_array();
    for (Creature creature : combat.Creatures()) {
        writer.begin_object().field("creature_ref", static_cast<int32_t>(creature.Index()))
            .field("combat_id", creature.CombatId()).field("side", static_cast<int32_t>(creature.Side()))
            .field("is_alive", creature.IsAlive()).key("monster");
        MonsterModel monster = creature.Monster();
        if (!monster) writer.null();
        else {
            require_model(monster);
            writer.begin_object().field("dense_id", static_cast<int32_t>(monster.DenseId()))
                .field("entry", name(monster.MonsterId_())).field("next_move_id", monster.NextMove().Id())
                .field("stunned", monster.NextMove().IsStunned()).field("intends_to_attack", monster.IntendsToAttack())
                .key("intents").begin_array();
            for (const Intent& intent : monster.NextMove().Intents()) {
                const bool attack = AttackIntent::IsAttack(intent);
                writer.begin_object().field("type", static_cast<int32_t>(intent.type)).field("is_attack", attack)
                    .field("multi", intent.multi).field("repeats", attack ? AttackIntent::Repeats(intent, monster) : 0)
                    .field("strong", intent.strong).field("status_count", intent.count).key("damage_raw");
                if (attack) decimal(writer, AttackIntent::DamageCalc(intent, monster)); else writer.null();
                writer.end_object();
            }
            writer.end_array().end_object();
        }
        writer.end_object();
    }
    writer.end_array();
}

void require_no_execution(const GameState& before, GameState& after) {
    // The only shipped getter write supported here is CardModel.BaseStarCost.
    // Comparing every logical field also proves no RNG/AI/history/action advance.
    before.cards.visit_snapshot([](auto, auto) {}, [&](CardRef ref, const CardState& original) {
        CardState& queried = after.cards[ref];
        if (!original.star_cost_set && queried.star_cost_set
            && queried.base_star_cost == data(queried.id).star_cost) {
            queried.star_cost_set = original.star_cost_set;
            queried.base_star_cost = original.base_star_cost;
        }
    });
    if (hash_state(before) != hash_state(after))
        throw Unported("game queries: getter changed gameplay state outside lazy star cost");
}

}  // namespace

std::string ExportCombatQueries(const Env& source) {
    const uint64_t source_hash = source.Hash();
    auto scratch = source.Clone();
    const GameState before = scratch->state();
    require_query_coverage(*scratch);
    Sim* sim = &scratch->sim();
    Player player(sim, 1);
    PlayerCombatState state = player.PlayerCombatState();
    CombatState combat(sim);
    JsonWriter writer;
    writer.begin_object().field("format", "sts2env.combat_queries").field("version", 4)
        .field("state_hash", source_hash).field("query_semantics", "game")
        .field("pending_choice", scratch->decision().kind == DecisionKind::Choice)
        .key("player").begin_object().field("creature_ref", static_cast<int32_t>(player.Creature().Index()))
        .field("energy", state.Energy()).field("stars", state.Stars()).field("max_energy", state.MaxEnergy())
        .field("max_hand_size", CardPile::MaxCardsInHand).field("max_hand_size_source", "CardPile.MaxCardsInHand")
        .field("turn", state.TurnNumber()).field("phase", static_cast<int32_t>(state.Phase()))
        .field("can_pay_excess_energy_with_stars", Hook::ShouldPayExcessEnergyCostWithStars(combat, player)).end_object()
        .key("piles").begin_array();
    int32_t pile_index = 0;
    for (CardPile pile : state.AllPiles()) {
        writer.begin_object().field("type", static_cast<int32_t>(pile.Type())).key("cards").begin_array();
        int32_t index = 0;
        for (CardModel card : pile.Cards()) card_queries(writer, card, pile_index, index++, combat);
        writer.end_array().end_object();
        ++pile_index;
    }
    writer.end_array();
    roster_queries(writer, combat);
    writer.end_object();
    require_no_execution(before, scratch->state());
    if (source.Hash() != source_hash) throw GameError("game queries: source ownership violated");
    return writer.str();
}

std::string ExportCombatPowers(const Env& source) {
    const uint64_t source_hash = source.Hash();
    auto scratch = source.Clone();
    const GameState before = scratch->state();
    require_query_coverage(*scratch);
    JsonWriter writer;
    writer.begin_object().field("format", "sts2env.combat_power_queries").field("version", 1)
        .field("state_hash", source_hash).field("query_semantics", "game").key("powers").begin_array();
    auto combat_id = [](Creature creature) -> uint64_t {
        return creature ? static_cast<uint32_t>(creature.CombatId()) : UINT32_MAX;
    };
    for (AbstractModel listener : CombatState(&scratch->sim()).IterateHookListeners()) {
        if (listener.Kind() != ModelKind::Power) continue;
        PowerModel power(&scratch->sim(), listener.Index());
        require_model(power);
        const PowerId id = power.PowerId_();
        writer.begin_object().field("power_ref", static_cast<int32_t>(power.Index())).field("power_id", name(id))
            .field("owner_combat_id", combat_id(power.Owner())).field("applier_combat_id", combat_id(power.Applier()))
            .field("target_combat_id", combat_id(power.Target())).field("amount", power.Amount())
            .field("amount_on_turn_start", power.AmountOnTurnStart())
            .field("skip_next_duration_tick", power.SkipNextDurationTick()).key("ritual_flag");
        if (id == PowerId::RITUAL_POWER) writer.value(power.st().extra[0] != 0); else writer.null();
        writer.key("surrounded_direction");
        if (id == PowerId::SURROUNDED_POWER) writer.value(power.st().extra[0]); else writer.null();
        writer.key("dynamic_vars").begin_array();
        const auto defs = data(id).vars;
        for (size_t index = 0; index < defs.size(); ++index) {
            DynamicVar var(power, static_cast<int8_t>(index));
            writer.begin_object().field("name", var.Name()).field("is_calculated", var.IsCalculated())
                .field("is_string", var.Type() == VarType::StringVar).key("base_value");
            // StringVar's numeric BaseValue is still a game field. String
            // presentation payload is not stored by this headless engine.
            decimal(writer, var.BaseValue());
            writer.end_object();
        }
        writer.end_array().end_object();
    }
    writer.end_array().end_object();
    if (hash_state(before) != hash_state(scratch->state())) throw Unported("power queries: query changed gameplay state");
    if (source.Hash() != source_hash) throw GameError("power queries: source ownership violated");
    return writer.str();
}

std::string ExportCombatOrbs(const Env& source) {
    const uint64_t source_hash = source.Hash();
    auto scratch = source.Clone();
    const GameState before = scratch->state();
    require_query_coverage(*scratch);
    const auto pcs = Player(&scratch->sim(), 1).PlayerCombatState();
    JsonWriter writer;
    writer.begin_object().field("format", "sts2env.combat_orb_queries").field("version", 2)
        .field("state_hash", source_hash).field("query_semantics", "game")
        .field("capacity", static_cast<int32_t>(pcs.st().orb_capacity)).key("orbs").begin_array();
    for (OrbRef ref : pcs.st().orbs) {
        OrbModel orb(&scratch->sim(), ref);
        require_model(orb);
        // Shipped getters retain Dark's accumulated value, ignore Focus for
        // Plasma, and double the modified passive value for Glass.
        writer.begin_object().field("orb_ref", static_cast<int32_t>(ref))
            .field("orb_id", name(orb.OrbId_())).key("passive_value");
        decimal(writer, orb.PassiveVal());
        writer.key("evoke_value");
        decimal(writer, orb.EvokeVal());
        writer.end_object();
    }
    writer.end_array().end_object();
    if (hash_state(before) != hash_state(scratch->state()))
        throw Unported("combat orb queries: getter changed gameplay state");
    if (source.Hash() != source_hash) throw GameError("combat orb queries: source ownership violated");
    return writer.str();
}

std::string ExportCombatCalculatedVars(const Env& source) {
    const uint64_t source_hash = source.Hash();
    auto scratch = source.Clone();
    const GameState before = scratch->state();
    require_query_coverage(*scratch);
    Player player(&scratch->sim(), 1);
    CombatState combat(&scratch->sim());
    JsonWriter writer;
    writer.begin_object().field("format", "sts2env.combat_calculated_vars").field("version", 2)
        .field("state_hash", source_hash).field("query_semantics", "game")
        .key("cards").begin_array();
    int32_t pile_index = 0;
    for (CardPile pile : player.PlayerCombatState().AllPiles()) {
        int32_t index = 0;
        for (CardModel card : pile.Cards()) {
            const auto defs = data(card.CardId_()).vars;
            for (size_t variable_index = 0; variable_index < defs.size(); ++variable_index) {
                DynamicVar variable(card, static_cast<int8_t>(variable_index));
                if (!variable.IsCalculated()) continue;
                writer.begin_object().field("card_ref", static_cast<int32_t>(card.Index()))
                    .field("card_id", name(card.CardId_())).field("pile_index", pile_index).field("index", index)
                    .field("variable", variable.Name()).field("type", static_cast<int32_t>(variable.Type()))
                    .key("base_value");
                decimal(writer, variable.BaseValue());
                writer.key("calculation_base");
                decimal(writer, card.DynamicVars().CalculationBase().BaseValue());
                writer.key("calculation_extra");
                decimal(writer, (variable.Type() == VarType::CalculatedDamageVar
                    ? card.DynamicVars().ExtraDamage() : card.DynamicVars().CalculationExtra()).BaseValue());
                writer.key("values").begin_array();
                auto write_value = [&](Creature target) {
                    writer.begin_object().field("target_ref", static_cast<int32_t>(target.Index())).key("game_value");
                    decimal(writer, variable.Calculate(target));
                    writer.key("raw_multiplier");
                    decimal(writer, variable.RawCalculationMultiplier(target));
                    writer.end_object();
                };
                write_value({});
                for (Creature target : combat.Creatures()) write_value(target);
                writer.end_array().end_object();
            }
            ++index;
        }
        ++pile_index;
    }
    writer.end_array().end_object();
    if (hash_state(before) != hash_state(scratch->state()))
        throw Unported("combat calculated vars: calculation changed gameplay state");
    if (source.Hash() != source_hash) throw GameError("combat calculated vars: source ownership violated");
    return writer.str();
}


}  // namespace sts2
