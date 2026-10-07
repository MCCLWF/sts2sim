// JSON dumps of the simulator state in the oracle's formats
// (tools/Sts2Oracle/OracleCombat.cs Dump, OracleRun.cs DumpRun).
#include "tools/cli/dump.h"

#include "game/All.h"
#include "game/Rewards/Reward.h"
#include "game/MonsterMoves/Intents/AttackIntent.h"

namespace sts2 {

namespace {
void write_rng_counters_(JsonWriter& w, const RunData& run) {
    static constexpr const char* kNames[] = {"UpFront",           "Shuffle",     "UnknownMapPoint",     "CombatCardGeneration",
                                             "CombatPotionGeneration", "CombatCardSelection", "CombatEnergyCosts",
                                             "CombatTargets",     "MonsterAi",   "Niche",               "CombatOrbs",
                                             "TreasureRoomRelics"};
    w.begin_object();
    for (size_t i = 0; i < static_cast<size_t>(RunRngType::Count); ++i) {
        w.field(kNames[i], run.rng.rngs[i].counter);
    }
    w.end_object();
}

void write_powers(JsonWriter& w, Creature c) {
    w.begin_array();
    for (PowerModel p : c.Powers()) {
        w.begin_array();
        w.value(name(p.PowerId_()));
        w.value(p.Amount());
        w.end_array();
    }
    w.end_array();
}

void write_pile(JsonWriter& w, CardPile pile) {
    w.begin_array();
    for (CardModel c : pile.Cards()) {
        std::string s(c.data().entry);
        if (c.CurrentUpgradeLevel() > 0) s += "+" + std::to_string(c.CurrentUpgradeLevel());
        w.value(s);
    }
    w.end_array();
}

void write_state(JsonWriter& w, Sim* sim) {
    CombatState combat(sim);
    Player player(sim, 1);
    PlayerCombatState pcs = player.PlayerCombatState();
    w.begin_object();
    w.field("round", combat.RoundNumber());
    w.field("turn", pcs.TurnNumber());
    w.field("test_subject_kills", sim->g->run.test_subject_kills);
    w.key("player").begin_object();
    w.field("hp", player.Creature().CurrentHp());
    w.field("max_hp", player.Creature().MaxHp());
    w.field("block", player.Creature().Block());
    w.field("energy", pcs.Energy());
    w.field("stars", pcs.Stars());
    w.key("powers");
    write_powers(w, player.Creature());
    w.key("hand");
    write_pile(w, pcs.Hand());
    w.key("hand_costs").begin_array();
    for (CardModel c : pcs.Hand().Cards()) w.value(c.EnergyCost().GetWithModifiers(CostModifiers::All));
    w.end_array();
    w.key("hand_afflictions").begin_array();
    for (CardModel c : pcs.Hand().Cards()) {
        AfflictionModel affliction = c.Affliction();
        if (affliction) w.value(name(affliction.AfflictionId_()));
        else w.null();
    }
    w.end_array();
    w.key("draw");
    write_pile(w, pcs.DrawPile());
    w.key("discard");
    write_pile(w, pcs.DiscardPile());
    w.key("exhaust");
    write_pile(w, pcs.ExhaustPile());
    w.key("play");
    write_pile(w, pcs.PlayPile());
    w.key("relics").begin_array();
    for (RelicRef r : sim->g->run.player.relics) {
        RelicModel relic(sim, r);
        w.begin_object();
        w.field("id", relic.data().entry);
        if (relic.ShowCounter()) w.field("counter", relic.DisplayAmount());
        w.end_object();
    }
    w.end_array();
    w.key("potion_slots").begin_array();
    for (int i = 0; i < sim->g->run.player.max_potion_slots; ++i) {
        PotionRef p = sim->g->run.player.potions[i];
        if (p == 0) {
            w.null();
        } else {
            w.value(data(sim->g->potions[p].id).entry);
        }
    }
    w.end_array();
    w.field("orb_capacity", pcs.st().orb_capacity);
    w.key("orbs").begin_array();
    for (OrbRef ref : pcs.st().orbs) w.value(data(sim->g->orbs[ref].id).entry);
    w.end_array();
    w.key("orb_values").begin_array();
    for (OrbRef ref : pcs.st().orbs) w.value(OrbModel(sim, ref).EvokeVal().to_int());
    w.end_array();
    w.key("pets").begin_array();
    for (Creature pet : pcs.Pets()) {
        w.begin_object();
        w.field("id", pet.Monster().data().entry);
        w.field("combat_id", pet.CombatId());
        w.field("hp", pet.CurrentHp());
        w.field("max_hp", pet.MaxHp());
        w.field("block", pet.Block());
        w.key("powers");
        write_powers(w, pet);
        std::string_view next = pet.Monster().NextMove().Id();
        w.field("next_move", next);
        w.end_object();
    }
    w.end_array();
    w.end_object();
    w.key("enemies").begin_array();
    CreatureList targets = combat.PlayerCreatures();
    for (Creature e : combat.Enemies()) {
        MonsterModel m = e.Monster();
        w.begin_object();
        w.field("id", m.data().entry);
        w.field("combat_id", e.CombatId());
        w.field("hp", e.CurrentHp());
        w.field("max_hp", e.MaxHp());
        w.field("block", e.Block());
        w.key("powers");
        write_powers(w, e);
        std::string_view next = m.NextMove().Id();
        if (next == "UNSET_MOVE") {
            w.key("next_move").null();
        } else {
            w.field("next_move", next);
        }
        w.key("intents").begin_array();
        for (const Intent& intent : m.NextMove().Intents()) {
            w.begin_object();
            w.field("type", name(intent.type));
            if (AttackIntent::IsAttack(intent)) {
                w.field("damage", AttackIntent::GetSingleDamage(intent, targets, e));
                w.field("repeats", AttackIntent::Repeats(intent, m));
            }
            w.end_object();
        }
        w.end_array();
        w.field("rng_counter", m.Rng().counter);
        w.end_object();
    }
    w.end_array();
    w.key("rng");
    write_rng_counters(w, sim->g->run);
    w.end_object();
}


}  // namespace

void write_rng_counters(JsonWriter& w, const RunData& run) { write_rng_counters_(w, run); }
void write_combat_state(JsonWriter& w, Sim* sim) { write_state(w, sim); }
void write_run(JsonWriter& w, Sim* sim) {
    const RunData& run = sim->g->run;
    Player player(sim, 1);
    w.begin_object();
    w.field("act", run.current_act_index);
    w.field("total_floor", static_cast<int32_t>(run.total_floor));
    w.field("act_floor", static_cast<int32_t>(run.act_floor));
    if (run.visited_map_coords.empty()) {
        w.key("coord").null();
    } else {
        w.key("coord").begin_array();
        w.value(static_cast<int32_t>(run.visited_map_coords.back().col));
        w.value(static_cast<int32_t>(run.visited_map_coords.back().row));
        w.end_array();
    }
    if (run.rooms.empty()) {
        w.key("room").null();
    } else {
        w.field("room", name(run.rooms.back()));
    }
    w.field("hp", player.Creature().CurrentHp());
    w.field("max_hp", player.Creature().MaxHp());
    w.field("gold", player.Gold());
    w.key("deck").begin_array();
    for (CardRef c : run.player.deck) write_card_name(w, sim, c);
    w.end_array();
    w.key("relics").begin_array();
    for (RelicRef r : run.player.relics) {
        RelicModel relic(sim, r);
        w.begin_object();
        w.field("id", relic.data().entry);
        if (relic.ShowCounter()) w.field("counter", relic.DisplayAmount());
        w.end_object();
    }
    w.end_array();
    w.key("potions").begin_array();
    for (int i = 0; i < run.player.max_potion_slots; ++i) {
        PotionRef p = run.player.potions[i];
        if (p == 0) {
            w.null();
        } else {
            w.value(data(sim->g->potions[p].id).entry);
        }
    }
    w.end_array();
    w.key("rng");
    write_rng_counters(w, run);
    w.key("player_rng").begin_object();
    static constexpr const char* kPlayerRngNames[] = {"Rewards", "Shops", "Transformations"};
    for (size_t i = 0; i < static_cast<size_t>(PlayerRngType::Count); ++i) w.field(kPlayerRngNames[i], run.player.rng.rngs[i].counter);
    w.end_object();
    w.key("odds").begin_object();
    w.field("card_rarity", run.player.odds.card_rarity);
    w.field("potion_reward", run.player.odds.potion_reward);
    w.key("unknown").begin_object();
    w.field("Monster", run.odds.monster);
    w.field("Elite", run.odds.elite);
    w.field("Treasure", run.odds.treasure);
    w.field("Shop", run.odds.shop);
    w.end_object();
    w.end_object();
    w.end_object();
}

void write_card_name(JsonWriter& w, Sim* sim, uint32_t card) {
    CardModel c(sim, card);
    std::string s(c.data().entry);
    if (c.CurrentUpgradeLevel() > 0) s += "+" + std::to_string(c.CurrentUpgradeLevel());
    w.value(s);
}

void write_reward(JsonWriter& w, Sim* sim, Reward r) {
    w.begin_object();
    switch (r.Type()) {
    case RewardType::Gold:
        w.field("type", "Gold");
        w.field("amount", r.Amount());
        break;
    case RewardType::Potion:
        w.field("type", "Potion");
        w.field("id", r.Potion() ? r.Potion().data().entry : std::string_view{});
        break;
    case RewardType::Card:
        w.field("type", "Card");
        w.field("can_skip", r.CanSkip()).field("can_reroll", r.CanReroll())
            .field("has_been_rerolled", r.st().has_been_rerolled);
        w.key("cards").begin_array();
        for (CardRef c : r.st().cards) write_card_name(w, sim, c);
        w.end_array();
        break;
    case RewardType::Relic:
        w.field("type", "Relic");
        w.field("id", r.Relic() ? r.Relic().data().entry : std::string_view{});
        break;
    case RewardType::SpecialCard:
        w.field("type", "SpecialCard");
        w.key("card");
        write_card_name(w, sim, r.st().cards[0]);
        break;
    case RewardType::RemoveCard: w.field("type", "RemoveCard"); break;
    default: w.field("type", name(r.Type())); break;
    }
    w.end_object();
}

}  // namespace sts2
