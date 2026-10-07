// Models/Afflictions/Bound.cs
#pragma once
#include "game/Models/AfflictionModel.h"
namespace sts2 {
struct Bound : AfflictionModel {
    SIM_MODEL(Bound, AfflictionModel)
    bool HasExtraCardText() const { return true; }
};
}  // namespace sts2
