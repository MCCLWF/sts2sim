// Models/Afflictions/Tainted.cs
#pragma once

#include "game/Models/AfflictionModel.h"

namespace sts2 {

struct Tainted : AfflictionModel {
    SIM_MODEL(Tainted, AfflictionModel)

    bool HasExtraCardText() const { return true; }
    bool IsStackable() const { return true; }
    bool CanAfflictCardType(CardType type) const { return type == CardType::Skill; }
};

}  // namespace sts2
