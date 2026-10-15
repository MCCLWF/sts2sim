// See pilot.h.
#include "nosl/pilot.h"

#include <chrono>

#include "engine/json.h"
#include "engine/sim/errors.h"
#include "game/All.h"

namespace sts2::nosl {

namespace {

const char* const kDecisionNames[] = {"Map", "Combat", "Rewards", "Event", "RestSite", "Shop", "Treasure", "Choice", "Ended", "Boundary"};
const char* const kActionNames[] = {"StartRun", "MapMove", "PlayCard", "EndTurn", "TakeReward", "Proceed", "EventOption",
                                    "RestOption", "ShopBuy", "TreasurePick", "UsePotion", "DiscardPotion", "Answer", "Resume"};
const char* const kChoiceNames[] = {"CardSelect", "Bundle", "Option"};

}  // namespace

Pilot::Pilot(const PilotOptions& options)
    : actor_(options.weights, options.actor_filter, options.policy_seed, options.greedy) {
    env_.Reset(NewRunOptions{.seed = options.seed, .character = options.character, .ascension = options.ascension});
}

bool Pilot::ended() const { return env_.decision().kind == DecisionKind::Ended; }

std::string Pilot::Digest(const Env& env) {
    std::unique_ptr<Env> scratch = env.Clone();  // getters take a mutable Sim
    Sim* sim = &scratch->sim();
    const GameState& g = scratch->state();
    Player player(sim, 1);
    Creature self = player.Creature();
    std::string s = "a" + std::to_string(g.run.current_act_index) + " f" + std::to_string(g.run.total_floor) +
                    " hp" + std::to_string(self.CurrentHp()) + "/" + std::to_string(self.MaxHp()) +
                    " g" + std::to_string(player.Gold()) + " d" + std::to_string(player.Deck().Cards().size());
    int32_t relics = 0;
    for (RelicModel relic : player.Relics()) { (void)relic; ++relics; }
    s += " r" + std::to_string(relics) + " p[";
    for (int32_t slot = 0; slot < player.MaxPotionCount(); ++slot) {
        PotionModel potion = player.GetPotionAtSlotIndex(slot);
        if (slot) s += ",";
        s += potion ? std::string(name(potion.PotionId_())) : "-";
    }
    s += "]";
    PlayerCombatState pcs = player.PlayerCombatState();
    if (g.combat.is_in_progress && pcs) {
        CombatState combat(sim);
        s += " | b" + std::to_string(self.Block()) + " e" + std::to_string(pcs.Energy()) + " t" + std::to_string(pcs.TurnNumber()) + " h[";
        bool first = true;
        for (CardModel card : pcs.Hand().Cards()) {
            if (!first) s += ",";
            first = false;
            s += std::string(name(card.CardId_()));
            if (card.CurrentUpgradeLevel() > 0) s += "+" + std::to_string(card.CurrentUpgradeLevel());
        }
        s += "] dr" + std::to_string(pcs.DrawPile().Count()) + " di" + std::to_string(pcs.DiscardPile().Count()) +
             " ex" + std::to_string(pcs.ExhaustPile().Count()) + " | m[";
        first = true;
        for (Creature enemy : combat.Enemies()) {
            if (!first) s += ",";
            first = false;
            s += std::string(name(enemy.Monster().MonsterId_())) + ":" + std::to_string(enemy.CurrentHp()) + ":" + std::to_string(enemy.Block());
        }
        s += "]";
    }
    return s;
}

namespace {

void WriteChoice(JsonWriter& w, const ChoiceRequest& r) {
    w.key("choice").begin_object().field("kind", kChoiceNames[static_cast<size_t>(r.kind)]).field("source", r.source)
        .field("prompt", r.prompt).field("subset", r.subset).field("min", r.min_select).field("max", r.max_select)
        .field("options", r.option_count).field("can_skip", r.can_skip).end_object();
}

}  // namespace

std::string Pilot::Peek() const {
    const Decision decision = env_.decision();
    JsonWriter w;
    w.begin_object().field("step", step_).field("decision", kDecisionNames[static_cast<size_t>(decision.kind)]);
    if (decision.kind == DecisionKind::Choice) WriteChoice(w, decision.request);
    w.field("digest", Digest(env_)).end_object();
    return w.str();
}

std::string Pilot::Next() {
    const auto begin = std::chrono::steady_clock::now();
    const Decision decision = env_.decision();
    JsonWriter w;
    w.begin_object().field("step", step_).field("decision", kDecisionNames[static_cast<size_t>(decision.kind)]);
    if (decision.kind == DecisionKind::Ended) {
        w.field("victory", env_.state().run.is_victory).field("floor", env_.state().run.total_floor)
            .field("digest", Digest(env_)).end_object();
        return w.str();
    }
    nosl::Observe(env_, obs_);
    const Actor::Decision chosen = actor_.Decide(obs_);
    const bool forced = chosen.forced;
    const size_t row = chosen.row;
    const double prob = chosen.prob;
    const float value = chosen.value;
    const Action action = chosen.action;
    w.field("forced", forced).key("action").begin_object()
        .field("kind", kActionNames[static_cast<size_t>(action.kind)]).field("index", action.index).field("target", action.target)
        .field("col", static_cast<int32_t>(action.coord.col)).field("row", static_cast<int32_t>(action.coord.row)).end_object();
    if (decision.kind == DecisionKind::Choice) WriteChoice(w, decision.request);
    w.field("digest", Digest(env_)).field("row", static_cast<int64_t>(row)).field("rows", static_cast<int64_t>(obs_.legal.size()))
        .field("prob", prob).field("value", static_cast<double>(value));
    if (chosen.capped) w.field("capped", true);   // the actor's turn cap ended the turn (nosl/actor.h)
    env_.Step(action);
    ++step_;
    w.field("ms", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count()).end_object();
    return w.str();
}

}  // namespace sts2::nosl
