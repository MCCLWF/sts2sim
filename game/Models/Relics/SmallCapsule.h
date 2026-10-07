// Models/Relics/SmallCapsule.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Models/Relics/NeowRelicHelpers.h"
namespace sts2 {
struct SmallCapsule : RelicModel {
    SIM_MODEL(SmallCapsule, RelicModel)
    void AfterObtained() const { NeowRelicHelpers::OfferRelics(Owner(), 1); }
};
}
