// Models/RelicModel.cs
#pragma once

#include <cstdint>

#include "content/generated/tables.h"
#include "game/Localization/DynamicVars/DynamicVarSet.h"
#include "game/Models/AbstractModel.h"

namespace sts2 {

class RelicModel : public AbstractModel {
public:
    RelicModel() = default;
    RelicModel(Sim* sim, RelicRef index) : AbstractModel(sim, ModelKind::Relic, index) {}

    RelicState& st() const { return sim_->g->relics[index_]; }
    RelicId RelicId_() const { return st().id; }
    const RelicData& data() const { return sts2::data(st().id); }

    Player Owner() const;
    bool HasBeenRemovedFromState() const { return st().removed; }
    bool IsMelted() const { return st().is_melted; }
    bool IsWax() const { return st().is_wax; }
    RelicStatus Status() const { return st().status; }
    void SetStatus(RelicStatus s) const { st().status = s; }
    int32_t FloorAddedToDeck() const { return st().floor_added; }
    DynamicVarSet DynamicVars() const { return DynamicVarSet(*this); }
    sts2::RelicRarity Rarity() const { return data().rarity; }  // a constant per relic (RelicData)
    void Flash() const {}  // UI
    // IsBeforeAct3TreasureChest: TotalFloor below the act-3 treasure chest's floor.
    static bool IsBeforeAct3TreasureChest(RunState runState);

    // --- overridable members (RelicModel.def)
#define SIM_MEMBER(Base, Ret, Name, Params, Args, Default) Ret Name Params const;
#define SIM_MEMBER_CUSTOM(Base, Ret, Name, Params, Args, Default) \
    Ret Name Params const;                                          \
    Ret Name##_Default Params const;
#include "game/Models/RelicModel.def"
#undef SIM_MEMBER
#undef SIM_MEMBER_CUSTOM

protected:
    int32_t& extra(int i) const { return st().extra[i]; }
};

}  // namespace sts2
