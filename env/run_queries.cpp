#include "env/run_queries.h"

#include <algorithm>
#include <vector>
#include "engine/sim/alloc.h"
#include "engine/sim/state_hash.h"
#include "game/All.h"
#include "game/Entities/Merchant/MerchantInventory.h"
#include "game/Entities/RestSite/RestSiteOption.h"
#include "game/Events/EventOption.h"
#include "game/Models/Events/CrystalSphere.h"
#include "game/Runs/RunFlow.h"

namespace sts2 {
namespace {
const char* decision_name(DecisionKind kind) {
    constexpr const char* names[] = {"map", "combat", "rewards", "event", "rest_site", "shop", "treasure", "choice", "ended", "boundary"};
    return names[static_cast<size_t>(kind)];
}
void action(JsonWriter& w, const Action& value) {
    w.begin_object().field("kind", static_cast<int32_t>(value.kind)).field("index", value.index)
        .field("target", value.target).field("col", value.coord.col).field("row", value.coord.row).end_object();
}
void decimal(JsonWriter& w, Dec value) {
    w.begin_object().key("decimal_bits").begin_array();
    for (uint32_t word : {value.lo, value.mid, value.hi, value.flags}) w.value(static_cast<uint64_t>(word));
    w.end_array().end_object();
}
void model_id(JsonWriter& w, ModelKind kind, uint16_t id, std::string_view entry) {
    w.key("model_id").begin_object().field("kind", static_cast<int32_t>(kind))
        .field("entry", entry).field("dense_id", static_cast<int32_t>(id)).end_object()
        .field("static_usable", IsUsable(kind, id));
}
std::string_view loc_key(uint32_t hash) {
    const auto key = LocKey{hash}.str();
    if (hash && key.empty()) throw Unported("run queries: localization key not registered in loaded runtime");
    return key;
}
void vars(JsonWriter& w, AbstractModel model, const auto& definitions) {
    w.key("dynamic_vars").begin_array();
    for (size_t i = 0; i < definitions.size(); ++i) {
        DynamicVar var(model, static_cast<int8_t>(i));
        w.begin_object().field("name", definitions[i].name).key("base_value");
        decimal(w, var.BaseValue());
        w.end_object();
    }
    w.end_array();
}
void card(JsonWriter& w, Sim* sim, CardRef ref) {
    if (!ref || !sim->g->cards.live(ref)) throw GameError("run queries: invalid card reference");
    CardModel value(sim, ref);
    w.begin_object().field("card_ref", static_cast<int32_t>(ref));
    model_id(w, ModelKind::Card, value.DenseId(), name(value.CardId_()));
    w.field("upgrade", value.CurrentUpgradeLevel()).field("type", static_cast<int32_t>(value.Type()))
        .field("rarity", static_cast<int32_t>(value.Rarity())).field("pile", static_cast<int32_t>(value.st().pile))
        .field("floor_added", value.FloorAddedToDeck()).key("enchantment");
    if (const auto enchantment = value.Enchantment()) {
        w.begin_object().field("entry", name(enchantment.EnchantmentId_()))
            .field("amount", enchantment.Amount()).end_object();
    } else w.null();
    w.key("affliction");
    if (const auto affliction = value.Affliction()) {
        w.begin_object().field("entry", name(affliction.AfflictionId_()))
            .field("amount", affliction.Amount()).end_object();
    } else w.null();
    vars(w, value, value.data().vars);
    w.end_object();
}
void relic(JsonWriter& w, Sim* sim, RelicRef ref) {
    if (!ref || !sim->g->relics.live(ref)) throw GameError("run queries: invalid relic reference");
    RelicModel value(sim, ref);
    w.begin_object().field("relic_ref", static_cast<int32_t>(ref));
    model_id(w, ModelKind::Relic, value.DenseId(), name(value.RelicId_()));
    w.field("status", static_cast<int32_t>(value.Status())).field("floor_added", value.FloorAddedToDeck())
        .field("melted", value.IsMelted()).field("wax", value.IsWax());
    vars(w, value, value.data().vars); w.end_object();
}
void potion(JsonWriter& w, Sim* sim, PotionRef ref) {
    if (!ref || !sim->g->potions.live(ref)) throw GameError("run queries: invalid potion reference");
    PotionModel value(sim, ref);
    w.begin_object().field("potion_ref", static_cast<int32_t>(ref));
    model_id(w, ModelKind::Potion, value.DenseId(), name(value.PotionId_()));
    w.field("rarity", static_cast<int32_t>(value.Rarity()));
    vars(w, value, value.data().vars); w.end_object();
}
void map_query(JsonWriter& w, const RunData& run) {
    const ActMap& map = run.map;
    w.key("map").begin_object().field("cols", static_cast<int32_t>(map.cols))
        .field("rows", static_cast<int32_t>(map.rows)).key("current_coord");
    if (run.visited_map_coords.empty()) w.null();
    else w.begin_array().value(static_cast<int32_t>(run.visited_map_coords.back().col))
        .value(static_cast<int32_t>(run.visited_map_coords.back().row)).end_array();
    w.key("points").begin_array();
    for (int row = 0; row < map.rows; ++row) {
        for (int col = 0; col < map.cols; ++col) {
            int id = map.grid[col][row];
            if (id < 0) continue;
            const MapPoint& point = map.points[static_cast<size_t>(id)];
            w.begin_object().key("coord").begin_array()
                .value(static_cast<int32_t>(point.coord.col)).value(static_cast<int32_t>(point.coord.row)).end_array()
                .field("type", name(point.type)).field("fur_coat_quest", point.fur_coat_quest)
                .field("spoils_map_quest", point.spoils_map_quest)
                .key("children").begin_array();
            std::vector<MapCoord> children;
            point.children.for_each([&](uint8_t child) { children.push_back(map.points[child].coord); });
            std::sort(children.begin(), children.end(), [](MapCoord a, MapCoord b) {
                return a.row != b.row ? a.row < b.row : a.col < b.col;
            });
            for (MapCoord coord : children)
                w.begin_array().value(static_cast<int32_t>(coord.col)).value(static_cast<int32_t>(coord.row)).end_array();
            w.end_array().end_object();
        }
    }
    w.end_array().field("fur_coat_act_index", run.fur_coat_act_index)
        .field("current_map_affected_by_fur_coat", run.current_map_affected_by_fur_coat)
        .end_object();
}
void reward(JsonWriter& w, Sim* sim, RewardRef ref) {
    if (!ref || !sim->g->rewards.live(ref)) throw GameError("run queries: invalid reward reference");
    Reward value(sim, ref);
    const auto& state = value.st();
    w.begin_object().field("reward_ref", static_cast<int32_t>(ref))
        .field("type", static_cast<int32_t>(value.Type())).field("selected", value.SuccessfullySelected())
        .field("populated", value.IsPopulated()).field("gold", state.gold_amount)
        .field("can_skip", value.CanSkip()).field("can_reroll", value.CanReroll())
        .field("has_been_rerolled", state.has_been_rerolled).key("cards").begin_array();
    for (CardRef c : state.cards) card(w, sim, c);
    w.end_array().key("relic");
    if (state.relic) relic(w, sim, state.relic); else w.null();
    w.key("potion");
    if (state.potion) potion(w, sim, state.potion); else w.null();
    w.end_object();
}
void choice(JsonWriter& w, Env& env, const std::vector<Action>& actions) {
    const auto decision = env.decision();
    w.key("choice");
    if (decision.kind != DecisionKind::Choice) { w.null(); return; }
    const auto& request = decision.request;
    constexpr const char* kinds[] = {"card_select", "bundle", "option"};
    constexpr const char* option_kinds[] = {"card", "bundle", "reward", "alternative", "proceed"};
    if (request.options.size() != static_cast<size_t>(request.option_count))
        throw GameError("run queries: incomplete choice descriptions");
    w.begin_object().field("kind", kinds[static_cast<size_t>(request.kind)])
        .field("source", request.source).field("prompt", request.prompt)
        .field("option_count", request.option_count).field("min_select", request.min_select)
        .field("max_select", request.max_select).field("subset", request.subset)
        .field("can_skip", request.can_skip).field("answer_semantics",
            request.subset && request.prompt == "count" ? "count" : "option_index")
        .key("selected").begin_array();
    for (int32_t selected : request.selected) w.value(selected);
    w.end_array().key("options").begin_array();
    int32_t index = 0;
    for (const auto& option : request.options) {
        w.begin_object().field("index", index++).field("kind", option_kinds[static_cast<size_t>(option.kind)])
            .field("key", option.key).field("enabled", option.enabled).key("cards").begin_array();
        for (CardRef ref : option.cards) card(w, &env.sim(), ref);
        w.end_array().key("reward");
        if (option.reward) reward(w, &env.sim(), option.reward); else w.null();
        w.end_object();
    }
    w.end_array().key("legal_answers").begin_array();
    for (const auto& answer : actions) w.value(answer.index);
    w.end_array().end_object();
}
void shop(JsonWriter& w, Sim* sim) {
    const auto& state = sim->g->run.merchant;
    w.begin_object().field("card_removal_used", state.card_removal_used).key("entries").begin_array();
    const auto stocked = RunFlow::ShopOptions(sim);
    constexpr const char* kinds[] = {"character_card", "colorless_card", "relic", "potion", "card_removal"};
    int32_t entry_index = 0;
    for (const auto entry : Merchant::AllEntries(sim)) {
        const bool is_stocked = Merchant::IsStocked(sim, entry);
        const auto found = std::find(stocked.begin(), stocked.end(), entry);
        w.begin_object().field("entry_index", entry_index++).field("kind", kinds[entry.kind]).field("slot", static_cast<int32_t>(entry.index))
            .field("stocked", is_stocked).field("action_index", found == stocked.end() ? -1 : static_cast<int32_t>(found - stocked.begin()))
            .field("cost", Merchant::Cost(sim, entry)).field("buyable", is_stocked && Merchant::EnoughGold(sim, entry))
            .key("item");
        switch (entry.kind) {
        case Merchant::EntryRef::CharacterCard:
        case Merchant::EntryRef::ColorlessCard: {
            const auto& c = entry.kind == Merchant::EntryRef::CharacterCard ? state.character_cards[entry.index] : state.colorless_cards[entry.index];
            if (c.card) card(w, sim, c.card); else w.null();
            w.field("on_sale", c.on_sale); break;
        }
        case Merchant::EntryRef::Relic:
            if (state.relics[entry.index].relic) relic(w, sim, state.relics[entry.index].relic); else w.null(); break;
        case Merchant::EntryRef::Potion:
            if (state.potions[entry.index].potion) potion(w, sim, state.potions[entry.index].potion); else w.null(); break;
        case Merchant::EntryRef::CardRemoval: w.null(); break;
        }
        w.end_object();
    }
    w.end_array().end_object();
}

const char* crystal_item_kind(CrystalSphere::ItemKind kind) {
    switch (kind) {
    case CrystalSphere::Relic: return "relic";
    case CrystalSphere::Potion: return "potion";
    case CrystalSphere::Card: return "card";
    case CrystalSphere::Curse: return "curse";
    case CrystalSphere::Gold: return "gold";
    }
    throw GameError("run queries: unknown Crystal Sphere item kind");
}

void crystal_sphere(JsonWriter& w, Sim* sim, const EventModel& event) {
    if (event.EventId_() != EventId::CRYSTAL_SPHERE || event.st().crystal_sphere_cells.empty()) {
        w.null();
        return;
    }
    CrystalSphere sphere(sim, event.Index());
    w.begin_object().field("visibility", "privileged")
        .field("width", CrystalSphere::kWidth).field("height", CrystalSphere::kWidth)
        .field("active", sphere.active() != 0)
        .field("tool", sphere.tool())
        .field("divinations_remaining", sphere.divinations())
        .field("revealed_bits", sphere.revealed_bits())
        .key("grid").begin_array();
    for (uint8_t cell : event.st().crystal_sphere_cells) w.value(static_cast<int32_t>(cell));
    w.end_array().key("revealed_order").begin_array();
    for (uint8_t index : event.st().crystal_sphere_revealed_order) w.value(static_cast<int32_t>(index));
    w.end_array().key("items").begin_array();
    for (int index = 0; index < CrystalSphere::kItemCount; ++index) {
        const auto& item = CrystalSphere::kItems[static_cast<size_t>(index)];
        w.begin_object().field("index", index).field("kind", crystal_item_kind(item.kind))
            .field("width", item.width).field("height", item.height).field("value", item.value)
            .field("x", sphere.item_position_x(index)).field("y", sphere.item_position_y(index)).end_object();
    }
    w.end_array().end_object();
}
}  // namespace

void WriteRunQueries(JsonWriter& w, const Env& source, const RunQueryOptions& options) {
    const uint64_t source_hash = source.Hash();
    auto scratch = source.Clone();
    const uint64_t scratch_hash = scratch->Hash();
    Sim* sim = &scratch->sim();
    const auto& run = scratch->state().run;
    Player player(sim, 1);
    if (!run.player.creature || !scratch->state().creatures.live(run.player.creature))
        throw GameError("run queries: no player creature");
    const auto decision = scratch->decision();
    const auto actions = scratch->LegalActions();
    w.begin_object().field("format", "sts2env.run_queries").field("version", 1)
        .field("state_hash", source_hash).field("decision", static_cast<int32_t>(decision.kind))
        .field("decision_name", decision_name(decision.kind)).field("query_semantics", "game")
        .field("is_victory", run.is_victory).field("is_abandoned", run.is_abandoned)
        .field("is_agent_observation", false).field("refs_scope", "owned_state")
        .field("act_index", run.current_act_index).field("act_floor", run.act_floor).field("total_floor", run.total_floor)
        .key("player").begin_object().field("character", name(player.Character()))
        .field("creature_ref", static_cast<int32_t>(run.player.creature))
        .field("hp", player.Creature().CurrentHp()).field("max_hp", player.Creature().MaxHp())
        .field("block", player.Creature().Block()).field("gold", player.Gold()).key("energy");
    if (player.PlayerCombatState()) w.value(player.PlayerCombatState().Energy()); else w.null();
    w.key("stars");
    if (player.PlayerCombatState()) w.value(player.PlayerCombatState().Stars()); else w.null();
    w.key("boundary");
    if (decision.boundary) w.begin_object().field("kind", static_cast<int32_t>(decision.boundary->kind))
        .field("ordinal", decision.boundary->ordinal).end_object();
    else w.null();
    w.key("deck").begin_array();
    for (CardModel c : player.Deck().Cards()) card(w, sim, c.Index());
    w.end_array().key("relics").begin_array();
    for (RelicModel r : player.Relics()) relic(w, sim, r.Index());
    w.end_array().key("potions").begin_array();
    for (int32_t slot = 0; slot < player.MaxPotionCount(); ++slot) {
        const auto p = player.GetPotionAtSlotIndex(slot);
        w.begin_object().field("slot", slot).key("item");
        if (p) potion(w, sim, p.Index()); else w.null();
        w.end_object();
    }
    w.end_array().end_object();
    map_query(w, run);
    w.key("legal_actions").begin_array();
    for (const auto& a : actions) action(w, a);
    w.end_array();
    choice(w, *scratch, actions);
    w.key("rewards");
    // Successfully taking the final reward deactivates RewardsSet, but its
    // screen/OfferAndAwait loop still displays the entries until Proceed.
    const bool reward_screen = decision.kind == DecisionKind::Rewards ||
        (decision.kind == DecisionKind::Choice && decision.request.source == "rewards");
    if (!run.rewards.active && !reward_screen) w.null();
    else {
        w.begin_object().field("active", run.rewards.active).field("room_type", static_cast<int32_t>(run.rewards.room))
            .field("generated", run.rewards.generated).field("disallow_skipping", run.rewards.disallow_skipping)
            .key("entries").begin_array();
        int32_t index = 0;
        for (RewardRef ref : run.rewards.rewards) {
            w.begin_object().field("index", index++).key("reward"); reward(w, sim, ref); w.end_object();
        }
        w.end_array().end_object();
    }
    const RoomType current_room = run.rooms.empty() ? RoomType::Unassigned : run.rooms.back();
    w.field("room_type", static_cast<int32_t>(current_room)).key("shop");
    if (current_room == RoomType::Shop && run.merchant.exists) shop(w, sim); else w.null();
    w.key("event");
    if (current_room != RoomType::Event || run.room_event == EventId::NONE) w.null();
    else {
        const EventModel event(sim, 1);
        const auto& event_state = event.st();
        w.begin_object().field("entry", name(event_state.id)).field("finished", event_state.is_finished)
            .field("description_key", loc_key(event_state.description)).key("options").begin_array();
        int32_t index = 0;
        for (const auto& option : event_state.options) {
            w.begin_object().field("index", index++).field("text_key", loc_key(option.text_key))
                .field("locked", option.is_locked).field("proceed", option.is_proceed).field("chosen", option.was_chosen)
                .field("will_kill_player", WillKillPlayer(option, player)).key("relic");
            if (option.relic) relic(w, sim, option.relic); else w.null();
            w.end_object();
        }
        w.end_array().key("crystal_sphere"); crystal_sphere(w, sim, event);
        w.end_object();
    }
    w.key("rest_site");
    if (current_room != RoomType::RestSite) w.null();
    else {
        w.begin_object().field("completed", run.rest_site.completed).field("can_proceed", RunFlow::CanProceedFromRestSite(sim))
            .key("options").begin_array();
        int32_t index = 0;
        for (const auto& option : RunFlow::RestSiteOptions(sim))
            w.begin_object().field("index", index++).field("key", RestSite::OptionId(option.id))
                .field("enabled", option.enabled).end_object();
        w.end_array().end_object();
    }
    w.key("treasure");
    if (current_room != RoomType::Treasure || !run.treasure.picking) w.null();
    else {
        w.begin_object().key("relics").begin_array();
        int32_t index = 0;
        for (RelicId id : run.treasure.current_relics)
            w.begin_object().field("index", index++).field("entry", name(id)).end_object();
        w.end_array().end_object();
    }
    w.key("future");
    if (!options.include_future) w.null();
    else {
        w.begin_object().field("visibility", "privileged").key("next_act");
        const int32_t next = run.current_act_index + 1;
        if (next < 0 || next >= static_cast<int32_t>(run.acts.size())) w.null();
        else {
            const auto& act = run.acts[static_cast<size_t>(next)];
            w.begin_object().field("index", next).field("entry", name(act.id))
                .field("ancient", name(act.rooms.ancient)).field("boss", name(act.rooms.boss))
                .field("second_boss", name(act.rooms.second_boss)).end_object();
        }
        w.end_object();
    }
    w.end_object();
    if (scratch->Hash() != scratch_hash) throw Unported("run queries: getter changed gameplay state");
    if (source.Hash() != source_hash) throw GameError("run queries: source ownership violated");
}

std::string ExportRunQueries(const Env& source, const RunQueryOptions& options) {
    JsonWriter w;
    WriteRunQueries(w, source, options);
    return w.str();
}
}  // namespace sts2
