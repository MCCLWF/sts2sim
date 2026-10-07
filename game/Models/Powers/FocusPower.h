// Models/Powers/FocusPower.cs
#pragma once
#include "game/Models/OrbModel.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct FocusPower : PowerModel { SIM_MODEL(FocusPower, PowerModel)
    Dec ModifyOrbValue(OrbModel orb, Dec value) const {
        if (Owner().Player() != orb.Owner()) return value;
        Dec modified = value + Dec(Amount());
        return modified < Dec(0) ? Dec(0) : modified;
    }
};
}
