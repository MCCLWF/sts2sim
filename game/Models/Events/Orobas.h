// Models/Events/Orobas.cs. The three displayed offers are drawn from three
// materialized pools; creating every candidate matters for model allocation.
#pragma once

#include "engine/sim/alloc.h"
#include "game/Commands/CardCmd.h"
#include "game/Models/EventModel.h"
#include "game/Models/Relics/ArchaicTooth.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct Orobas : EventModel {
    SIM_MODEL(Orobas, EventModel)

    RelicModel NewRelic(RelicId id) const { return RelicModel(sim_, alloc_relic(*sim_->g, id, 0)); }

    static RelicId RefinedStarter(RelicId id) {
        switch (id) {
        case RelicId::BURNING_BLOOD: return RelicId::BLACK_BLOOD;
        case RelicId::RING_OF_THE_SNAKE: return RelicId::RING_OF_THE_DRAKE;
        case RelicId::DIVINE_RIGHT: return RelicId::DIVINE_DESTINY;
        case RelicId::BOUND_PHYLACTERY: return RelicId::PHYLACTERY_UNBOUND;
        case RelicId::CRACKED_CORE: return RelicId::INFUSED_CORE;
        default: return RelicId::CIRCLET;
        }
    }

    // TouchOfOrobas.SetupForPlayer: the first owned Starter-rarity relic.
    // extra[0..1] hold the two SavedProperty model ids.
    bool SetupTouch(RelicModel relic) const {
        for (RelicModel owned : Owner().Relics()) {
            if (owned.Rarity() != RelicRarity::Starter) continue;
            const RelicId upgraded = RefinedStarter(owned.RelicId_());
            if (upgraded == RelicId::CIRCLET) (void)NewRelic(RelicId::CIRCLET);
            relic.st().extra[0] = static_cast<int32_t>(owned.RelicId_());
            relic.st().extra[1] = static_cast<int32_t>(upgraded);
            return true;
        }
        return false;
    }

    // ArchaicTooth.SetupForPlayer. The original records two SerializableCard
    // values; these pool references retain the corresponding immutable facts
    // in the copied GameState, including upgrade/enchantment fields.
    bool SetupTooth(RelicModel relic) const {
        for (CardModel starter : Owner().Deck().Cards()) {
            const CardId ancient = ArchaicTooth::Transcendence(starter.CardId_());
            if (ancient == CardId::NONE) continue;
            CardModel transformed = Owner().RunState().CreateCard(ancient, Owner());
            if (starter.CurrentUpgradeLevel() > 0) CardCmd::Upgrade(transformed);
            CardCmd::CloneEnchantment(starter, transformed);
            relic.st().extra[0] = starter.Index();
            relic.st().extra[1] = transformed.Index();
            return true;
        }
        return false;
    }

    EventOptionList GenerateInitialOptions() const {
        // UnlockState.all: Characters uses ModelDb.AllCharacters order.
        StaticVec<CharacterId, 8> otherCharacters;
        for (CharacterId id : modeldb::all_characters()) {
            if (id != Owner().Character()) otherCharacters.push_back(id);
        }
        CharacterId character = Rng().NextItem(otherCharacters);
        if (character == CharacterId::NONE) character = Owner().Character();

        // OptionPool1.ToList() allocates both relics before the float draw.
        EventOptionList first;
        first.push_back(AncientRelicOption(RelicId::ELECTRIC_SHRYMP));
        first.push_back(AncientRelicOption(RelicId::GLASS_EYE));
        if (Rng().NextFloat() < 0.3333333f) {
            first.push_back(AncientRelicOption(RelicId::PRISMATIC_GEM));
        } else {
            RelicModel seaGlass = NewRelic(RelicId::SEA_GLASS);
            seaGlass.st().extra[0] = static_cast<int32_t>(character);  // SavedProperty CharacterId
            first.push_back(AncientRelicOption(seaGlass));
        }

        EventOptionList result;
        result.push_back(Rng().NextItem(first));

        // OptionPool2 is evaluated after the first selection, in source order.
        EventOptionList second;
        second.push_back(AncientRelicOption(RelicId::ALCHEMICAL_COFFER));
        second.push_back(AncientRelicOption(RelicId::DRIFTWOOD));
        second.push_back(AncientRelicOption(RelicId::RADIANT_PEARL));
        second.push_back(AncientRelicOption(RelicId::SAND_CASTLE));
        result.push_back(Rng().NextItem(second));

        // OptionPool3 constructs both mutable candidates before its draw.
        EventOptionList third;
        RelicModel touch = NewRelic(RelicId::TOUCH_OF_OROBAS);
        if (SetupTouch(touch)) third.push_back(AncientRelicOption(touch));
        RelicModel tooth = NewRelic(RelicId::ARCHAIC_TOOTH);
        if (SetupTooth(tooth)) third.push_back(AncientRelicOption(tooth));
        if (third.empty()) third.push_back(EventOption(0, InitialOptionKey("OPTION_POOL_3_LOCKED")));
        result.push_back(Rng().NextItem(third));
        return result;
    }
};

}  // namespace sts2
