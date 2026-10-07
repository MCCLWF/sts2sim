// Serialization of simulator state in the game's own save schema
// (SerializableRun), so it can be diffed field by field against saves written
// by the real game or by tools/Sts2Oracle.
#pragma once

#include <string>

#include "engine/json.h"
#include "engine/sim/state.h"

namespace sts2 {

void write_run_save(JsonWriter& w, const GameState& g);
void write_map(JsonWriter& w, const ActMap& map);

std::string model_id(CardId id);
std::string model_id(RelicId id);
std::string model_id(PotionId id);
std::string model_id(EncounterId id);
std::string model_id(EventId id);
std::string model_id(ActId id);
std::string model_id(CharacterId id);

}  // namespace sts2
