// JSON dumps of the simulator state in the oracle's formats
// (tools/Sts2Oracle/OracleCombat.cs Dump, OracleRun.cs DumpRun).
#pragma once

#include <cstdint>

#include "engine/json.h"
#include "engine/sim/sim.h"

namespace sts2 {

class Reward;
void write_reward(JsonWriter& w, Sim* sim, Reward reward);

void write_rng_counters(JsonWriter& w, const RunData& run);
void write_combat_state(JsonWriter& w, Sim* sim);  // OracleCombat.Dump
void write_run(JsonWriter& w, Sim* sim);           // OracleRun.DumpRun
void write_card_name(JsonWriter& w, Sim* sim, uint32_t card);

}  // namespace sts2
