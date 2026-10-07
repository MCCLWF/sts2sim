#include "search/combatsolver/solver.h"
#include "engine/json.h"
#include "tools/cli/dump.h"
#include "game/All.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PotionCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Runs/RunManager.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <sstream>

using namespace sts2;
using namespace sts2::combatsolver;
namespace {
template<class Id, size_t N> Id ParseId(const std::string& value, const std::array<std::string_view, N>& names) {
    for (size_t i = 1; i < names.size(); ++i) if (names[i] == value) return static_cast<Id>(i);
    throw std::invalid_argument("unknown canonical model ID " + value);
}
std::vector<std::string> Split(const std::string& value) {
    std::vector<std::string> parts;
    std::istringstream input(value);
    std::string part;
    while (std::getline(input, part, ',')) if (!part.empty()) parts.push_back(part);
    return parts;
}
// Controlled nonstarter loadouts use the same run setup and game commands as
// the replay CLI. They are benchmark fixtures, not sampled natural-run states.
void PrepareLoadout(Env& env, const NewRunOptions& options, EncounterId encounter, int32_t act,
    const std::string& deck, const std::string& relics, const std::string& potions) {
    new_run(env.state(), options);
    Player player(&env.sim(), 1);
    if (!deck.empty()) {
        for (auto card : player.Deck().Cards()) CardPileCmd::RemoveFromDeck(card, false);
        for (const auto& spec : Split(deck)) {
            const auto plus = spec.find('+');
            auto card = RunState(&env.sim()).CreateCard(ParseId<CardId>(spec.substr(0, plus), kCardIdNames), player);
            CardPileCmd::Add(card, PileType::Deck, CardPilePosition::Bottom, AbstractModel{}, true);
            const auto upgrades = plus == std::string::npos ? 0 : std::stoi(spec.substr(plus + 1));
            if (upgrades < 0) throw std::invalid_argument("negative fixture card upgrade");
            for (int i = 0; i < upgrades; ++i) CardCmd::Upgrade(card);
        }
    }
    for (const auto& id : Split(relics)) RelicCmd::Obtain(ParseId<RelicId>(id, kRelicIdNames), player);
    for (const auto& id : Split(potions)) PotionCmd::TryToProcure(ParseId<PotionId>(id, kPotionIdNames), player);
    RunManager::SetActInternal(&env.sim(), act);
    RunManager::EnterMapCoordDebug(&env.sim(), MapCoord{0, 0}, encounter);
    env.state().run.phase = RunPhase::Combat;
}
void WriteQuality(JsonWriter& w, const CombatSearchResult& result) {
    const auto& q = result.Quality;
    w.begin_object().field("won", q.Won).field("survives", q.Survives)
        .field("death_save_count", q.DeathSaveUseCount).field("strategic_hp_deficit", q.StrategicHpDeficit)
        .field("battle_hp_lost", q.ProjectedBattleHpLost).field("potion_count", q.ProjectedBattlePotionCount)
        .field("potion_cost", q.PotionStrategicCost).field("enemy_hp", q.EnemyHp)
        .field("final_hp", result.FinalPlayerHp).field("final_max_hp", result.FinalPlayerMaxHp)
        .field("score", q.Score).key("ended_turn");
    if (q.CombatEndedTurn) w.value(*q.CombatEndedTurn); else w.null();
    w.end_object();
}
void ReplayPlan(JsonWriter& w, const Env& root, const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("cannot read replay plan " + path);
    auto current = CombatSearchState::Capture(root);
    w.key("steps").begin_array();
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream input(line);
        std::string kind;
        input >> kind;
        Action action;
        if (kind == "EndTurn") action.kind = Action::EndTurn;
        else if (kind == "Answer") { action.kind = Action::Answer; input >> action.index; }
        else if (kind == "PlayCard") {
            std::string id; int32_t occurrence, target;
            if (!(input >> id >> occurrence >> target)) throw std::invalid_argument("malformed replay card line");
            auto inspection = current.CloneEnv();
            Player player(&inspection->sim(), 1);
            action.kind = Action::PlayCard; action.index = -1;
            auto hand = player.PlayerCombatState().Hand().Cards();
            for (size_t i = 0; i < hand.size(); ++i)
                if (name(hand[i].CardId_()) == id && occurrence-- == 0) { action.index = static_cast<int32_t>(i); break; }
            if (action.index < 0) throw std::runtime_error("replay card absent from hand: " + id);
            if (target >= 0) {
                for (auto enemy : CombatState(&inspection->sim()).Enemies())
                    if (enemy.CombatId() == target) action.target = enemy.CombatId();
                if (action.target < 0) throw std::runtime_error("replay target absent");
            }
        } else throw std::invalid_argument("unsupported replay line " + kind);
        try { current = current.Predict(action); }
        catch (const std::exception& error) { throw std::runtime_error("replay line '" + line + "': " + error.what()); }
        w.begin_object().field("command", line).key("state");
        auto inspection = current.CloneEnv(); write_combat_state(w, &inspection->sim()); w.end_object();
    }
    w.end_array().field("won", current.Outcome() == CombatOutcome::Victory)
        .field("final_hp", current.Facts().PlayerHp).field("final_max_hp", current.Facts().PlayerMaxHp)
        .field("final_hash", current.NativeHash());
}
}
int main(int argc, char** argv) {
    try {
        CombatSearchConfig config;
        std::string seed = "CSPROBE1";
        NewRunOptions options{.character = CharacterId::IRONCLAD};
        EncounterId encounter = EncounterId::CULTISTS_NORMAL;
        int32_t act = 0;
        std::string snapshot, output, exportRoot, replayPlan;
        std::string deck, relics, potions;
        std::vector<Action> prepareActions;
        bool exportOnly = false;
        bool portableRoot = false;
        for (int i = 1; i < argc; ++i) {
            const std::string flag = argv[i];
            if (i + 1 >= argc) throw std::invalid_argument("missing value for " + flag);
            const std::string value = argv[++i];
            if (flag == "--seed") seed = value;
            else if (flag == "--character") options.character = ParseId<CharacterId>(value, kCharacterIdNames);
            else if (flag == "--encounter") encounter = ParseId<EncounterId>(value, kEncounterIdNames);
            else if (flag == "--ascension") options.ascension = std::stoi(value);
            else if (flag == "--act-index") act = std::stoi(value);
            else if (flag == "--beam") config.Profile.BeamWidth = std::stoi(value);
            else if (flag == "--nodes") config.Profile.MaxExpandedNodes = std::stoi(value);
            else if (flag == "--budget-ms") config.Profile.SoftTimeBudgetMilliseconds = std::stoi(value);
            else if (flag == "--max-turns") config.MaxTurns = std::stoi(value);
            else if (flag == "--deck") deck = value;
            else if (flag == "--relics") relics = value;
            else if (flag == "--potions") potions = value;
            else if (flag == "--export-only") exportOnly = std::stoi(value) != 0;
            else if (flag == "--export-root-format") {
                if (value != "local" && value != "portable") throw std::invalid_argument("unknown root export format");
                portableRoot = value == "portable";
            }
            else if (flag == "--prepare-action") {
                const auto fields = Split(value);
                if (fields.size() != 3) throw std::invalid_argument("prepare action needs kind,index,target");
                Action action;
                if (fields[0] == "PlayCard") action.kind = Action::PlayCard;
                else if (fields[0] == "UsePotion") action.kind = Action::UsePotion;
                else if (fields[0] == "Answer") action.kind = Action::Answer;
                else if (fields[0] == "EndTurn") action.kind = Action::EndTurn;
                else throw std::invalid_argument("unknown prepare action kind");
                action.index = std::stoi(fields[1]); action.target = std::stoi(fields[2]);
                prepareActions.push_back(action);
            }
            else if (flag == "--profile") config.CollectPhaseTimings = std::stoi(value) != 0;
            else if (flag == "--cache-entries") config.EvaluationCacheEntries = std::stoi(value);
            else if (flag == "--transition-cache-entries") config.TransitionCacheEntries = std::stoi(value);
            else if (flag == "--transition-cache-mib") {
                const auto mib = std::stoi(value);
                if (mib < 0) throw std::invalid_argument("transition cache memory must be nonnegative");
                config.TransitionCacheBytes = static_cast<size_t>(mib) * 1024 * 1024;
            }
            else if (flag == "--reuse-end-turn") config.ReuseEndTurnForecast = std::stoi(value) != 0;
            else if (flag == "--prune-before-scoring") config.PruneBeforeScoring = std::stoi(value) != 0;
            else if (flag == "--skip-equivalent-focus") config.SkipEquivalentFocusPasses = std::stoi(value) != 0;
            else if (flag == "--threads") config.ThreadCount = std::stoi(value);
            else if (flag == "--snapshot") snapshot = value;
            else if (flag == "--export-root") exportRoot = value;
            else if (flag == "--replay-plan") replayPlan = value;
            else if (flag == "--out") output = value;
            else if (flag == "--potion-policy") {
                if (value == "Disabled") config.PotionPolicy = SolverPotionPolicy::Disabled;
                else if (value == "Smart") config.PotionPolicy = SolverPotionPolicy::Smart;
                else if (value == "RequireAtLeastOne") config.PotionPolicy = SolverPotionPolicy::RequireAtLeastOne;
                else throw std::invalid_argument("unknown potion policy " + value);
            } else throw std::invalid_argument("unknown search option " + flag);
        }
        std::unique_ptr<Env> root;
        options.seed = seed;
        if (snapshot.empty()) {
            root = std::make_unique<Env>();
            if (deck.empty() && relics.empty() && potions.empty()) root->ResetCombat(options, encounter, act);
            else {
                if (act < 0 || act >= 3) throw std::invalid_argument("fixture act index out of range");
                PrepareLoadout(*root, options, encounter, act, deck, relics, potions);
            }
        } else {
            if (!deck.empty() || !relics.empty() || !potions.empty())
                throw std::invalid_argument("snapshot and fixture loadout options cannot be combined");
            std::ifstream input(snapshot);
            if (!input) throw std::runtime_error("cannot read root snapshot " + snapshot);
            const std::string json((std::istreambuf_iterator<char>(input)), {});
            root = Env::ImportSnapshotJson(json);
        }
        for (auto action : prepareActions)
            root = CombatSearchState::Capture(*root).Predict(action).CloneEnv();
        const auto sourceBefore = root->ExportSnapshotJson();
        if (!exportRoot.empty()) {
            std::ofstream file(exportRoot);
            if (!file) throw std::runtime_error("cannot export root " + exportRoot);
            file << (portableRoot ? root->ExportPortableSnapshotJson() : sourceBefore);
        }
        if (exportOnly) {
            if (exportRoot.empty()) throw std::invalid_argument("export-only needs --export-root");
            return 0;
        }
        JsonWriter w;
        w.begin_object().field("schema", "sts2.combatsolver.result.v1").field("upstream", "d7b4620")
            .field("search_organization", "native_turn_beam").key("root");
        auto inspection = root->Clone(); write_combat_state(w, &inspection->sim());
        if (!replayPlan.empty()) {
            ReplayPlan(w, *root, replayPlan);
            if (root->ExportSnapshotJson() != sourceBefore) throw std::runtime_error("replay modified caller root");
            w.end_object();
            if (output.empty()) std::cout << w.str() << '\n';
            else { std::ofstream file(output); if (!file) throw std::runtime_error("cannot write replay result"); file << w.str() << '\n'; }
            return 0;
        }
        const auto result = CombatSolver::Solve(*root, config);
        if (root->ExportSnapshotJson() != sourceBefore) throw std::runtime_error("search modified caller root");
        w.field("root_hash", result.RootHash).field("final_search_hash", result.FinalSearchHash).key("quality");
        WriteQuality(w, result);
        const auto& m = result.Metrics;
        w.key("metrics").begin_object().field("expanded", m.ExpandedNodes).field("transitions", m.Transitions)
            .field("forecast_transitions", m.ForecastTransitions).field("route_replay_transitions", m.RouteReplayTransitions)
            .field("choice_answers", m.ChoiceAnswers).field("transposition_pruned", m.TranspositionPruned)
            .field("early_transposition_pruned", m.EarlyTranspositionPruned)
            .field("equivalent_focus_passes_skipped", m.EquivalentFocusPassesSkipped)
            .field("requested_threads", config.ThreadCount).field("peak_concurrent_passes", m.PeakConcurrentPasses)
            .field("duplicate_actions_pruned", m.DuplicateActionsPruned).field("action_truncated", m.ActionTruncated)
            .field("choice_truncated", m.ChoiceTruncated).field("searched_turns", m.SearchedTurns)
            .field("passes", m.Passes)
            .field("board_captures", m.BoardCaptures).field("board_ms", m.BoardMilliseconds)
            .field("board_cache_hits", m.BoardCacheHits).field("forecast_cache_hits", m.ForecastCacheHits)
            .field("transition_cache_hits", m.TransitionCacheHits).field("transition_cache_stored", m.TransitionCacheStored)
            .field("native_predictions", m.NativePredictions).field("end_turn_reused", m.EndTurnReused)
            .field("transition_cache_estimated_bytes", m.TransitionCacheEstimatedBytes)
            .field("transition_ms", m.TransitionMilliseconds).field("forecast_ms", m.ForecastMilliseconds)
            .field("legal_actions_ms", m.LegalActionsMilliseconds).field("retain_ms", m.RetainMilliseconds)
            .field("elapsed_ms", m.ElapsedMilliseconds).field("stop_reason", m.StopReason)
            .key("pass_details").begin_array();
        for (const auto& pass : m.PassDetails) {
            w.begin_object().field("beam", pass.BeamWidth).field("focus_target", pass.FocusTarget)
                .field("allows_potions", pass.AllowsPotions).field("completed", pass.Completed).field("expanded", pass.ExpandedNodes)
                .field("native_predictions", pass.NativePredictions).field("elapsed_ms", pass.ElapsedMilliseconds);
            if (pass.BestQuality) {
                const auto& q = *pass.BestQuality;
                w.field("won", q.Won).field("hp_lost", q.ProjectedBattleHpLost)
                    .field("hp_deficit", q.StrategicHpDeficit).field("potion_cost", q.PotionStrategicCost)
                    .field("potions", q.ProjectedBattlePotionCount).field("enemy_hp", q.EnemyHp);
            }
            w.end_object();
        }
        w.end_array().end_object();
        w.key("actions").begin_array();
        for (auto action : result.Actions)
            w.begin_object().field("kind", static_cast<int32_t>(action.kind))
                .field("index", action.index).field("target", action.target).end_object();
        w.end_array().end_object();
        if (output.empty()) std::cout << w.str() << '\n';
        else { std::ofstream file(output); if (!file) throw std::runtime_error("cannot write result " + output); file << w.str() << '\n'; }
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
