#include "search/combatsolver/mod_api.h"
#include "search/combatsolver/solver.h"
#include "game/All.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

using namespace sts2;
using namespace sts2::combatsolver;
namespace {
void Require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
int32_t Cancel(void* context) { return ++*static_cast<int*>(context) > 3; }
int32_t AlreadyCancelled(void*) { return 1; }
void Check() {
    using Session = std::unique_ptr<void, decltype(&sts2_solver_destroy)>;
    Session session(sts2_solver_create(), sts2_solver_destroy);
    Require(session != nullptr && sts2_solver_abi_version() == 1, "ABI session failed");
    sts2_solver_config config{};
    Require(sts2_solver_default_config(&config, sizeof(config) - 1) == STS2_SOLVER_INVALID,
        "short configuration buffer accepted");
    Require(sts2_solver_default_config(&config, sizeof(config)) == STS2_SOLVER_OK, "defaults failed");
    const std::string schemaRequest = R"({"ascension":0,"monsters":[]})";
    Require(sts2_solver_live_schema(session.get(), schemaRequest.data(), schemaRequest.size()) == STS2_SOLVER_OK,
        sts2_solver_last_error(session.get()));
    const auto* borrowed = sts2_solver_last_result(session.get());
    const std::string schema = borrowed;
    Require(schema.find("sts2solver.live_schema") != std::string::npos, "capture schema missing");
    Require(sts2_solver_live_schema(session.get(), borrowed, schema.size()) != STS2_SOLVER_OK,
        "schema response accepted as request");
    Require(sts2_solver_live_schema(session.get(), schemaRequest.data(), schemaRequest.size()) == STS2_SOLVER_OK,
        "schema session recovery failed");
    config.beam_width = 8; config.max_nodes = 500; config.time_budget_ms = 2000;
    config.transition_cache_mib = 8;
    Require(sts2_solver_solve(session.get(), &config, nullptr, nullptr) == STS2_SOLVER_INVALID,
        "search without root accepted");

    Env root;
    root.ResetCombat({.seed = "CSMODABI", .character = CharacterId::IRONCLAD}, EncounterId::CULTISTS_NORMAL);
    // A tiny winning fixture makes final display facts independently checkable.
    for (auto enemy : CombatState(&root.sim()).Enemies()) enemy.st().current_hp = 1;
    const auto snapshot = root.ExportSnapshotJson();
    const auto rootHash = root.Hash();
    Require(sts2_solver_import_snapshot(session.get(), snapshot.data(), snapshot.size()) == STS2_SOLVER_OK,
        sts2_solver_last_error(session.get()));
    Require(sts2_solver_root_hash(session.get()) == rootHash, "import changed root");
    Require(sts2_solver_solve(session.get(), &config, nullptr, nullptr) == STS2_SOLVER_OK,
        sts2_solver_last_error(session.get()));
    const std::string result = sts2_solver_last_result(session.get());
    Require(result.find("\"display_only\":true") != std::string::npos
        && result.find("\"replay_verified\":true") != std::string::npos
        && result.find("\"status\":\"victory\"") != std::string::npos
        && result.find("\"model_id\":") != std::string::npos
        && result.find("\"turn\":1") != std::string::npos, "missing verified textual victory route");
    CombatSearchConfig directConfig;
    directConfig.Profile.BeamWidth = config.beam_width;
    directConfig.Profile.MaxExpandedNodes = config.max_nodes;
    directConfig.Profile.SoftTimeBudgetMilliseconds = config.time_budget_ms;
    directConfig.TransitionCacheBytes = 8 * 1024 * 1024;
    const auto expected = CombatSolver::Solve(root, directConfig);
    Require(expected.Quality.Won && result.find("\"final_hash\":" + std::to_string(expected.FinalSearchHash))
        != std::string::npos, "ABI differs from actual native solver");
    Require(sts2_solver_root_hash(session.get()) == rootHash && root.ExportSnapshotJson() == snapshot,
        "search mutated caller root");
    Require(sts2_solver_solve(session.get(), &config, AlreadyCancelled, nullptr) == STS2_SOLVER_CANCELLED
        && std::string(sts2_solver_last_result(session.get())).empty(), "pre-cancel published a result");

    root.ResetCombat({.seed = "CSMODCANCEL", .character = CharacterId::SILENT}, EncounterId::CULTISTS_NORMAL);
    const auto second = root.ExportSnapshotJson();
    Require(sts2_solver_import_snapshot(session.get(), second.data(), second.size()) == STS2_SOLVER_OK,
        sts2_solver_last_error(session.get()));
    int polls = 0;
    Require(sts2_solver_solve(session.get(), &config, Cancel, &polls) == STS2_SOLVER_CANCELLED
        && polls > 3 && std::string(sts2_solver_last_result(session.get())).empty(),
        "active cancellation did not discard result");
    config.max_nodes = 100;
    config.max_turns = 1;
    Require(sts2_solver_solve(session.get(), &config, nullptr, nullptr) == STS2_SOLVER_OK,
        sts2_solver_last_error(session.get()));
    Require(std::string(sts2_solver_last_result(session.get())).find("\"status\":\"partial\"") != std::string::npos,
        "incomplete route mislabeled as defeat or victory");
    config.abi_version = 99;
    Require(sts2_solver_solve(session.get(), &config, nullptr, nullptr) == STS2_SOLVER_INVALID,
        "incompatible ABI accepted");
    config.abi_version = 1;
    Require(sts2_solver_import_snapshot(session.get(), "{}", 2) != STS2_SOLVER_OK
        && sts2_solver_root_hash(session.get()) == 0, "failed import retained old root");
    Require(sts2_solver_solve(session.get(), &config, nullptr, nullptr) == STS2_SOLVER_INVALID,
        "failed capture silently searched stale root");
    // Recovery uses the same session after both cancellation and invalid input.
    Require(sts2_solver_import_snapshot(session.get(), second.data(), second.size()) == STS2_SOLVER_OK
        && sts2_solver_solve(session.get(), &config, nullptr, nullptr) == STS2_SOLVER_OK,
        sts2_solver_last_error(session.get()));
}
}
int main() {
    try { Check(); std::cout << "Mod search ABI: same solver, display replay, partial outcome, cancellation and recovery passed\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
