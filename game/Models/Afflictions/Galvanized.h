#pragma once

#include "game/Models/AfflictionModel.h"

namespace sts2 {

struct Galvanized : AfflictionModel {
    SIM_MODEL(Galvanized, AfflictionModel)

    bool HasExtraCardText() const { return true; }
    bool IsStackable() const { return true; }
};

}  // namespace sts2
