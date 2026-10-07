// Models/AbstractModel.cs
//
// The handle of a model: a kind plus instance storage, with an explicit
// immutable definition identity for ModelDb templates. Concrete models
// (StrikeIronclad, VulnerablePower, ...) derive from the kind's base handle
// and define the members their C# class overrides. Calls to overridable
// members go through generated dispatch (game/generated/Dispatch.cpp, made by
// tools/gen/gen_dispatch.py from the *.def member tables).
#pragma once

#include <cstdint>

#include "content/generated/ids.h"
#include "content/generated/members.h"
#include "core/dec.h"
#include "engine/sim/sim.h"
#include "game/fwd.h"
#include "game/Models/ModelId.h"
#include "game/Models/signature_types.h"

namespace sts2 {

// Opens the body of a concrete model (game/Models/<Kind>s/<Name>.h): the base
// handle's constructors, plus the conversion from a base handle that the
// dispatch and the SIM_MOVE / SIM_COND callbacks use.
#define SIM_MODEL(Cls, Base) \
    using Base::Base;        \
    explicit Cls(Base m) : Base(m) {}

// Inside a concrete model: a member the C# class overrides that is not ported
// yet. Calling it throws Unported (tools/gen/gen_dispatch.py). Lets a class be
// ported member by member, e.g. only IsAllowed of a relic.
#define SIM_NOT_PORTED(Member)

class AbstractModel {
public:
    AbstractModel() = default;
    AbstractModel(Sim* sim, ModelKind kind, uint32_t index) : sim_(sim), kind_(kind), index_(index) {}

    explicit operator bool() const { return index_ != 0; }
    bool IsNull() const { return index_ == 0; }
    friend bool operator==(const AbstractModel& a, const AbstractModel& b) {
        if(a.kind_!=b.kind_)return false;
        if(a.IsCanonical() || b.IsCanonical())return a.canonical_id_!=0 && a.canonical_id_==b.canonical_id_;
        return a.index_==b.index_;
    }

    ModelKind Kind() const { return kind_; }
    uint32_t Index() const { return index_; }  // pool slot
    bool IsCanonical() const { return canonical_id_ != 0; }
    bool IsMutable() const { return !IsCanonical(); }
    // Preserve the original definition identity across concrete dispatch.
    // Storage scratch slots are implementation details of canonical models.
    template<class T> T Rebind() const {
        T model(sim_,index_);model.canonical_id_=canonical_id_;return model;
    }
    template<class T> static T CanonicalView(T model,uint16_t definition_id) {
        model.canonical_id_=definition_id;return model;
    }
    Sim& sim() const { return *sim_; }
    GameState& gs() const { return *sim_->g; }

    // The model's dense id within its kind (CardId, PowerId, ...), read from
    // the instance's state.
    uint16_t DenseId() const;
    ModelId Id() const { return ModelId{kind_, DenseId()}; }

    // Whether the C# class of this model overrides `member` (content.json).
    bool Overrides(Member member) const;

    // --- Overridable members declared by AbstractModel.
#define SIM_MEMBER(Base, Ret, Name, Params, Args, Default) Ret Name Params const;
#define SIM_MEMBER_CUSTOM(Base, Ret, Name, Params, Args, Default) \
    Ret Name Params const;                                          \
    Ret Name##_Default Params const;
#include "game/Models/AbstractModel.def"
#undef SIM_MEMBER
#undef SIM_MEMBER_CUSTOM

protected:
    Sim* sim_ = nullptr;
    ModelKind kind_ = ModelKind::Card;
    uint32_t index_ = 0;
    uint16_t canonical_id_ = 0; // transient handle fact; never GameState storage
};

[[noreturn]] void throw_unported(const AbstractModel& model, const char* member);

// Whether a model (kind, dense id) can be used without reaching Unported:
// every member it overrides in C# is ported (generated in Dispatch.cpp).
bool IsUsable(ModelKind kind, uint16_t id);

}  // namespace sts2
