// Entities/Creatures/DamageResult.cs
#pragma once

#include <cstdint>
#include "core/csharp_int.h"

#include "game/Entities/Creatures/Creature.h"

namespace sts2 {

struct DamageResult {
    Creature Receiver;
    ValueProp Props{};
    int32_t BlockedDamage = 0;
    int32_t UnblockedDamage = 0;
    int32_t OverkillDamage = 0;
    bool WasBlockBroken = false;
    bool WasFullyBlocked = false;
    bool WasTargetKilled = false;

    DamageResult() = default;
    DamageResult(Creature receiver, ValueProp props) : Receiver(receiver), Props(props) {}
    int32_t TotalDamage() const { return BlockedDamage + UnblockedDamage; }
};

// LINQ Sum<int> checks each accumulation; the selector's int addition is
// still unchecked. Fisticuffs and BlightStrike use this exact distinction.
template<class Hits>
int32_t SumDamageResults(const Hits& hits, bool include_overkill = false) {
    int32_t total = 0;
    for (const auto& hit : hits) for (const DamageResult& result : hit)
        total = checked_int_add(total, include_overkill
            ? int_add(result.TotalDamage(), result.OverkillDamage) : result.TotalDamage());
    return total;
}

}  // namespace sts2
