// The guard for hooks whose C# signatures use types not ported yet (their
// Hook.* is not generated): no hook listener of the run may override them.
#pragma once

#include "game/All.h"

namespace sts2 {

inline void RequireNoRunOverride(Sim* sim, Member member, const char* name) {
    RunState state(sim);
    for (AbstractModel m : state.IterateHookListeners(CombatState{})) {
        if (state.Contains(m) && m.Overrides(member)) throw_unported(m, name);
    }
}

}  // namespace sts2
