#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "content/generated/ids.h"
#include "engine/json.h"
#include "search/reference_ismcts.h"
#include "game/All.h"

using namespace sts2;

namespace {

template <class Id, size_t N>
Id parse_id(const std::array<std::string_view, N>& names, const std::string& text, const char* kind) {
    for (size_t i = 1; i < names.size(); ++i) if (names[i] == text) return static_cast<Id>(i);
    throw std::invalid_argument(std::string("unknown ") + kind + " " + text);
}

CharacterId parse_character(const std::string& text) {
    return parse_id<CharacterId>(kCharacterIdNames, text, "character");
}

EncounterId parse_encounter(const std::string& text) {
    return parse_id<EncounterId>(kEncounterIdNames, text, "encounter");
}

int value_arg(int argc, char** argv, const char* name, int fallback) {
    for (int i = 1; i + 1 < argc; ++i) if (std::string(argv[i]) == name) return std::stoi(argv[i + 1]);
    return fallback;
}

std::string text_arg(int argc, char** argv, const char* name, std::string fallback) {
    for (int i = 1; i + 1 < argc; ++i) if (std::string(argv[i]) == name) return argv[i + 1];
    return fallback;
}

void write_action(JsonWriter& w, const Env& env, const Action& action) {
    w.begin_object();
    const char* kind = action.kind == Action::PlayCard ? "PlayCard" :
                       action.kind == Action::EndTurn ? "EndTurn" : "Other";
    w.field("kind", kind).field("index", action.index).field("target", action.target);
    if (action.kind == Action::PlayCard) {
        CardPile hand = Player(const_cast<Sim*>(&env.sim()), 1).PlayerCombatState().Hand();
        if (action.index >= 0 && action.index < hand.Count()) {
            CardModel card(const_cast<Sim*>(&env.sim()), hand.refs()[static_cast<size_t>(action.index)]);
            w.field("card", name(card.CardId_()));
            w.field("upgrade", card.CurrentUpgradeLevel());
        }
    }
    w.end_object();
}

void write_root(JsonWriter& w, const Env& env) {
    Sim* sim = const_cast<Sim*>(&env.sim());
    Player player(sim, 1);
    w.key("hand").begin_array();
    for (CardModel card : player.PlayerCombatState().Hand().Cards()) w.value(name(card.CardId_()));
    w.end_array();
    w.key("enemies").begin_array();
    for (Creature enemy : CombatState(sim).Enemies()) {
        w.begin_object().field("id", enemy.CombatId()).field("monster", name(enemy.Monster().MonsterId_()))
            .field("hp", enemy.CurrentHp()).field("max_hp", enemy.MaxHp()).end_object();
    }
    w.end_array();
    w.field("energy", player.PlayerCombatState().Energy()).field("hp", player.Creature().CurrentHp())
        .field("legal_actions", static_cast<int32_t>(env.LegalActions().size()));
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::string seed = text_arg(argc, argv, "--seed", "DUAL001");
        std::string character = text_arg(argc, argv, "--character", "IRONCLAD");
        std::string encounter = text_arg(argc, argv, "--encounter", "CULTISTS_NORMAL");
        int ascension = value_arg(argc, argv, "--ascension", 0);
        int act = value_arg(argc, argv, "--act-index", 0);
        int node_budget = value_arg(argc, argv, "--node-budget", value_arg(argc, argv, "--iterations", 1000));
        int beam_width = value_arg(argc, argv, "--beam-width", 8);
        int depth = value_arg(argc, argv, "--rollout-depth", 16);

        Env env;
        env.ResetCombat(NewRunOptions{.seed = seed, .character = parse_character(character), .ascension = ascension},
                        parse_encounter(encounter), act);
        SearchConfig config{.rollout_depth = depth, .beam_width = beam_width, .node_budget = node_budget};
        auto started = std::chrono::steady_clock::now();
        std::vector<Action> plan = ReferenceBeam::ChoosePath(env, config);
        if (plan.empty()) throw std::runtime_error("beam returned an empty plan");
        Action action = plan.front();
        auto ended = std::chrono::steady_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(ended - started).count();

        JsonWriter w;
        w.begin_object().field("solver", "sts2sim_reference_beam").field("algorithm", "beam").field("seed", seed)
            .field("character", character).field("encounter", encounter).field("ascension", ascension)
            .field("node_budget", node_budget).field("beam_width", beam_width).field("depth", depth)
            .field("elapsed_ms", elapsed_ms)
            .field("state_hash", env.Hash());
        w.key("root").begin_object(); write_root(w, env); w.end_object();
        w.key("action"); write_action(w, env, action);
        w.key("plan").begin_array();
        for (const Action& planned : plan) {
            w.begin_object().field("action_kind", static_cast<int>(planned.kind))
                .field("action_index", planned.index).field("action_target", planned.target)
                .field("action_col", planned.coord.col).field("action_row", planned.coord.row).end_object();
        }
        w.end_array();
        w.end_object();
        std::fputs(w.str().c_str(), stdout);
        std::fputc('\n', stdout);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "env_solver error: %s\n", error.what());
        return 1;
    }
}
