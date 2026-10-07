// Starting a new run: StartRunLobby.BeginRunLocally (act selection),
// Player.CreateForNewRun, RunState.CreateForNewRun and
// RunManager.SetUpNewSingleplayer (grab bags, ascension, room generation).
#pragma once

#include <array>
#include <string_view>

#include "engine/sim/state.h"

namespace sts2 {

struct NewRunOptions {
    std::string_view seed;  // any case; canonicalized like SeedHelper.CanonicalizeSeed
    CharacterId character = CharacterId::IRONCLAD;
    int ascension = 0;
    ProfileConfig profile;
    int32_t progress_wongo_points = 0;
    int32_t progress_character_wins = 0;
    AncientEventProgressState progress_ancient_stats;
};

// ActModel.GetRandomList with Rng(hash(seed), "act_selection").
StaticVec<ActId, kMaxActs> choose_acts(std::string_view canonical_seed, const ProfileConfig& profile);

// Resets `out` and starts a new run in it.
void new_run(GameState& out, const NewRunOptions& options);

// The generated map of act `act_index` of `run` (StandardActMap.CreateFor).
ActMap generate_map_for_act(const RunData& run, int act_index);

}  // namespace sts2
