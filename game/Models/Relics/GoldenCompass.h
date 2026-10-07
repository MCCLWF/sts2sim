// Models/Relics/GoldenCompass.cs
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2::RunManager {
void GenerateMap(Sim* sim);
}

namespace sts2 {

struct GoldenCompass : RelicModel {
    SIM_MODEL(GoldenCompass, RelicModel)

    bool HasUponPickupEffect() const { return true; }
    int32_t GoldenPathAct() const { return extra(0) - 1; }

    void AfterObtained() const {
        extra(0) = Owner().RunState().CurrentActIndex() + 1;
        RunManager::GenerateMap(&sim());
    }

    ActMap ModifyGeneratedMap(RunState, ActMap map, int32_t actIndex) const {
        return GoldenPathAct() == actIndex ? generate_golden_path_map() : map;
    }

    RoomTypeSet ModifyUnknownMapPointRoomTypes(RoomTypeSet roomTypes) const {
        if (GoldenPathAct() != Owner().RunState().CurrentActIndex()) return roomTypes;
        RoomTypeSet eventOnly;
        eventOnly.Add(RoomType::Event);
        return eventOnly;
    }
};

}  // namespace sts2
