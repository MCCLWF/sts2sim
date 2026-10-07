// Models/Events/DollRoom.cs
#pragma once

#include <array>
#include <cstddef>

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct DollRoom : EventModel {
    SIM_MODEL(DollRoom, EventModel)

    // The source's _dolls array order is significant for the no-damage random
    // choice. StableShuffle sorts DollChoice by RelicModel.CompareTo first;
    // ModelId compares category then entry using ordinal string order.
    static constexpr std::array<RelicId, 3> kDolls = {
        RelicId::DAUGHTER_OF_THE_WIND, RelicId::MR_STRUGGLES, RelicId::BING_BONG};
    static constexpr std::array<RelicId, 3> kStableSortedDolls = {
        RelicId::BING_BONG, RelicId::DAUGHTER_OF_THE_WIND, RelicId::MR_STRUGGLES};

    // BeforeEventStarted and OnEventFinished only start/stop a debug ambience
    // sound in the original; those hooks have no run-state or RNG effect.
    void BeforeEventStarted(bool isPreFinished) const { (void)isPreFinished; }
    void OnEventFinished() const {}

    EventOptionList GenerateInitialOptions() const {
        EventOptionList options;
        options.push_back(EventOption(SIM_OPTION(DollRoom, ChooseRandom), InitialOptionKey("RANDOM")));
        options.push_back(EventOption(SIM_OPTION(DollRoom, TakeSomeTime), InitialOptionKey("TAKE_SOME_TIME"))
                             .ThatDoesDamage(DynamicVars()["TakeTimeHpLoss"].BaseValue()));
        options.push_back(EventOption(SIM_OPTION(DollRoom, Examine), InitialOptionKey("EXAMINE"))
                             .ThatDoesDamage(DynamicVars()["ExamineHpLoss"].BaseValue()));
        return options;
    }

    void ChooseRandom(const EventOptionState&) const {
        ChooseDollForRelic(Rng().NextItem(kDolls));
    }

    void TakeSomeTime(const EventOptionState&) const {
        PlayerChoiceContext context = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(context, Owner().Creature(), DynamicVars()["TakeTimeHpLoss"], Creature{});

        auto dolls = kStableSortedDolls;
        Rng().Shuffle(dolls);
        EventOptionList options;
        for (size_t i = 0; i < 2; ++i) options.push_back(OptionFromChoice(dolls[i]));
        SetEventState(L10NLookup("DOLL_ROOM.pages.TAKE_SOME_TIME.description"), options);
    }

    void Examine(const EventOptionState&) const {
        PlayerChoiceContext context = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(context, Owner().Creature(), DynamicVars()["ExamineHpLoss"], Creature{});

        auto dolls = kStableSortedDolls;
        Rng().Shuffle(dolls);
        EventOptionList options;
        for (RelicId relic : dolls) options.push_back(OptionFromChoice(relic));
        SetEventState(L10NLookup("DOLL_ROOM.pages.EXAMINE.description"), options);
    }

    void ChooseDoll(const EventOptionState& option) const {
        const RelicId relic = static_cast<RelicId>(option.arg);
        ChooseDollForRelic(relic);
    }

    bool IsAllowed(RunState runState) const { return runState.CurrentActIndex() == 1; }

private:
    void ChooseDollForRelic(RelicId relic) const {
        // ChooseDollAndShowDescription obtains the mutable counterpart of the
        // canonical relic. RelicCmd::Obtain(id, ...) creates that same instance.
        RelicCmd::Obtain(relic, Owner());
        SetEventFinished(L10NLookup(DescriptionFor(relic)));
    }
    EventOption OptionFromChoice(RelicId relic) const {
        // EventOption.TextKey is the relic title's raw localized text in the
        // source constructor. The current oracle runs with English strings.
        // The relic id in arg preserves the selected instance's identity.
        return EventOption(SIM_OPTION(DollRoom, ChooseDoll), TitleFor(relic))
            .WithArg(static_cast<int32_t>(relic));
    }

    static std::string_view TitleFor(RelicId relic) {
        switch (relic) {
        case RelicId::BING_BONG: return "Bing Bong";
        case RelicId::DAUGHTER_OF_THE_WIND: return "Daughter of the Wind";
        case RelicId::MR_STRUGGLES: return "Mr. Struggles";
        default: throw GameError("DollRoom: invalid doll relic");
        }
    }

    static std::string_view DescriptionFor(RelicId relic) {
        switch (relic) {
        case RelicId::DAUGHTER_OF_THE_WIND: return "DOLL_ROOM.pages.DAUGHTER_OF_WIND.description";
        case RelicId::MR_STRUGGLES: return "DOLL_ROOM.pages.MR_STRUGGLES.description";
        case RelicId::BING_BONG: return "DOLL_ROOM.pages.FABLE.description";
        default: throw GameError("DollRoom: invalid doll relic");
        }
    }
};

}  // namespace sts2
