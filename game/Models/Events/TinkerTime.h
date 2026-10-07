// Models/Events/TinkerTime.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/Cards/MadScience.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct TinkerTime : EventModel {
    SIM_MODEL(TinkerTime, EventModel)

    CardType ChosenCardType() const { return static_cast<CardType>(extra(0)); }
    void SetChosenCardType(CardType type) const { extra(0) = static_cast<int32_t>(type); }

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(TinkerTime, ChooseCardType),
                                   InitialOptionKey("CHOOSE_CARD_TYPE")));
        return list;
    }

    void ChooseCardType(const EventOptionState&) const {
        StaticVec<CardType, 3> choices;
        choices.push_back(CardType::Attack);
        choices.push_back(CardType::Skill);
        choices.push_back(CardType::Power);
        Rng().Shuffle(choices);

        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(TinkerTime, Attack),
                                   OptionKey("CHOOSE_CARD_TYPE", "ATTACK")));
        list.push_back(EventOption(SIM_OPTION(TinkerTime, Skill),
                                   OptionKey("CHOOSE_CARD_TYPE", "SKILL")));
        list.push_back(EventOption(SIM_OPTION(TinkerTime, Power),
                                   OptionKey("CHOOSE_CARD_TYPE", "POWER")));
        // TakeRandom(2) shuffles the materialized sequence and returns its
        // first two entries.  Preserve that order in the event options.
        EventOptionList selected;
        for (size_t i = 0; i < 2; ++i) {
            switch (choices[i]) {
            case CardType::Attack: selected.push_back(list[0]); break;
            case CardType::Skill: selected.push_back(list[1]); break;
            case CardType::Power: selected.push_back(list[2]); break;
            default: throw GameError("TinkerTime: invalid card type roll");
            }
        }
        SetEventState(L10NLookup("TINKER_TIME.pages.CHOOSE_CARD_TYPE.description"), selected);
    }

    void Attack(const EventOptionState&) const { SetChosenCardType(CardType::Attack); ChooseRiderEffect(); }
    void Skill(const EventOptionState&) const { SetChosenCardType(CardType::Skill); ChooseRiderEffect(); }
    void Power(const EventOptionState&) const { SetChosenCardType(CardType::Power); ChooseRiderEffect(); }

    void ChooseRiderEffect() const {
        StaticVec<MadScienceRider, 3> riders;
        switch (ChosenCardType()) {
        case CardType::Attack:
            riders.push_back(MadScienceRider::Sapping);
            riders.push_back(MadScienceRider::Violence);
            riders.push_back(MadScienceRider::Choking);
            break;
        case CardType::Skill:
            riders.push_back(MadScienceRider::Energized);
            riders.push_back(MadScienceRider::Wisdom);
            riders.push_back(MadScienceRider::Chaos);
            break;
        case CardType::Power:
            riders.push_back(MadScienceRider::Expertise);
            riders.push_back(MadScienceRider::Curious);
            riders.push_back(MadScienceRider::Improvement);
            break;
        default: throw GameError("TinkerTime: invalid chosen card type");
        }
        Rng().Shuffle(riders);

        EventOptionList list;
        for (size_t i = 0; i < 2; ++i) {
            const MadScienceRider rider = riders[i];
            list.push_back(EventOption(SIM_OPTION(TinkerTime, RiderChosen),
                                       RiderOptionKey(rider)).WithArg(static_cast<int32_t>(rider)));
        }
        SetEventState(L10NLookup("TINKER_TIME.pages.CHOOSE_RIDER.description"), list);
    }

    void RiderChosen(const EventOptionState& option) const {
        const auto rider = static_cast<MadScienceRider>(option.arg);
        CardModel card = Owner().RunState().CreateCard(CardId::MAD_SCIENCE, Owner());
        MadScience madScience(card);
        madScience.SetTinkerTimeType(ChosenCardType());
        madScience.SetTinkerTimeRider(rider);
        CardPileCmd::Add(madScience, PileType::Deck);
        SetEventFinished(L10NLookup("TINKER_TIME.pages.DONE.description"));
    }

    static std::string RiderOptionKey(MadScienceRider rider) {
        switch (rider) {
        case MadScienceRider::Sapping: return "TINKER_TIME.pages.CHOOSE_RIDER.options.SAPPING";
        case MadScienceRider::Violence: return "TINKER_TIME.pages.CHOOSE_RIDER.options.VIOLENCE";
        case MadScienceRider::Choking: return "TINKER_TIME.pages.CHOOSE_RIDER.options.CHOKING";
        case MadScienceRider::Energized: return "TINKER_TIME.pages.CHOOSE_RIDER.options.ENERGIZED";
        case MadScienceRider::Wisdom: return "TINKER_TIME.pages.CHOOSE_RIDER.options.WISDOM";
        case MadScienceRider::Chaos: return "TINKER_TIME.pages.CHOOSE_RIDER.options.CHAOS";
        case MadScienceRider::Expertise: return "TINKER_TIME.pages.CHOOSE_RIDER.options.EXPERTISE";
        case MadScienceRider::Curious: return "TINKER_TIME.pages.CHOOSE_RIDER.options.CURIOUS";
        case MadScienceRider::Improvement: return "TINKER_TIME.pages.CHOOSE_RIDER.options.IMPROVEMENT";
        default: throw GameError("TinkerTime: invalid rider option");
        }
    }
};

}  // namespace sts2
