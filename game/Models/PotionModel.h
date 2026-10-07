// Models/PotionModel.cs
#pragma once

#include <cstdint>

#include "content/generated/tables.h"
#include "game/Localization/DynamicVars/DynamicVarSet.h"
#include "game/Models/AbstractModel.h"

namespace sts2 {

class PotionModel : public AbstractModel {
public:
    PotionModel() = default;
    PotionModel(Sim* sim, PotionRef index) : AbstractModel(sim, ModelKind::Potion, index) {}

    PotionState& st() const { return sim_->g->potions[index_]; }
    PotionId PotionId_() const { return st().id; }
    const PotionData& data() const { return sts2::data(st().id); }

    Player Owner() const;
    bool HasBeenRemovedFromState() const { return st().removed; }
    DynamicVarSet DynamicVars() const { return DynamicVarSet(*this); }
    sts2::PotionRarity Rarity() const { return data().rarity; }
    sts2::PotionUsage Usage() const { return data().usage; }
    // A constant per potion (PotionData), except FoulPotion's.
    sts2::TargetType TargetType() const;
    void OnUseWrapper(PlayerChoiceContext& choiceContext, Creature target) const;

    // --- overridable members (PotionModel.def)
#define SIM_MEMBER(Base, Ret, Name, Params, Args, Default) Ret Name Params const;
#define SIM_MEMBER_CUSTOM(Base, Ret, Name, Params, Args, Default) \
    Ret Name Params const;                                          \
    Ret Name##_Default Params const;
#include "game/Models/PotionModel.def"
#undef SIM_MEMBER
#undef SIM_MEMBER_CUSTOM
};

}  // namespace sts2
