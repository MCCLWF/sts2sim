#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/sim/state.h"
#include "env/env.h"
#include "env/preview.h"

using namespace sts2;

namespace {
constexpr size_t kWarmup = 32;
constexpr size_t kSamples = 9;
constexpr size_t kIterations = 1000;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Measurement {
    double mean_ns = 0;
    double median_ns = 0;
    uint64_t digest = 0;
};

template <class Operation>
Measurement measure(Operation&& operation) {
    for (size_t i = 0; i < kWarmup; ++i) (void)operation();

    std::vector<double> ns_per_operation;
    ns_per_operation.reserve(kSamples);
    uint64_t digest = 0;
    for (size_t sample = 0; sample < kSamples; ++sample) {
        uint64_t sample_digest = 0;
        const auto begin = std::chrono::steady_clock::now();
        for (size_t i = 0; i < kIterations; ++i)
            sample_digest += operation();
        const auto end = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
        ns_per_operation.push_back(static_cast<double>(elapsed) / static_cast<double>(kIterations));
        digest ^= sample_digest + static_cast<uint64_t>(sample);
    }

    const double total = std::accumulate(ns_per_operation.begin(), ns_per_operation.end(), 0.0);
    std::sort(ns_per_operation.begin(), ns_per_operation.end());
    return {total / static_cast<double>(kSamples), ns_per_operation[kSamples / 2], digest};
}

void print(const char* operation, const Measurement& m) {
    std::cout << operation << "," << kIterations << "," << kSamples << ","
              << std::fixed << std::setprecision(2) << m.mean_ns << "," << m.median_ns
              << "," << m.digest << '\n';
}
}  // namespace

int main() {
    try {
        Env root;
        root.ResetCombat(NewRunOptions{.seed = "DUAL001", .character = CharacterId::IRONCLAD, .ascension = 0},
                         EncounterId::CULTISTS_NORMAL);
        require(root.decision().kind == DecisionKind::Combat, "fixed root is not a combat decision");
        const Action end_turn{.kind = Action::EndTurn};
        const auto legal = root.LegalActions();
        require(std::find(legal.begin(), legal.end(), end_turn) != legal.end(),
                "EndTurn is not legal at the fixed root");

        const uint64_t root_hash = root.Hash();
        uint64_t ordinary_successor_hash = 0;
        size_t trace_on_frames = 0;
        size_t trace_on_events = 0;
        size_t trace_on_power_events = 0;
        {
            const auto ordinary = root.Predict(end_turn);
            ordinary_successor_hash = ordinary->Hash();
            const auto traced = PreviewAction(root, end_turn, {.collect_trace = true});
            const auto untraced = PreviewAction(root, end_turn, {.collect_trace = false});
            require(traced.successor->Hash() == ordinary_successor_hash,
                    "trace-enabled Preview successor differs from Predict");
            require(untraced.successor->Hash() == ordinary_successor_hash,
                    "trace-disabled Preview successor differs from Predict");
            trace_on_frames = traced.trace.frames.size();
            trace_on_events = traced.trace.events.size();
            trace_on_power_events = traced.trace.power_events.size();
        }
        require(root.Hash() == root_hash, "verification changed the source root");

        std::cout << "compiler=" << __VERSION__ << '\n'
                  << "warmup_per_operation=" << kWarmup << '\n'
                  << "samples=" << kSamples << '\n'
                  << "iterations_per_sample=" << kIterations << '\n'
                  << "sizeof_GameState_bytes=" << sizeof(GameState) << '\n'
                  << "root_hash=" << root_hash << '\n'
                  << "ordinary_successor_hash=" << ordinary_successor_hash << '\n'
                  << "trace_on_successor_hash=" << ordinary_successor_hash << '\n'
                  << "trace_off_successor_hash=" << ordinary_successor_hash << '\n'
                  << "trace_on_frames=" << trace_on_frames << '\n'
                  << "trace_on_events=" << trace_on_events << '\n'
                  << "trace_on_power_events=" << trace_on_power_events << '\n'
                  << "root_hash_stable_after_verification=true\n"
                  << "operation,iterations_per_sample,samples,mean_ns_per_operation,median_ns_per_operation,digest\n";

        print("Clone_including_destruction", measure([&] {
            uint64_t value = 0;
            {
                auto copy = root.Clone();
                value = static_cast<uint64_t>(copy->state().run.total_floor);
            }
            return value;
        }));

        print("Hash", measure([&] { return root.Hash(); }));

        print("Predict_EndTurn_including_destruction", measure([&] {
            uint64_t value = 0;
            {
                auto successor = root.Predict(end_turn);
                value = static_cast<uint64_t>(successor->state().run.phase)
                      ^ static_cast<uint64_t>(static_cast<uint32_t>(successor->state().player_combat.energy));
            }
            return value;
        }));

        print("Preview_trace_on_including_destruction", measure([&] {
            uint64_t value = 0;
            {
                auto result = PreviewAction(root, end_turn, {.collect_trace = true});
                value = static_cast<uint64_t>(result.successor->state().run.phase)
                      ^ static_cast<uint64_t>(static_cast<uint32_t>(result.successor->state().player_combat.energy))
                      ^ static_cast<uint64_t>(result.trace.events.size());
            }
            return value;
        }));

        print("Preview_trace_off_including_destruction", measure([&] {
            uint64_t value = 0;
            {
                auto result = PreviewAction(root, end_turn, {.collect_trace = false});
                value = static_cast<uint64_t>(result.successor->state().run.phase)
                      ^ static_cast<uint64_t>(static_cast<uint32_t>(result.successor->state().player_combat.energy))
                      ^ static_cast<uint64_t>(result.trace.events.size());
            }
            return value;
        }));

        print("ExportSnapshotJson_including_string_destruction", measure([&] {
            uint64_t value = 0;
            {
                std::string snapshot = root.ExportSnapshotJson();
                value = static_cast<uint64_t>(snapshot.size());
            }
            return value;
        }));

        require(root.Hash() == root_hash, "benchmark mutated the source root");
        std::cout << "root_hash_stable_after_benchmark=true\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "env_api_bench: " << e.what() << '\n';
        return 1;
    }
}
