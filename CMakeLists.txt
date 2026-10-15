cmake_minimum_required(VERSION 3.20)
project(sts2sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)  # unsigned __int128
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Bit-exactness with .NET: floating-point expressions must be evaluated exactly
# as written (no FMA contraction, no reassociation, no fast-math). Ordinary
# C# signed addition/subtraction/multiplication are unchecked; -fwrapv gives
# the port those semantics rather than C++ signed-overflow undefined behavior.
# Explicit checked operations (Decimal, LINQ sums/counts) still need guards.
add_compile_options(-ffp-contract=off -fno-fast-math -fwrapv -Wall -Wextra -Wno-missing-field-initializers)

add_library(sts2core STATIC core/dec.cpp)
set_target_properties(sts2core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(sts2core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Static content tables generated from data/content.json (tools/gen/gen_content.py).
add_library(sts2content STATIC content/generated/tables.cpp)
set_target_properties(sts2content PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(sts2content PUBLIC sts2core)

# Simulation engine.
add_library(sts2engine STATIC
  engine/map.cpp
  engine/run_setup.cpp
  engine/save_json.cpp
  engine/sim/gc.cpp
  engine/sim/rng_execution_observer.cpp)
target_link_libraries(sts2engine PUBLIC sts2content)
set_target_properties(sts2engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The C# game logic ported file by file (game/ mirrors src/Core).
file(GLOB_RECURSE STS2GAME_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/game/*.cpp)
add_library(sts2game STATIC ${STS2GAME_SOURCES})
target_link_libraries(sts2game PUBLIC sts2engine)
set_target_properties(sts2game PROPERTIES POSITION_INDEPENDENT_CODE ON)
# Ported signatures keep every C# parameter, used or not.
target_compile_options(sts2game PRIVATE -Wno-unused-parameter)

# The decision loop over a run (choice prefixes, snapshots).
add_library(sts2env STATIC env/env.cpp env/operation.cpp env/snapshot.cpp env/snapshot_import.cpp env/snapshot_validation.cpp env/portable_registry.cpp
  env/preview.cpp env/game_queries.cpp env/run_queries.cpp)
add_custom_target(check_state_field_schema
  COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen/gen_state_fields.py --check
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Checking complete owning-state field traversal")
add_dependencies(sts2env check_state_field_schema)
target_link_libraries(sts2env PUBLIC sts2game)
set_target_properties(sts2env PROPERTIES POSITION_INDEPENDENT_CODE ON)
include(cmake/EnvBuildIdentity.cmake)

# Dependency-free reference search used by M3 smoke tests and downstream
# agents. It deliberately depends only on the public Env surface.
add_library(sts2search STATIC search/reference_ismcts.cpp)
target_link_libraries(sts2search PUBLIC sts2env)
set_target_properties(sts2search PROPERTIES POSITION_INDEPENDENT_CODE ON)

# CombatSolver is an independent privileged Env consumer. Pure policy values
# are built separately so their upstream-source differential probe need not
# run or link the game simulation. The full solver is still being ported.
add_library(sts2combatsolver_policy STATIC search/combatsolver/policy.cpp)
target_include_directories(sts2combatsolver_policy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(sts2combatsolver_policy PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(sts2combatsolver STATIC search/combatsolver/env_state.cpp search/combatsolver/solver.cpp)
target_link_libraries(sts2combatsolver PUBLIC sts2env sts2combatsolver_policy)
set_target_properties(sts2combatsolver PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_executable(combatsolver_policy_probe tools/combatsolver/policy_probe.cpp)
target_link_libraries(combatsolver_policy_probe PRIVATE sts2combatsolver_policy)
add_executable(combatsolver_search tools/combatsolver/search_cli.cpp tools/cli/dump.cpp)
target_link_libraries(combatsolver_search PRIVATE sts2combatsolver)

# Silent A10 NoSL work: baseline policies and a whole-run measurement runner.
# Env consumers; timing-dependent, so not a CTest gate.
add_library(sts2nosl STATIC nosl/policies.cpp nosl/observation.cpp nosl/actor_filter.cpp nosl/policy_net.cpp nosl/resample.cpp)
target_link_libraries(sts2nosl PUBLIC sts2combatsolver)
add_executable(nosl_run tools/nosl/nosl_run.cpp)
target_link_libraries(nosl_run PRIVATE sts2nosl)
add_executable(nosl_obs_check tools/nosl/nosl_obs_check.cpp)
target_link_libraries(nosl_obs_check PRIVATE sts2nosl)
add_executable(nosl_script tools/nosl/nosl_script.cpp)
target_link_libraries(nosl_script PRIVATE sts2nosl)
add_executable(nosl_teacher_export tools/nosl/nosl_teacher_export.cpp)
target_link_libraries(nosl_teacher_export PRIVATE sts2nosl)
add_executable(nosl_combat_diag tools/nosl/nosl_combat_diag.cpp)
target_link_libraries(nosl_combat_diag PRIVATE sts2nosl)
add_executable(nosl_decision_diag tools/nosl/nosl_decision_diag.cpp)
target_link_libraries(nosl_decision_diag PRIVATE sts2nosl)
add_executable(nosl_card_table tools/nosl/nosl_card_table.cpp)
target_link_libraries(nosl_card_table PRIVATE sts2nosl)
add_executable(nosl_headroom tools/nosl/nosl_headroom.cpp)
target_link_libraries(nosl_headroom PRIVATE sts2nosl)
# The same forward pass built for speed, for offline tools that run it millions of times: the host's vector
# instructions and reassociated sums (the project-wide flags keep float sums in source order, which leaves the
# inner products scalar). Its logits differ from sts2nosl's in the last bits; nothing deployed links it.
add_library(sts2nosl_fastnet STATIC nosl/policy_net.cpp)
target_link_libraries(sts2nosl_fastnet PUBLIC sts2nosl)
target_compile_options(sts2nosl_fastnet PRIVATE -march=native -fno-math-errno -fno-trapping-math -fno-signed-zeros -fassociative-math)
add_executable(nosl_rollout_labels tools/nosl/nosl_rollout_labels.cpp)
target_link_libraries(nosl_rollout_labels PRIVATE sts2nosl_fastnet sts2nosl)
# The same-root tool with the fast forward pass, for its rollout player only (rollouts run the policy millions of
# times). Comparisons of a deployed policy use nosl_headroom, whose forward pass is the deployed one.
add_executable(nosl_headroom_fast tools/nosl/nosl_headroom.cpp)
target_link_libraries(nosl_headroom_fast PRIVATE sts2nosl_fastnet sts2nosl)
# The sampler ABI for Python (python/nosl): one single-threaded batch per process.
set_target_properties(sts2nosl PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(sts2nosl_c SHARED nosl/batch_api.cpp)
target_link_libraries(sts2nosl_c PRIVATE sts2nosl)
# The actor for a game-side executor (tools/NoslPilot): a shadow Env, the
# actor filter and the native forward pass behind one "next action" call.
add_library(sts2nosl_pilot SHARED nosl/actor.cpp nosl/pilot.cpp nosl/pilot_api.cpp)
target_link_libraries(sts2nosl_pilot PRIVATE sts2nosl)
option(STS2_PILOT_C_ABI_ONLY "Hide pilot C++ symbols for the standalone game Mod package" OFF)
if(STS2_PILOT_C_ABI_ONLY AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # Keep the bundled C++ implementation private to the C ABI. Steam/Godot may
  # already have an older libstdc++ loaded; it must not interpose our static one.
  target_link_options(sts2nosl_pilot PRIVATE "LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/nosl/pilot_exports.map")
  set_property(TARGET sts2nosl_pilot APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/nosl/pilot_exports.map")
endif()
add_executable(nosl_pilot tools/nosl/nosl_pilot.cpp)
target_link_libraries(nosl_pilot PRIVATE sts2nosl_pilot sts2nosl)
add_executable(nosl_actor_check tools/nosl/nosl_actor_check.cpp nosl/actor.cpp)
target_link_libraries(nosl_actor_check PRIVATE sts2nosl)

# Mod-facing search consumer. This stays outside the ordinary Env C ABI.
add_library(sts2solver SHARED search/combatsolver/mod_api.cpp search/combatsolver/live_schema.cpp)
target_link_libraries(sts2solver PRIVATE sts2combatsolver)
target_include_directories(sts2solver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Dependency-free Python binding surface. The Python package uses ctypes so
# M3 remains buildable on hosts without pybind11 or Python development headers.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/data/content.json
  ${CMAKE_CURRENT_SOURCE_DIR}/engine/sim/state_fields.generated.h)
file(SHA256 ${CMAKE_CURRENT_SOURCE_DIR}/data/content.json STS2SIM_CONTENT_JSON_SHA256)
file(SHA256 ${CMAKE_CURRENT_SOURCE_DIR}/engine/sim/state_fields.generated.h STS2SIM_STATE_SCHEMA_HEADER_SHA256)
add_library(sts2env_c SHARED env/c_api.cpp env/api_descriptor.cpp tools/cli/dump.cpp)
target_compile_definitions(sts2env_c PUBLIC
  STS2SIM_CONTENT_JSON_SHA256="${STS2SIM_CONTENT_JSON_SHA256}"
  STS2SIM_STATE_SCHEMA_HEADER_SHA256="${STS2SIM_STATE_SCHEMA_HEADER_SHA256}")
target_link_libraries(sts2env_c PRIVATE sts2env)
set_target_properties(sts2env_c PROPERTIES OUTPUT_NAME sts2env_c)

# Differential-only injected shop root. It imports through the public ABI of
# this same loaded library; it is never included in the product ABI descriptor.
add_library(env_api_shop_fixture SHARED EXCLUDE_FROM_ALL tests/api/shop_fixture.cpp)
target_include_directories(env_api_shop_fixture PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(env_api_shop_fixture PRIVATE sts2env_c)
add_library(env_api_crystal_sphere_fixture SHARED EXCLUDE_FROM_ALL tests/api/crystal_sphere_fixture.cpp)
target_include_directories(env_api_crystal_sphere_fixture PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(env_api_crystal_sphere_fixture PRIVATE sts2env_c)
add_library(env_api_trial_fixture SHARED tests/api/trial_fixture.cpp)
target_include_directories(env_api_trial_fixture PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(env_api_trial_fixture PRIVATE sts2env_c)
add_library(env_api_capacity_fixture SHARED EXCLUDE_FROM_ALL tests/api/capacity_fixture.cpp)
target_include_directories(env_api_capacity_fixture PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(env_api_capacity_fixture PRIVATE sts2env_c)

add_executable(sts2sim_cli tools/cli/sts2sim_cli.cpp tools/cli/combat_cmd.cpp tools/cli/smoke_cmd.cpp
  tools/cli/replay_cmd.cpp tools/cli/dump.cpp)
target_link_libraries(sts2sim_cli PRIVATE sts2env)
target_compile_options(sts2sim_cli PRIVATE -Wno-unused-parameter)

# Standalone reproducible API performance probe. It is intentionally not a
# CTest gate: timing is machine-dependent, while the state/hash checks inside
# the probe make each reported measurement meaningful.
add_executable(env_api_bench tools/bench/env_api_bench.cpp)
target_link_libraries(env_api_bench PRIVATE sts2env)

add_executable(env_compare_native EXCLUDE_FROM_ALL tools/bench/compare/native.cpp tools/cli/dump.cpp)
target_link_libraries(env_compare_native PRIVATE sts2env)

# Standalone shipped-DLL differential probe; native captures are external.
add_executable(rng_execution_checks tools/rng_execution_checks.cpp)
target_link_libraries(rng_execution_checks PRIVATE sts2core)
add_executable(model_execution_checks tools/model_execution_checks.cpp)
target_link_libraries(model_execution_checks PRIVATE sts2env)
add_executable(numeric_extreme_checks tools/numeric_extreme_checks.cpp)
target_link_libraries(numeric_extreme_checks PRIVATE sts2env)

enable_testing()
add_executable(test_combatsolver_env tests/test_combatsolver_env.cpp)
target_link_libraries(test_combatsolver_env PRIVATE sts2combatsolver)
add_test(NAME combatsolver_env_adapter COMMAND test_combatsolver_env)
add_executable(test_combatsolver_search tests/test_combatsolver_search.cpp)
target_link_libraries(test_combatsolver_search PRIVATE sts2combatsolver)
add_test(NAME combatsolver_search_contract COMMAND test_combatsolver_search)
add_executable(test_combatsolver_mod_api tests/test_combatsolver_mod_api.cpp)
target_link_libraries(test_combatsolver_mod_api PRIVATE sts2solver sts2combatsolver)
add_test(NAME combatsolver_mod_api_contract COMMAND test_combatsolver_mod_api)
add_executable(test_dynamic_storage tests/test_dynamic_storage.cpp)
target_include_directories(test_dynamic_storage PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME dynamic_storage COMMAND test_dynamic_storage)
add_executable(test_api_descriptor tests/test_api_descriptor.cpp)
target_include_directories(test_api_descriptor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_api_descriptor PRIVATE sts2env_c)
add_test(NAME env_api_descriptor COMMAND test_api_descriptor)
add_executable(test_operation tests/test_operation.cpp)
target_link_libraries(test_operation PRIVATE sts2env)
add_test(NAME env_native_operations COMMAND test_operation)
add_executable(env_portable_fixture tests/api/portable_fixture.cpp)
target_include_directories(env_portable_fixture PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(env_portable_fixture PRIVATE sts2env_c)
add_test(NAME env_portable_snapshot COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tests/api/portable_snapshot.py
  --fixture $<TARGET_FILE:env_portable_fixture> --library $<TARGET_FILE:sts2env_c>)
find_program(STS2SIM_DOTNET_EXECUTABLE dotnet HINTS "$ENV{HOME}/.dotnet")
if(STS2SIM_DOTNET_EXECUTABLE)
  add_test(NAME env_dotnet_sdk COMMAND ${STS2SIM_DOTNET_EXECUTABLE} run
    --project ${CMAKE_CURRENT_SOURCE_DIR}/sdk/dotnet/Sts2Env.ContractTests/Sts2Env.ContractTests.csproj
    --configuration Release -- $<TARGET_FILE:sts2env_c>)
endif()
add_executable(test_core tests/test_core.cpp)
target_link_libraries(test_core PRIVATE sts2core)
add_test(NAME core_golden COMMAND test_core ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden)

add_executable(test_content tests/test_content.cpp)
target_link_libraries(test_content PRIVATE sts2content)
add_test(NAME content_tables COMMAND test_content)

add_executable(test_env tests/test_env.cpp)
target_link_libraries(test_env PRIVATE sts2env)
add_test(NAME env_m3 COMMAND test_env)
add_executable(test_snapshot tests/test_snapshot.cpp)
target_link_libraries(test_snapshot PRIVATE sts2env)
add_test(NAME env_snapshot COMMAND test_snapshot)
add_executable(test_audit_capacity_downgrade tests/test_audit_capacity_downgrade.cpp)
target_link_libraries(test_audit_capacity_downgrade PRIVATE sts2env)
add_test(NAME audit_capacity_downgrade COMMAND test_audit_capacity_downgrade)
add_executable(test_audit_interactions tests/test_audit_interactions.cpp)
target_link_libraries(test_audit_interactions PRIVATE sts2env)
add_test(NAME audit_interactions COMMAND test_audit_interactions)
add_executable(test_audit_status_creators tests/test_audit_status_creators.cpp)
target_link_libraries(test_audit_status_creators PRIVATE sts2env)
add_test(NAME audit_status_creators COMMAND test_audit_status_creators)
add_executable(test_audit_bing_bong tests/test_audit_bing_bong.cpp)
target_link_libraries(test_audit_bing_bong PRIVATE sts2env)
add_test(NAME audit_bing_bong COMMAND test_audit_bing_bong)
add_executable(test_audit_free_cost tests/test_audit_free_cost.cpp)
target_link_libraries(test_audit_free_cost PRIVATE sts2env)
add_test(NAME audit_free_cost COMMAND test_audit_free_cost)
add_executable(test_audit_heirloom tests/test_audit_heirloom.cpp)
target_link_libraries(test_audit_heirloom PRIVATE sts2env)
add_test(NAME audit_heirloom COMMAND test_audit_heirloom)
add_executable(test_audit_claws tests/test_audit_claws.cpp)
target_link_libraries(test_audit_claws PRIVATE sts2env)
add_test(NAME audit_claws COMMAND test_audit_claws)
add_executable(test_audit_attack_context tests/test_audit_attack_context.cpp)
target_link_libraries(test_audit_attack_context PRIVATE sts2env)
add_test(NAME audit_attack_context COMMAND test_audit_attack_context)
add_executable(test_combat_gc tests/test_combat_gc.cpp)
target_link_libraries(test_combat_gc PRIVATE sts2env)
add_test(NAME combat_gc COMMAND test_combat_gc)
add_test(NAME differential_protocol COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_differential_tools.py)
add_test(NAME differential_batch COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_differential_batch.py)
add_test(NAME differential_matrix COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_differential_matrix.py)
# Every private field of a live game model has a slot in its port (or a
# stated reason not to); the CombatSolver mod's capture is built from this map.
add_test(NAME live_state_map COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen/check_live_state_map.py)
add_executable(test_preview tests/test_preview.cpp)
target_link_libraries(test_preview PRIVATE sts2env)
add_test(NAME env_preview COMMAND test_preview)
find_package(Threads REQUIRED)
target_link_libraries(sts2combatsolver PUBLIC Threads::Threads)
add_executable(test_rng_execution tests/test_rng_execution.cpp)
target_link_libraries(test_rng_execution PRIVATE sts2env Threads::Threads)
add_test(NAME env_rng_execution COMMAND test_rng_execution)
add_executable(test_run_queries tests/test_run_queries.cpp)
target_link_libraries(test_run_queries PRIVATE sts2env)
add_test(NAME env_run_queries COMMAND test_run_queries)
add_executable(test_act_transition tests/test_act_transition.cpp)
target_link_libraries(test_act_transition PRIVATE sts2env)
add_test(NAME env_act_transition COMMAND test_act_transition)
add_executable(test_spoils_map tests/test_spoils_map.cpp)
target_link_libraries(test_spoils_map PRIVATE sts2env)
add_test(NAME spoils_map_run_content COMMAND test_spoils_map)
add_executable(test_meat_cleaver tests/test_meat_cleaver.cpp)
target_link_libraries(test_meat_cleaver PRIVATE sts2game)
add_test(NAME meat_cleaver_rest_site COMMAND test_meat_cleaver)
add_executable(test_card_removal_reward tests/test_card_removal_reward.cpp)
target_link_libraries(test_card_removal_reward PRIVATE sts2env)
add_test(NAME card_removal_reward COMMAND test_card_removal_reward)
add_executable(test_architect_event tests/test_architect_event.cpp)
target_link_libraries(test_architect_event PRIVATE sts2env)
add_test(NAME env_architect_event COMMAND test_architect_event)
add_executable(test_generated_card_add tests/test_generated_card_add.cpp)
target_link_libraries(test_generated_card_add PRIVATE sts2game)
add_test(NAME generated_card_add COMMAND test_generated_card_add)
add_executable(test_search tests/test_search.cpp)
target_link_libraries(test_search PRIVATE sts2search)
add_test(NAME search_m3 COMMAND test_search)
add_executable(test_potions tests/test_potions.cpp)
target_link_libraries(test_potions PRIVATE sts2game)
add_test(NAME potions_common_m4 COMMAND test_potions)
add_executable(test_entropic_brew tests/test_entropic_brew.cpp)
target_link_libraries(test_entropic_brew PRIVATE sts2game)
add_test(NAME entropic_brew_procure_failure COMMAND test_entropic_brew)
set_tests_properties(entropic_brew_procure_failure PROPERTIES TIMEOUT 5)
add_executable(test_shared_models tests/test_shared_models.cpp)
target_link_libraries(test_shared_models PRIVATE sts2env)
add_test(NAME shared_cards_potions_v0110 COMMAND test_shared_models)
add_executable(test_relics tests/test_relics.cpp)
target_link_libraries(test_relics PRIVATE sts2game)
add_test(NAME relics_common_m4 COMMAND test_relics)
add_executable(test_card_reward_options tests/test_card_reward_options.cpp)
target_link_libraries(test_card_reward_options PRIVATE sts2game)
add_test(NAME card_reward_options COMMAND test_card_reward_options)

add_executable(test_seed_rule_primitives tests/test_seed_rule_primitives.cpp)
target_link_libraries(test_seed_rule_primitives PRIVATE sts2env)
add_test(NAME seed_rule_primitives COMMAND test_seed_rule_primitives)

add_executable(test_power_card_capture tests/test_power_card_capture.cpp)
target_include_directories(test_power_card_capture PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_power_card_capture PRIVATE sts2env_c)
add_test(NAME power_card_capture COMMAND test_power_card_capture)
add_executable(test_paels_legion tests/test_paels_legion.cpp)
target_link_libraries(test_paels_legion PRIVATE sts2env)
add_test(NAME paels_legion COMMAND test_paels_legion)
add_executable(test_paels_growth tests/test_paels_growth.cpp)
target_link_libraries(test_paels_growth PRIVATE sts2env)
add_test(NAME paels_growth COMMAND test_paels_growth)
add_executable(test_paels_eye tests/test_paels_eye.cpp)
target_link_libraries(test_paels_eye PRIVATE sts2env)
add_test(NAME paels_eye COMMAND test_paels_eye)
add_executable(test_paels_tooth tests/test_paels_tooth.cpp)
target_link_libraries(test_paels_tooth PRIVATE sts2env)
add_test(NAME paels_tooth COMMAND test_paels_tooth)
add_executable(test_warrior_act_relics tests/test_warrior_act_relics.cpp)
target_link_libraries(test_warrior_act_relics PRIVATE sts2env)
add_test(NAME warrior_act_relics COMMAND test_warrior_act_relics)
add_executable(test_colorless tests/test_colorless.cpp)
target_link_libraries(test_colorless PRIVATE sts2game)
add_test(NAME colorless_m4 COMMAND test_colorless)
add_executable(test_silent_pools tests/test_silent_pools.cpp)
target_link_libraries(test_silent_pools PRIVATE sts2game)
add_test(NAME silent_source_pools COMMAND test_silent_pools)
add_executable(test_necrobinder tests/test_necrobinder.cpp)
target_link_libraries(test_necrobinder PRIVATE sts2env)
add_test(NAME necrobinder_v0110 COMMAND test_necrobinder)
add_executable(test_silent_play_capture tests/test_silent_play_capture.cpp)
target_link_libraries(test_silent_play_capture PRIVATE sts2env)
add_test(NAME silent_play_capture_restore COMMAND test_silent_play_capture)
add_executable(test_vakuu tests/test_vakuu.cpp)
target_link_libraries(test_vakuu PRIVATE sts2game)
add_test(NAME vakuu_event COMMAND test_vakuu)
add_executable(test_run_event_batch tests/test_run_event_batch.cpp)
target_link_libraries(test_run_event_batch PRIVATE sts2game)
add_test(NAME run_event_batch COMMAND test_run_event_batch)
add_executable(test_war_historian tests/test_war_historian.cpp)
target_link_libraries(test_war_historian PRIVATE sts2game)
add_test(NAME war_historian_event COMMAND test_war_historian)
add_executable(test_history_course tests/test_history_course.cpp)
target_link_libraries(test_history_course PRIVATE sts2env)
add_test(NAME history_course_relic COMMAND test_history_course)
add_executable(test_trial_event tests/test_trial_event.cpp)
target_link_libraries(test_trial_event PRIVATE sts2game)
add_test(NAME trial_event COMMAND test_trial_event)
add_executable(test_fake_merchant tests/test_fake_merchant.cpp)
target_link_libraries(test_fake_merchant PRIVATE sts2game)
add_test(NAME fake_merchant_event COMMAND test_fake_merchant)
add_executable(test_orobas_enchanted tests/test_orobas_enchanted.cpp)
target_link_libraries(test_orobas_enchanted PRIVATE sts2game)
add_test(NAME orobas_enchanted_starter COMMAND test_orobas_enchanted)
add_executable(test_battleworn_timeout tests/test_battleworn_timeout.cpp)
target_link_libraries(test_battleworn_timeout PRIVATE sts2env)
add_test(NAME battleworn_timeout COMMAND test_battleworn_timeout)
add_test(NAME env_python COMMAND ${CMAKE_COMMAND} -E env
  STS2SIM_ENV_LIB=$<TARGET_FILE:sts2env_c>
  python3 ${CMAKE_CURRENT_SOURCE_DIR}/python/test_env.py)
