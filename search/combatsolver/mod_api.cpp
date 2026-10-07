#include "search/combatsolver/mod_api.h"
#include "search/combatsolver/live_schema.h"
#include "search/combatsolver/solver.h"
#include "engine/json.h"
#include "game/All.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>

using namespace sts2;
using namespace sts2::combatsolver;
namespace {
constexpr uint32_t kAbiVersion = 1;
struct Session {
    std::unique_ptr<Env> Root;
    std::string Result;
    std::string Error;
};

template<class F> int32_t Call(Session& session, F&& function) noexcept {
    session.Result.clear();
    session.Error.clear();
    try { function(); return STS2_SOLVER_OK; }
    catch (const InvalidRequest& e) { session.Error = e.what(); return STS2_SOLVER_INVALID; }
    catch (const std::invalid_argument& e) { session.Error = e.what(); return STS2_SOLVER_INVALID; }
    catch (const Unported& e) { session.Error = e.what(); return STS2_SOLVER_UNSUPPORTED; }
    catch (const UnsupportedOperation& e) { session.Error = e.what(); return STS2_SOLVER_UNSUPPORTED; }
    catch (const std::exception& e) { session.Error = e.what(); return STS2_SOLVER_ERROR; }
    catch (...) { session.Error = "unknown native search failure"; return STS2_SOLVER_ERROR; }
}

CombatSearchConfig Convert(const sts2_solver_config& source) {
    if (source.abi_version != kAbiVersion || source.struct_size != sizeof(source))
        throw InvalidRequest("search ABI/configuration version mismatch");
    if (source.potion_policy < 0 || source.potion_policy > 2
        || source.transition_cache_mib < 0
        || (source.use_portfolio != 0 && source.use_portfolio != 1)
        || (source.stop_at_hp_target != 0 && source.stop_at_hp_target != 1))
        throw InvalidRequest("invalid search configuration");
    CombatSearchConfig config;
    config.Profile.BeamWidth = source.beam_width;
    config.Profile.MaxExpandedNodes = source.max_nodes;
    config.Profile.SoftTimeBudgetMilliseconds = source.time_budget_ms;
    config.Profile.MaxCardBranchesPerNode = source.max_card_branches;
    config.Profile.MaxPileChoiceBranchesPerAction = source.max_pile_choice_branches;
    config.Profile.MaxHandChoiceBranchesPerAction = source.max_hand_choice_branches;
    config.Profile.ReallocatedRefinementPortfolio = source.use_portfolio != 0;
    config.Profile.StopPortfolioAtHpTarget = source.stop_at_hp_target != 0;
    config.ThreadCount = source.threads;
    config.MaxTurns = source.max_turns;
    config.MaxActionsPerTurn = source.max_actions_per_turn;
    config.MaxChoiceLeavesPerAction = std::max(source.max_pile_choice_branches, source.max_hand_choice_branches);
    config.PotionPolicy = static_cast<SolverPotionPolicy>(source.potion_policy);
    config.TransitionCacheBytes = static_cast<size_t>(source.transition_cache_mib) * 1024 * 1024;
    return config;
}

void WriteBudget(JsonWriter& w, const sts2_solver_config& c) {
    w.begin_object().field("beam_width", c.beam_width).field("max_nodes", c.max_nodes)
        .field("time_budget_ms", c.time_budget_ms).field("max_card_branches", c.max_card_branches)
        .field("max_pile_choice_branches", c.max_pile_choice_branches)
        .field("max_hand_choice_branches", c.max_hand_choice_branches)
        .field("threads", c.threads).field("max_turns", c.max_turns)
        .field("max_actions_per_turn", c.max_actions_per_turn).field("potion_policy", c.potion_policy)
        .field("use_portfolio", c.use_portfolio != 0).field("stop_at_hp_target", c.stop_at_hp_target != 0)
        .end_object();
}

// Resolve identities against each pre-action state. Generated cards and targets
// therefore have their real branch-local identities instead of root hand indices.
void WriteStep(JsonWriter& w, const CombatSearchState& current, const Action& action, size_t ordinal) {
    auto inspect = current.CloneEnv();
    Player player(&inspect->sim(), 1);
    const auto decision = current.DecisionPoint();
    const char* kind = nullptr;
    switch (action.kind) {
        case Action::PlayCard: kind = "play_card"; break;
        case Action::EndTurn: kind = "end_turn"; break;
        case Action::UsePotion: kind = "use_potion"; break;
        case Action::DiscardPotion: kind = "discard_potion"; break;
        case Action::Answer: kind = "answer"; break;
        default: throw GameError("unsupported search result action");
    }
    w.begin_object().field("ordinal", static_cast<int64_t>(ordinal))
        .field("turn", current.Facts().PlayerTurn).field("kind", kind)
        .field("index", action.index).field("target_combat_id", action.target);
    if (action.kind == Action::PlayCard) {
        const auto hand = player.PlayerCombatState().Hand().Cards();
        if (action.index < 0 || static_cast<size_t>(action.index) >= hand.size())
            throw GameError("display route card index is outside its replay hand");
        auto card = hand[action.index];
        w.field("model_id", name(card.CardId_())).field("upgrade", card.CurrentUpgradeLevel());
    } else if (action.kind == Action::UsePotion || action.kind == Action::DiscardPotion) {
        auto potion = player.GetPotionAtSlotIndex(action.index);
        w.field("model_id", name(potion.PotionId_()));
    }
    if (action.target >= 0) {
        for (auto creature : CombatState(&inspect->sim()).Creatures()) {
            if (creature.CombatId() != action.target) continue;
            w.field("target_is_player", creature.IsPlayer());
            if (auto monster = creature.Monster()) w.field("target_model_id", name(monster.MonsterId_()));
            break;
        }
    }
    if (action.kind == Action::Answer) {
        w.field("choice_source", decision.request.source).field("choice_prompt", decision.request.prompt)
            .field("choice_is_count", decision.request.subset && decision.request.prompt == "count")
            .key("choice_cards").begin_array();
        if (!(decision.request.subset && decision.request.prompt == "count") && action.index >= 0
            && static_cast<size_t>(action.index) < decision.request.options.size()) {
            for (auto ref : decision.request.options[action.index].cards) {
                CardModel card(&inspect->sim(), ref);
                w.begin_object().field("model_id", name(card.CardId_()))
                    .field("upgrade", card.CurrentUpgradeLevel()).end_object();
            }
        }
        w.end_array();
    }
    w.end_object();
}

std::string ResultJson(const Env& root, const CombatSearchResult& result, const sts2_solver_config& config,
    double searchMilliseconds) {
    JsonWriter w;
    w.begin_object().field("format", "sts2sim.solver_result").field("version", 1)
        .field("display_only", true).field("root_hash", result.RootHash)
        .field("final_hash", result.FinalSearchHash).field("search_ms", searchMilliseconds)
        .key("budget"); WriteBudget(w, config);
    const auto& q = result.Quality;
    w.key("metrics").begin_object().field("expanded", result.Metrics.ExpandedNodes)
        .field("native_predictions", result.Metrics.NativePredictions)
        .field("stop_reason", result.Metrics.StopReason).field("passes", result.Metrics.Passes)
        .field("node_unit", "portfolio_parent_nodes").end_object().key("steps").begin_array();
    auto replay = CombatSearchState::Capture(root);
    size_t ordinal = 0;
    for (const auto& action : result.Actions) {
        WriteStep(w, replay, action, ordinal++);
        replay = replay.Predict(action);
    }
    if (replay.NativeHash() != result.FinalSearchHash || replay.Facts().PlayerHp != result.FinalPlayerHp
        || (replay.Outcome() == CombatOutcome::Victory) != q.Won)
        throw GameError("display route replay disagrees with selected result");
    const auto outcome = replay.Outcome();
    w.end_array().key("outcome").begin_object()
        .field("status", outcome == CombatOutcome::Victory ? "victory"
            : outcome == CombatOutcome::Defeat ? "defeat" : "partial")
        .field("won", q.Won).field("combat_ended", outcome != CombatOutcome::Ongoing)
        .field("survival_estimate", q.Survives)
        .field("initial_hp", Player(&root.sim(), 1).Creature().CurrentHp())
        .field("final_hp", result.FinalPlayerHp).field("final_max_hp", result.FinalPlayerMaxHp)
        .field("hp_lost", q.ProjectedBattleHpLost).field("potions_used", q.ProjectedBattlePotionCount)
        .field("enemy_hp", q.EnemyHp).key("ended_turn");
    if (q.CombatEndedTurn) w.value(*q.CombatEndedTurn); else w.null();
    w.end_object().field("replay_verified", true).end_object();
    return w.str();
}
}

extern "C" uint32_t sts2_solver_abi_version() { return kAbiVersion; }
extern "C" int32_t sts2_solver_default_config(sts2_solver_config* target, size_t size) {
    if (!target || size != sizeof(*target)) return STS2_SOLVER_INVALID;
    *target = {kAbiVersion, sizeof(*target), 60, 120000, 120000, 32, 18, 24, 1, 64, 32, 1, 1, 1, 512};
    return STS2_SOLVER_OK;
}
extern "C" void* sts2_solver_create() {
    try { return new Session; } catch (...) { return nullptr; }
}
extern "C" void sts2_solver_destroy(void* handle) { delete static_cast<Session*>(handle); }
extern "C" int32_t sts2_solver_live_schema(void* handle, const char* data, size_t size) {
    if (!handle) return STS2_SOLVER_INVALID;
    auto& session = *static_cast<Session*>(handle);
    // The request can alias the borrowed previous result.
    try {
        const std::string input = data ? std::string(data, size) : std::string();
        return Call(session, [&] {
            if (input.empty()) throw InvalidRequest("missing capture schema request");
            session.Result = LiveCaptureSchema(input);
        });
    } catch (const std::exception& error) {
        session.Result.clear(); session.Error = error.what(); return STS2_SOLVER_ERROR;
    }
}
extern "C" int32_t sts2_solver_import_snapshot(void* handle, const char* data, size_t size) {
    if (!handle) return STS2_SOLVER_INVALID;
    auto& session = *static_cast<Session*>(handle);
    session.Root.reset();
    // Copy before Call clears borrowed result memory (data may alias it).
    try {
        const std::string input = data ? std::string(data, size) : std::string();
        return Call(session, [&] {
            if (!data || size == 0) throw InvalidRequest("missing combat snapshot");
            auto root = Env::ImportSnapshotJson(input);
            auto captured = CombatSearchState::Capture(*root);
            session.Result = captured.ExportSnapshotJson();
            session.Root = std::move(root);
        });
    } catch (const std::exception& e) {
        session.Result.clear(); session.Error = e.what(); return STS2_SOLVER_ERROR;
    }
}
extern "C" int32_t sts2_solver_solve(void* handle, const sts2_solver_config* source,
    sts2_solver_cancel_callback cancelled, void* context) {
    if (!handle) return STS2_SOLVER_INVALID;
    auto& session = *static_cast<Session*>(handle);
    std::atomic<bool> wasCancelled{false};
    const auto status = Call(session, [&] {
        if (!source || !session.Root) throw InvalidRequest("search requires configuration and a valid root");
        auto config = Convert(*source);
        config.Cancelled = [&] {
            const bool value = cancelled && cancelled(context) != 0;
            if (value) wasCancelled.store(true);
            return value;
        };
        if (config.Cancelled()) return;
        const auto before = session.Root->Hash();
        const auto started = std::chrono::steady_clock::now();
        const auto result = CombatSolver::Solve(*session.Root, config);
        const double elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        if (config.Cancelled() || result.Metrics.StopReason == "cancelled") {
            wasCancelled.store(true); return;
        }
        if (session.Root->Hash() != before) throw GameError("native search changed the captured root");
        session.Result = ResultJson(*session.Root, result, *source, elapsed);
    });
    if (wasCancelled.load()) {
        session.Result.clear(); session.Error = "search cancelled"; return STS2_SOLVER_CANCELLED;
    }
    return status;
}
extern "C" const char* sts2_solver_last_result(void* handle) {
    return handle ? static_cast<Session*>(handle)->Result.c_str() : "";
}
extern "C" const char* sts2_solver_last_error(void* handle) {
    return handle ? static_cast<Session*>(handle)->Error.c_str() : "null search session";
}
extern "C" uint64_t sts2_solver_root_hash(void* handle) {
    auto* session = static_cast<Session*>(handle);
    return session && session->Root ? session->Root->Hash() : 0;
}
