#pragma once

#include <cstdint>
#include <limits>

#include "engine/sim/errors.h"
#include "env/c_api.h"
#include "env/env.h"

namespace sts2 {

// Convert a C ABI action without allowing invalid values to alias valid ones
// through enum or coordinate narrowing. Call this before dispatching any C
// action to Env::Step, Env::Predict, or PreviewAction.
inline Action action_from_c_checked(sts2_action value) {
    if (value.kind < static_cast<int32_t>(Action::StartRun) ||
        value.kind > static_cast<int32_t>(Action::Resume)) {
        throw InvalidRequest("C API action kind is out of range");
    }

    constexpr int32_t min_coord = std::numeric_limits<int8_t>::min();
    constexpr int32_t max_coord = std::numeric_limits<int8_t>::max();
    if (value.col < min_coord || value.col > max_coord ||
        value.row < min_coord || value.row > max_coord) {
        throw InvalidRequest("C API action coordinate is out of int8 range");
    }

    Action result;
    result.kind = static_cast<Action::Kind>(value.kind);
    result.index = value.index;
    result.target = value.target;
    result.coord = MapCoord{static_cast<int8_t>(value.col), static_cast<int8_t>(value.row)};
    return result;
}

}  // namespace sts2
