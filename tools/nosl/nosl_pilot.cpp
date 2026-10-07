// nosl_pilot: the pilot (nosl/pilot.h) driven to the end of a run without a
// game, one JSON line per action. It is what the game-side executor
// (tools/NoslPilot) is expected to reproduce for the same seed, weights and
// policy seed, and a check that the pilot's ABI works.
//
//   nosl_pilot --seed S --weights W.bin [--ascension 10] [--filter 1] [--policy-seed 1] [--greedy 0]
//              [--out FILE.jsonl] [--max-steps N]
//
// The summary on stdout: {"seed","steps","end":"victory"|"death"|"running","floor","policy_decisions","ms_median","ms_max"}.
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "engine/json.h"
#include "nosl/pilot.h"

using namespace sts2;

extern "C" {
void* nosl_pilot_create(const char* seed, int32_t ascension, int32_t actor_filter, const char* weights, uint64_t policy_seed, int32_t greedy);
const char* nosl_pilot_next(void* pilot);
const char* nosl_pilot_last_error();
void nosl_pilot_destroy(void* pilot);
}

int main(int argc, char** argv) {
    std::map<std::string, std::string> opt;
    for (int i = 1; i + 1 < argc; i += 2) opt[std::string(argv[i]).substr(2)] = argv[i + 1];
    if (!opt.count("seed") || !opt.count("weights")) { std::cerr << "nosl_pilot: --seed and --weights are required\n"; return 2; }
    auto number = [&](const char* key, long long fallback) { return opt.count(key) ? std::stoll(opt[key]) : fallback; };
    void* pilot = nosl_pilot_create(opt["seed"].c_str(), static_cast<int32_t>(number("ascension", 10)),
                                    static_cast<int32_t>(number("filter", 1)), opt["weights"].c_str(),
                                    static_cast<uint64_t>(number("policy-seed", 1)), static_cast<int32_t>(number("greedy", 0)));
    if (!pilot) { std::cerr << "nosl_pilot: " << nosl_pilot_last_error() << '\n'; return 1; }
    std::ofstream out;
    if (opt.count("out")) out.open(opt["out"]);
    const long long max_steps = number("max-steps", 100000);
    long long steps = 0, decisions = 0;
    std::vector<double> ms;
    std::string end = "running", last;
    for (; steps < max_steps; ++steps) {
        const char* line = nosl_pilot_next(pilot);
        if (!line) { std::cerr << "nosl_pilot: " << nosl_pilot_last_error() << '\n'; nosl_pilot_destroy(pilot); return 1; }
        last = line;
        if (out.is_open()) out << last << '\n';
        if (last.find("\"decision\":\"Ended\"") != std::string::npos) {
            end = last.find("\"victory\":true") != std::string::npos ? "victory" : "death";
            break;
        }
        if (last.find("\"forced\":false") != std::string::npos) {
            ++decisions;
            const size_t at = last.rfind("\"ms\":");
            if (at != std::string::npos) ms.push_back(std::stod(last.substr(at + 5)));
        }
    }
    int32_t floor = -1;  // from the last line: "floor" of the end line, else the f<n> of the digest
    if (size_t at = last.find("\"floor\":"); at != std::string::npos) floor = std::stoi(last.substr(at + 8));
    else if (size_t at2 = last.find(" f"); at2 != std::string::npos) floor = std::stoi(last.substr(at2 + 2));
    nosl_pilot_destroy(pilot);
    std::sort(ms.begin(), ms.end());
    JsonWriter w;
    w.begin_object().field("seed", opt["seed"]).field("steps", static_cast<int64_t>(steps)).field("end", end).field("floor", floor)
        .field("policy_decisions", static_cast<int64_t>(decisions))
        .field("ms_median", ms.empty() ? 0.0 : ms[ms.size() / 2]).field("ms_max", ms.empty() ? 0.0 : ms.back()).end_object();
    std::cout << w.str() << std::endl;
    return 0;
}
