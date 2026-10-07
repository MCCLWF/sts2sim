// Models/Events/TeaMaster.cs
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct TeaMaster : EventModel {
    SIM_MODEL(TeaMaster, EventModel)

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(Owner().Gold() >= 50
                           ? EventOption(SIM_OPTION(TeaMaster, BoneTea),
                                         "TEA_MASTER.pages.INITIAL.options.BONE_TEA")
                           : EventOption(0, "TEA_MASTER.pages.INITIAL.options.BONE_TEA_LOCKED"));
        list.push_back(Owner().Gold() >= 150
                           ? EventOption(SIM_OPTION(TeaMaster, EmberTea),
                                         "TEA_MASTER.pages.INITIAL.options.EMBER_TEA")
                           : EventOption(0, "TEA_MASTER.pages.INITIAL.options.EMBER_TEA_LOCKED"));
        list.push_back(EventOption(SIM_OPTION(TeaMaster, TeaOfDiscourtesy),
                                   "TEA_MASTER.pages.INITIAL.options.TEA_OF_DISCOURTESY"));
        return list;
    }

    void BoneTea(const EventOptionState&) const {
        PlayerCmd::LoseGold(Dec(50), Owner(), GoldLossType::Spent);
        RelicCmd::Obtain(RelicId::BONE_TEA, Owner());
        SetEventFinished(L10NLookup("TEA_MASTER.pages.DONE.description"));
    }

    void EmberTea(const EventOptionState&) const {
        PlayerCmd::LoseGold(Dec(150), Owner(), GoldLossType::Spent);
        RelicCmd::Obtain(RelicId::EMBER_TEA, Owner());
        SetEventFinished(L10NLookup("TEA_MASTER.pages.DONE.description"));
    }

    void TeaOfDiscourtesy(const EventOptionState&) const {
        RelicCmd::Obtain(RelicId::TEA_OF_DISCOURTESY, Owner());
        SetEventFinished(L10NLookup("TEA_MASTER.pages.TEA_OF_DISCOURTESY.description"));
    }

    bool IsAllowed(RunState runState) const {
        if (runState.CurrentActIndex() >= 2) return false;
        for (Player p : runState.Players()) if (p.Gold() < 150) return false;
        return true;
    }
};

}  // namespace sts2
