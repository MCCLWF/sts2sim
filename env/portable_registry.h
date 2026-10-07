#pragma once
#include <map>
#include <string>
#include <vector>
#include "env/snapshot_json.h"
#include "engine/json.h"
#include "engine/sim/state.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2::snapshot_detail {
struct PortableMachine {
    MoveMachineDef definition;
    std::vector<MonsterId> owners;
};
struct PortableRegistry {
    std::vector<PortableMachine> machines;
    std::vector<std::string> stuns;
    std::vector<std::string> event_options;
    std::vector<std::string> will_kill;
    std::map<uint32_t,std::string> localization;
    std::map<uint16_t,uint16_t> machine_ids;
    std::map<uint16_t,uint16_t> stun_ids;
    std::map<uint16_t,uint16_t> event_option_ids;
    std::map<uint16_t,uint16_t> will_kill_ids;
};
PortableRegistry collect_portable_registry(const GameState& state, const GameState* replay_base);
void remap_portable_state(GameState& state, const PortableRegistry& registry);
void validate_portable_registry_usage(const GameState& state, const GameState* base,
    const PortableRegistry& registry);
void write_portable_registry(JsonWriter& writer, const PortableRegistry& registry);
PortableRegistry read_portable_registry(const Json& json);
void install_portable_registry(PortableRegistry& registry);
void write_snapshot_json(JsonWriter& writer, const Json& json);
std::string portable_payload(const Json& logical_state, const Json& registry);
uint64_t portable_wire_hash(std::string_view payload);
std::string native_runtime_abi();
}
