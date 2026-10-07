// Models/Events/DenseVegetation.cs (IsAllowed partial port)
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Entities/RestSite/RestSiteOption.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"
#include "game/Runs/RunManager.h"

namespace sts2 {

struct DenseVegetation : EventModel {
    SIM_MODEL(DenseVegetation, EventModel)
    void CalculateVars() const {
        DynamicVars().Gold().SetBaseValue(Dec(Rng().NextInt(61, 100)));
        DynamicVars().Heal().SetBaseValue(RestSite::GetHealAmount(Owner()));
    }
    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(DenseVegetation, TrudgeOn), InitialOptionKey("TRUDGE_ON"))
                           .ThatDoesDamage(DynamicVars().HpLoss().BaseValue()));
        list.push_back(EventOption(SIM_OPTION(DenseVegetation, Rest), InitialOptionKey("REST")));
        return list;
    }
    void TrudgeOn(const EventOptionState&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(ctx, Owner().Creature(), DynamicVars().HpLoss(), Creature{});
        PlayerCmd::GainGold(DynamicVars().Gold().BaseValue(), Owner());
        SetEventFinished(L10NLookup("DENSE_VEGETATION.pages.TRUDGE_ON.description"));
    }
    void Rest(const EventOptionState&) const {
        RestSite::ExecuteRestSiteHeal(Owner(), true);
        EventOptionList options;
        options.push_back(EventOption(SIM_OPTION(DenseVegetation, Fight), "DENSE_VEGETATION.pages.REST.options.FIGHT"));
        SetEventState(L10NLookup("DENSE_VEGETATION.pages.REST.description"), options);
    }
    void Fight(const EventOptionState&) const {
        RunManager::EnterEventCombat(sim_, EncounterId::DENSE_VEGETATION_EVENT_ENCOUNTER);
    }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const { if (runState.Players().size() == 1) return true; return all_players(runState, [this](Player p) { return Dec(p.Creature().CurrentHp()) > DynamicVars().HpLoss().BaseValue(); }); }
};

}  // namespace sts2
