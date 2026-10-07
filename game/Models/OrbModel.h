// Models/OrbModel.cs. A handle into the combat's fixed orb pool.
#pragma once

#include "game/Models/AbstractModel.h"

namespace sts2 {

class OrbModel : public AbstractModel {
public:
    OrbModel() = default;
    OrbModel(Sim* sim, OrbRef index) : AbstractModel(sim, ModelKind::Orb, index) {}

    OrbState& st() const { return sim_->g->orbs[index_]; }
    OrbId OrbId_() const { return st().id; }
    Player Owner() const;
    sts2::CombatState CombatState() const;
    bool HasBeenRemovedFromState() const { return st().removed; }
    void RemoveInternal() const { st().removed = true; }
    void TriggerPassive(PlayerChoiceContext& choiceContext, Creature target) const;
    Dec ModifyOrbValue(Dec value) const;

#define SIM_MEMBER(Base, Ret, Name, Params, Args, Default) Ret Name Params const;
#define SIM_MEMBER_CUSTOM(Base, Ret, Name, Params, Args, Default) \
    Ret Name Params const; Ret Name##_Default Params const;
#include "game/Models/OrbModel.def"
#undef SIM_MEMBER
#undef SIM_MEMBER_CUSTOM
};

}  // namespace sts2
