// Models/Cards/ByrdonisEgg.cs. Its rest-site option is handled by RestSite::Generate.
#pragma once

#include "game/Models/CardModel.h"

namespace sts2 {

struct ByrdonisEgg : CardModel {
    SIM_MODEL(ByrdonisEgg, CardModel)
    int32_t MaxUpgradeLevel() const { return 0; }
};

}  // namespace sts2
