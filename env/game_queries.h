// Privileged, read-only gameplay queries for an ordinary Env combat/choice state.
// These are shipped game getters; consumers implement their own policies.
#pragma once

#include <string>

#include "env/env.h"

namespace sts2 {

// Lazy getter writes are isolated to a private clone. Unsupported models or
// side effects other than BaseStarCost's initialization throw Unported.
std::string ExportCombatQueries(const Env& source);

// Shipped calculated values and raw formula operands. Caller policies choose
// how to use the operands. Source/game state/RNG/history are unchanged.
std::string ExportCombatCalculatedVars(const Env& source);

// Raw orb queue and shipped getter values. Getter hooks run on a private
// clone and must remain pure. Caller policies are outside this interface.
std::string ExportCombatOrbs(const Env& source);
// Raw game power fields in hook-listener order. Search field selection,
// fingerprint encoding and root lifecycle ledgers belong to the caller.
std::string ExportCombatPowers(const Env& source);

}  // namespace sts2
