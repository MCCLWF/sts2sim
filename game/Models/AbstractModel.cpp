// Models/AbstractModel.cs
#include "game/Models/AbstractModel.h"

#include <string>

#include "engine/sim/model_info.h"

namespace sts2 {

uint16_t AbstractModel::DenseId() const {
    if(canonical_id_)return canonical_id_;
    const GameState& g = *sim_->g;
    switch (kind_) {
    case ModelKind::Card: return static_cast<uint16_t>(g.cards[index_].id);
    case ModelKind::Power: return static_cast<uint16_t>(g.powers[index_].id);
    case ModelKind::Monster: return static_cast<uint16_t>(g.creatures[index_].monster.id);
    case ModelKind::Relic: return static_cast<uint16_t>(g.relics[index_].id);
    case ModelKind::Potion: return static_cast<uint16_t>(g.potions[index_].id);
    case ModelKind::Orb: return static_cast<uint16_t>(g.orbs[index_].id);
    case ModelKind::Enchantment: return static_cast<uint16_t>(g.cards[index_].enchantment);
    case ModelKind::Affliction:
        if ((index_ & 0x80000000) != 0 && g.afflictions.live(index_ & 0x7fffffff))
            return static_cast<uint16_t>(g.afflictions[index_ & 0x7fffffff].id);
        return static_cast<uint16_t>(AfflictionId::NONE);
    case ModelKind::Encounter: return static_cast<uint16_t>(g.combat.encounter);  // the combat's encounter
    case ModelKind::Event: return static_cast<uint16_t>(g.run.events[index_ - 1].id);  // 1 = the event, 2 = scratch
    default: throw GameError("DenseId: model kind without instances");
    }
}

bool AbstractModel::Overrides(Member member) const {
    const ModelInfo* info = model_info(kind_, DenseId());
    return info != nullptr && info->overrides.has(member);
}

void throw_unported(const AbstractModel& model, const char* member) {
    const ModelInfo* info = model_info(model.Kind(), model.DenseId());
    throw Unported(std::string(info ? info->type : "?") + "." + member + " is not ported");
}

}  // namespace sts2
