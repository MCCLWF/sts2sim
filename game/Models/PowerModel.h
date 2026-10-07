// Models/PowerModel.cs
#pragma once

#include <cstdint>

#include "content/generated/tables.h"
#include "game/Localization/DynamicVars/DynamicVarSet.h"
#include "game/Models/AbstractModel.h"

namespace sts2 {

class PowerModel : public AbstractModel {
public:
    PowerModel() = default;
    PowerModel(Sim* sim, PowerRef index) : AbstractModel(sim, ModelKind::Power, index) {}

    PowerState& st() const { return sim_->g->powers[index_]; }
    PowerId PowerId_() const { return st().id; }
    // ITemporaryPower.InternallyAppliedPower: the concrete model type used by
    // temporary stat powers to apply their reversible Strength/Dexterity/Focus.
    PowerId InternallyAppliedPowerId() const {
        switch (PowerId_()) {
            case PowerId::ANTICIPATE_POWER:
            case PowerId::FADE_POWER:
            case PowerId::HELICAL_DART_POWER:
            case PowerId::SPEED_POTION_POWER:
                return PowerId::DEXTERITY_POWER;
            case PowerId::COORDINATE_POWER:
            case PowerId::CRUSH_UNDER_POWER:
            case PowerId::DARK_SHACKLES_POWER:
            case PowerId::DYING_STAR_POWER:
            case PowerId::ENFEEBLING_TOUCH_POWER:
            case PowerId::FEEDING_FRENZY_POWER:
            case PowerId::FLEX_POTION_POWER:
            case PowerId::MANGLE_POWER:
            case PowerId::MONARCHS_GAZE_STRENGTH_DOWN_POWER:
            case PowerId::PIERCING_WAIL_POWER:
            case PowerId::REPTILE_TRINKET_POWER:
            case PowerId::SETUP_STRIKE_POWER:
            case PowerId::SHACKLING_POTION_POWER:
                return PowerId::STRENGTH_POWER;
            case PowerId::FOCUSED_STRIKE_POWER:
            case PowerId::HOTFIX_POWER:
            case PowerId::HYPERBEAM_FOCUS_DOWN_POWER:
            case PowerId::SYNCHRONIZE_POWER:
                return PowerId::FOCUS_POWER;
            default:
                return PowerId::NONE;
        }
    }
    bool IsTemporaryPower() const { return InternallyAppliedPowerId() != PowerId::NONE; }
    const PowerData& data() const { return sts2::data(st().id); }

    int32_t Amount() const { return st().amount; }
    int32_t AmountOnTurnStart() const { return st().amount_on_turn_start; }
    void SetAmountOnTurnStart(int32_t v) const { st().amount_on_turn_start = v; }
    bool SkipNextDurationTick() const { return st().skip_next_duration_tick; }
    void SetSkipNextDurationTick(bool v) const { st().skip_next_duration_tick = v; }
    PowerType TypeForCurrentAmount() const { return GetTypeForAmount(Dec(Amount())); }
    PowerType GetTypeForAmount(Dec customAmount) const;
    // IsVisible: Target is null, the local player or an enemy in single player,
    // so it is IsVisibleInternal (false only for AmbergrisPower).
    bool IsVisible() const { return PowerId_() != PowerId::AMBERGRIS_POWER; }
    bool ShouldRemoveDueToAmount() const;
    sts2::Creature Owner() const;
    sts2::Creature Applier() const;
    void SetApplier(sts2::Creature c) const;
    sts2::Creature Target() const;
    void SetTarget(sts2::Creature c) const;
    sts2::CombatState CombatState() const;
    DynamicVarSet DynamicVars() const { return DynamicVarSet(*this); }

    void Flash() const {}  // UI
    void SetAmount(int32_t amount, bool silent = false) const;
    void ApplyInternal(sts2::Creature owner, Dec amount, bool silent = false) const;
    void RemoveInternal() const;

    // Properties every power defines (Type, StackType, InstanceType,
    // AllowNegative, ShouldScaleInMultiplayer) come from the tables. The few
    // powers that compute Type or StackType are handled in PowerModel.cpp.
    PowerType Type() const;
    PowerStackType StackType() const;
    PowerInstanceType InstanceType() const { return data().instance_type; }
    bool AllowNegative() const { return data().allow_negative; }
    bool ShouldScaleInMultiplayer() const { return data().scale_in_multiplayer; }

    // --- overridable members (PowerModel.def)
#define SIM_MEMBER(Base, Ret, Name, Params, Args, Default) Ret Name Params const;
#define SIM_MEMBER_CUSTOM(Base, Ret, Name, Params, Args, Default) \
    Ret Name Params const;                                          \
    Ret Name##_Default Params const;
#include "game/Models/PowerModel.def"
#undef SIM_MEMBER
#undef SIM_MEMBER_CUSTOM

protected:
    int32_t& extra(int i) const { return st().extra[i]; }
};

}  // namespace sts2
