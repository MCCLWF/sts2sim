// Models/Events/JungleMazeAdventure.cs
#pragma once

#include <array>

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct JungleMazeAdventure : EventModel {
    SIM_MODEL(JungleMazeAdventure, EventModel)

    bool IsAllowed(RunState runState) const {
        if (runState.Players().size() == 1) return true;
        for (Player p : runState.Players()) {
            if (Dec(p.Creature().CurrentHp()) <= DynamicVars()["SoloHp"].BaseValue()) return false;
        }
        return true;
    }

    void CalculateVars() const {
        DynamicVars()["SoloGold"].SetBaseValue(
            DynamicVars()["SoloGold"].BaseValue() + Dec::from_float(Rng().NextFloat(-15.0f, 15.0f)));
        DynamicVars()["JoinForcesGold"].SetBaseValue(
            DynamicVars()["JoinForcesGold"].BaseValue() + Dec::from_float(Rng().NextFloat(-15.0f, 15.0f)));
    }

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(JungleMazeAdventure, DontNeedHelp),
                                   "JUNGLE_MAZE_ADVENTURE.pages.INITIAL.options.SOLO_QUEST")
                           .ThatDoesDamage(DynamicVars()["SoloHp"].BaseValue()));
        list.push_back(EventOption(SIM_OPTION(JungleMazeAdventure, SafetyInNumbers),
                                   "JUNGLE_MAZE_ADVENTURE.pages.INITIAL.options.JOIN_FORCES"));
        return list;
    }

    void DontNeedHelp(const EventOptionState&) const {
        // StableShuffle of the three VFX tuples still consumes the two shuffle draws.
        std::array<int32_t, 3> fx = {0, 1, 2};
        Rng().Shuffle(fx);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(ctx, Owner().Creature(), DynamicVars()["SoloHp"].BaseValue(),
                             ValueProp::Unblockable | ValueProp::Unpowered, Creature{});
        PlayerCmd::GainGold(DynamicVars()["SoloGold"].BaseValue(), Owner());
        SetEventFinished(L10NLookup("JUNGLE_MAZE_ADVENTURE.pages.SOLO_QUEST.description"));
    }

    void SafetyInNumbers(const EventOptionState&) const {
        PlayerCmd::GainGold(DynamicVars()["JoinForcesGold"].BaseValue(), Owner());
        SetEventFinished(L10NLookup("JUNGLE_MAZE_ADVENTURE.pages.JOIN_FORCES.description"));
    }
};

}  // namespace sts2
