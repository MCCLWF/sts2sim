#pragma once

#include "game/Models/AfflictionModel.h"

namespace sts2 {

struct Ringing : AfflictionModel {
    SIM_MODEL(Ringing, AfflictionModel)

    bool HasExtraCardText() const { return true; }
};

}  // namespace sts2
