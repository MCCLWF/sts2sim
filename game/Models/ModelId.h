// Models/ModelId.cs: a model's identity (category + entry). Here the category
// is the ModelKind and the entry is the dense id within that kind.
#pragma once

#include <cstdint>

#include "content/generated/ids.h"

namespace sts2 {

struct ModelId {
    ModelKind kind = ModelKind::Card;
    uint16_t id = 0;

    friend bool operator==(const ModelId&, const ModelId&) = default;

    static constexpr ModelId Of(CardId v) { return {ModelKind::Card, static_cast<uint16_t>(v)}; }
    static constexpr ModelId Of(RelicId v) { return {ModelKind::Relic, static_cast<uint16_t>(v)}; }
    static constexpr ModelId Of(PowerId v) { return {ModelKind::Power, static_cast<uint16_t>(v)}; }
    static constexpr ModelId Of(PotionId v) { return {ModelKind::Potion, static_cast<uint16_t>(v)}; }
    static constexpr ModelId Of(MonsterId v) { return {ModelKind::Monster, static_cast<uint16_t>(v)}; }
    static constexpr ModelId Of(EncounterId v) { return {ModelKind::Encounter, static_cast<uint16_t>(v)}; }
    static constexpr ModelId Of(EventId v) { return {ModelKind::Event, static_cast<uint16_t>(v)}; }
    static constexpr ModelId Of(EnchantmentId v) { return {ModelKind::Enchantment, static_cast<uint16_t>(v)}; }
    static constexpr ModelId Of(AfflictionId v) { return {ModelKind::Affliction, static_cast<uint16_t>(v)}; }
    static constexpr ModelId Of(OrbId v) { return {ModelKind::Orb, static_cast<uint16_t>(v)}; }
    static constexpr ModelId Of(ModifierId v) { return {ModelKind::Modifier, static_cast<uint16_t>(v)}; }
};

}  // namespace sts2
