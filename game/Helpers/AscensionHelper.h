// Helpers/AscensionHelper.cs. RunManager.Instance.HasAscension reads the
// current run; here the run is the Sim's.
#pragma once

#include <cstdint>

#include "engine/sim/sim.h"

namespace sts2::AscensionHelper {

inline bool HasAscension(const Sim& sim, AscensionLevel level) { return sim.g->run.has_ascension(level); }

inline int32_t GetValueIfAscension(const Sim& sim, AscensionLevel level, int32_t ascensionValue, int32_t fallbackValue) {
    return HasAscension(sim, level) ? ascensionValue : fallbackValue;
}
inline float GetValueIfAscension(const Sim& sim, AscensionLevel level, float ascensionValue, float fallbackValue) {
    return HasAscension(sim, level) ? ascensionValue : fallbackValue;
}

}  // namespace sts2::AscensionHelper
