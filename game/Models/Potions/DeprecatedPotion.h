#pragma once

#include "game/Models/PotionModel.h"

namespace sts2 {

// Legacy history/save placeholder: the original class has no OnUse override.
struct DeprecatedPotion : PotionModel {
    SIM_MODEL(DeprecatedPotion, PotionModel)
};

}  // namespace sts2
