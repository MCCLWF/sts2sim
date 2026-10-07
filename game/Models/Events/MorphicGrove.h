// Models/Events/MorphicGrove.cs (IsAllowed partial port)
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct MorphicGrove : EventModel {
    SIM_MODEL(MorphicGrove, EventModel)
    EventOptionList GenerateInitialOptions() const { EventOptionList list; list.push_back(EventOption(SIM_OPTION(MorphicGrove, Group), InitialOptionKey("GROUP"))); list.push_back(EventOption(SIM_OPTION(MorphicGrove, Loner), InitialOptionKey("LONER"))); return list; }
    void Loner(const EventOptionState&) const { CreatureCmd::GainMaxHp(Owner().Creature(), Dec(5)); SetEventFinished(L10NLookup("MORPHIC_GROVE.pages.LONER.description")); }
    void Group(const EventOptionState&) const { PlayerCmd::LoseGold(Dec(Owner().Gold()), Owner(), GoldLossType::Stolen); CardList cards = CardSelectCmd::FromDeckForTransformation(Owner(), CardSelectorPrefs(2)); for (CardModel c : cards) CardCmd::TransformToRandom(c, Rng()); SetEventFinished(L10NLookup("MORPHIC_GROVE.pages.GROUP.description")); }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const { return all_players(runState, [](Player p) { int n = 0; for (CardModel c : p.Deck().Cards()) if (c.IsTransformable()) ++n; return p.Gold() >= 100 && n >= 2; }); }
};

}  // namespace sts2
