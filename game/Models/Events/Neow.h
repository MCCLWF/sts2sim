// Models/Events/Neow.cs. The modifier options (runs with modifiers) are not
// ported: a standard run has none.
#pragma once

#include "game/Models/Canonical.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct Neow : EventModel {
    SIM_MODEL(Neow, EventModel)

    static constexpr std::string_view kCursed = "NEOW.pages.DONE.CURSED.description";
    static constexpr std::string_view kPositive = "NEOW.pages.DONE.POSITIVE.description";

    EventOption Option(RelicId relic, std::string_view donePage) const { return AncientRelicOption(relic, "INITIAL", donePage); }

    EventOptionList PositiveOptions() const {
        EventOptionList l;
        for (RelicId r : {RelicId::ARCANE_SCROLL, RelicId::BOOMING_CONCH, RelicId::FISHING_ROD, RelicId::GOLDEN_PEARL,
                          RelicId::KALEIDOSCOPE, RelicId::LEAD_PAPERWEIGHT, RelicId::LOST_COFFER, RelicId::MASSIVE_SCROLL,
                          RelicId::NEOWS_TORMENT, RelicId::NEW_LEAF, RelicId::PHIAL_HOLSTER, RelicId::PRECISE_SCISSORS,
                          RelicId::SCROLL_BOXES, RelicId::WINGED_BOOTS}) {
            l.push_back(Option(r, kPositive));
        }
        return l;
    }
    EventOptionState LavaRockOption() const { return Option(RelicId::LAVA_ROCK, kPositive); }
    EventOptionState NeowsTalismanOption() const { return Option(RelicId::NEOWS_TALISMAN, kPositive); }
    EventOptionState NutritiousOysterOption() const { return Option(RelicId::NUTRITIOUS_OYSTER, kPositive); }
    EventOptionState PomanderOption() const { return Option(RelicId::POMANDER, kPositive); }
    EventOptionState SmallCapsuleOption() const { return Option(RelicId::SMALL_CAPSULE, kPositive); }
    EventOptionState StoneHumidifierOption() const { return Option(RelicId::STONE_HUMIDIFIER, kPositive); }

    EventOptionList CurseOptions() const {
        EventOptionList l;
        l.push_back(Option(RelicId::CURSED_PEARL, kCursed));
        l.push_back(Option(RelicId::DOWSING_ROD, kCursed));
        l.push_back(Option(RelicId::HEFTY_TABLET, kCursed));
        l.push_back(Option(RelicId::LARGE_CAPSULE, kCursed));
        l.push_back(Option(RelicId::LEAFY_POULTICE, kCursed));
        l.push_back(Option(RelicId::NEOWS_BONES, kPositive));
        l.push_back(Option(RelicId::NEOWS_SACRIFICE, kCursed));
        l.push_back(Option(RelicId::PRECARIOUS_SHEARS, kCursed));
        l.push_back(Option(RelicId::SILKEN_TRESS, kCursed));
        l.push_back(Option(RelicId::SILVER_CRUCIBLE, kCursed));
        return l;
    }

    RelicId RelicOf(const EventOptionState& o) const { return o.relic != 0 ? RelicModel(sim_, o.relic).RelicId_() : RelicId::NONE; }

    // list.RemoveAll(r => r.Relic != null && !r.Relic.IsAllowedAtNeow(Owner))
    void RemoveNotAllowedAtNeow(EventOptionList& list) const {
        EventOptionList kept;
        for (const EventOptionState& o : list) {
            if (o.relic != 0 && !RelicModel(sim_, o.relic).IsAllowedAtNeow(Owner())) continue;
            kept.push_back(o);
        }
        list = kept;
    }
    void RemoveRelic(EventOptionList& list, RelicId relic) const {
        EventOptionList kept;
        for (const EventOptionState& o : list) {
            if (RelicOf(o) != relic) kept.push_back(o);
        }
        list = kept;
    }

    EventOptionList GenerateInitialOptions() const {
        // Owner.RunState.Modifiers.Count <= 0 (a standard run).
        EventOptionList list = CurseOptions();
        RemoveNotAllowedAtNeow(list);
        EventOptionState eventOption = Rng().NextItem(list);
        EventOptionList list2 = PositiveOptions();
        RelicId curse = RelicOf(eventOption);
        if (curse == RelicId::CURSED_PEARL) RemoveRelic(list2, RelicId::GOLDEN_PEARL);
        if (curse == RelicId::HEFTY_TABLET) RemoveRelic(list2, RelicId::ARCANE_SCROLL);
        if (curse == RelicId::LEAFY_POULTICE) RemoveRelic(list2, RelicId::NEW_LEAF);
        if (curse == RelicId::PRECARIOUS_SHEARS) RemoveRelic(list2, RelicId::PRECISE_SCISSORS);
        if (curse == RelicId::NEOWS_SACRIFICE) {
            RemoveRelic(list2, RelicId::PHIAL_HOLSTER);
            RemoveRelic(list2, RelicId::LOST_COFFER);
        }
        if (curse != RelicId::LARGE_CAPSULE) {
            if (Rng().NextBool()) {
                list2.push_back(LavaRockOption());
            } else {
                list2.push_back(SmallCapsuleOption());
            }
        }
        if (Rng().NextBool()) {
            list2.push_back(NutritiousOysterOption());
        } else {
            list2.push_back(StoneHumidifierOption());
        }
        if (Rng().NextBool()) {
            list2.push_back(NeowsTalismanOption());
        } else {
            list2.push_back(PomanderOption());
        }
        RemoveNotAllowedAtNeow(list2);
        // list2.ToList().UnstableShuffle(Rng).Take(2), then the curse option.
        EventOptionList shuffled = list2;
        Rng().Shuffle(shuffled);
        EventOptionList list3;
        for (size_t i = 0; i < shuffled.size() && i < 2; ++i) list3.push_back(shuffled[i]);
        list3.push_back(eventOption);
        return list3;
    }
};

}  // namespace sts2
