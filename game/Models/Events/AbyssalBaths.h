// Models/Events/AbyssalBaths.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/EventModel.h"

namespace sts2 {

struct AbyssalBaths : EventModel {
    SIM_MODEL(AbyssalBaths, EventModel)

    int32_t& LingerCount() const { return extra(0); }

    Dec CurrentDamage() const {
        return Dec(DynamicVars().Damage().IntValue()) - DynamicVars().MaxHp().BaseValue();
    }

    EventOption DamageOption(uint16_t fn, std::string_view key) const {
        return EventOption(fn, key).ThatDoesDamage(CurrentDamage());
    }

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(DamageOption(SIM_OPTION(AbyssalBaths, Immerse),
                                    "ABYSSAL_BATHS.pages.INITIAL.options.IMMERSE"));
        list.push_back(EventOption(SIM_OPTION(AbyssalBaths, Abstain),
                                   "ABYSSAL_BATHS.pages.INITIAL.options.ABSTAIN"));
        return list;
    }

    void Immerse(const EventOptionState&) const {
        OnImmerse();
        EventOptionList list;
        list.push_back(DamageOption(SIM_OPTION(AbyssalBaths, Linger),
                                    "ABYSSAL_BATHS.pages.ALL.options.LINGER"));
        list.push_back(EventOption(SIM_OPTION(AbyssalBaths, ExitBaths),
                                   "ABYSSAL_BATHS.pages.ALL.options.EXIT_BATHS"));
        SetEventState(L10NLookup("ABYSSAL_BATHS.pages.IMMERSE.description"), list);
    }

    void Abstain(const EventOptionState&) const {
        CreatureCmd::Heal(Owner().Creature(), DynamicVars().Heal().BaseValue());
        SetEventFinished(L10NLookup("ABYSSAL_BATHS.pages.ABSTAIN.description"));
    }

    void Linger(const EventOptionState&) const {
        ++LingerCount();
        if (LingerCount() > 9) LingerCount() = 9;
        OnImmerse();
        if (Dec(Owner().Creature().CurrentHp()) <= CurrentDamage()) {
            EventOptionList list;
            list.push_back(DamageOption(SIM_OPTION(AbyssalBaths, Linger),
                                        "ABYSSAL_BATHS.pages.ALL.options.LINGER"));
            list.push_back(EventOption(SIM_OPTION(AbyssalBaths, ExitBaths),
                                       "ABYSSAL_BATHS.pages.ALL.options.EXIT_BATHS"));
            SetEventState(L10NLookup("ABYSSAL_BATHS.pages.DEATH_WARNING.description"), list);
            return;
        }
        EventOptionList list;
        list.push_back(DamageOption(SIM_OPTION(AbyssalBaths, Linger),
                                    "ABYSSAL_BATHS.pages.ALL.options.LINGER"));
        list.push_back(EventOption(SIM_OPTION(AbyssalBaths, ExitBaths),
                                   "ABYSSAL_BATHS.pages.ALL.options.EXIT_BATHS"));
        LocKey page = L10NLookup(LingerCount() == 1 ? "ABYSSAL_BATHS.pages.LINGER1.description" :
                                 LingerCount() == 2 ? "ABYSSAL_BATHS.pages.LINGER2.description" :
                                 LingerCount() == 3 ? "ABYSSAL_BATHS.pages.LINGER3.description" :
                                 LingerCount() == 4 ? "ABYSSAL_BATHS.pages.LINGER4.description" :
                                 LingerCount() == 5 ? "ABYSSAL_BATHS.pages.LINGER5.description" :
                                 LingerCount() == 6 ? "ABYSSAL_BATHS.pages.LINGER6.description" :
                                 LingerCount() == 7 ? "ABYSSAL_BATHS.pages.LINGER7.description" :
                                 LingerCount() == 8 ? "ABYSSAL_BATHS.pages.LINGER8.description" :
                                 "ABYSSAL_BATHS.pages.LINGER9.description");
        SetEventState(page, list);
    }

    void ExitBaths(const EventOptionState&) const {
        SetEventFinished(L10NLookup("ABYSSAL_BATHS.pages.EXIT_BATHS.description"));
    }

    void OnImmerse() const {
        CreatureCmd::GainMaxHp(Owner().Creature(), DynamicVars().MaxHp().BaseValue());
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(ctx, Owner().Creature(), DynamicVars().Damage(), Creature{});
        DynamicVars().Damage().UpgradeValueBy(Dec(1));
    }
};

}  // namespace sts2
