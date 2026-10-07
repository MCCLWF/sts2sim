// Baseline policies for the Silent A10 NoSL work (docs/plans/2026-10-03-silent-a10-nosl.md).
//
// These are Env consumers. Each policy declares what it reads and whether it
// simulates: only `PublicNoSearch` policies may be reported as NoSL results.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "env/env.h"
#include "search/combatsolver/solver.h"

namespace sts2::nosl {

enum class PolicyClass : uint8_t {
    PublicNoSearch,     // public observation only, no simulation
    SimulatedLookahead, // steps copies of the true state: diagnostic only
    PrivilegedTeacher,  // searches the true future: diagnostic / teacher only
};
const char* PolicyClassName(PolicyClass c);

// One solver call made by the teacher policy.
struct TeacherSolve {
    double ms = 0;
    int64_t nodes = 0;
    int32_t route_actions = 0;
    bool won = false;
    std::string stop_reason;
};

struct PolicyStats {
    std::vector<TeacherSolve> solves;
    int32_t route_illegal = 0;     // a route action was not legal when its turn came
    int32_t teacher_fallback = 0;  // decisions answered by the fallback after a solver failure
    std::map<std::string, int32_t> teacher_errors;
};

class Policy {
public:
    virtual ~Policy() = default;
    virtual PolicyClass Class() const = 0;
    // A new run starts: drop everything remembered from the previous one.
    virtual void Reset(uint64_t policy_seed) = 0;
    // `legal` is env.LegalActions() at the current decision and is not empty.
    virtual Action Choose(Env& env, const std::vector<Action>& legal) = 0;
    PolicyStats stats;
};

struct TeacherOptions {
    combatsolver::CombatSearchConfig config;
};

// Names: random, rule0, lookahead, teacher.
std::unique_ptr<Policy> MakePolicy(const std::string& name, const TeacherOptions& teacher);

}  // namespace sts2::nosl
