// Models/Events/TheArchitect.cs (v0.111.0).
#pragma once

#include <string>

#include "game/Models/EventModel.h"
#include "game/Runs/RunManager.h"

namespace sts2 {

struct TheArchitect : EventModel {
    SIM_MODEL(TheArchitect, EventModel)

    EncounterId CanonicalEncounter() const { return EncounterId::THE_ARCHITECT_EVENT_ENCOUNTER; }

    void SetInitialEventState(bool) const {
        // The Architect replaces EventModel's normal INITIAL page with the
        // empty proceed description while it presents the selected dialogue.
        SetEventState(MakeLocKey("PROCEED.description"), GenerateInitialOptionsWrapper());
    }

    EventOptionList GenerateInitialOptions() const {
        const int32_t lineCount = DialogueLineCount(Owner().Character(), sim_->g->run.progress_character_wins);
        extra(0) = lineCount;
        extra(1) = 0;

        if (lineCount == 0) return EventOptionList{CreateProceedOption()};

        // TheArchitect.LoadDialogue materializes exactly one valid dialogue
        // for each supported TotalWins index and still calls NextItem on that
        // singleton sequence, consuming NextInt(0, 1) from the event RNG.
        (void)Rng().next_item_index(1);
        return EventOptionList{CreateOptionForCurrentLine()};
    }

    void AdvanceDialogue(const EventOptionState&) const {
        ++extra(1);
        SetEventState(MakeLocKey("PROCEED.description"), EventOptionList{CreateOptionForCurrentLine()});
    }

    void WinRun(const EventOptionState&) const {
        RunManager::WinRun(sim_);
        // The game clears its choices after RunManager.WinRun. Keep the Event
        // room as the terminal room, with no further available actions.
        SetEventState(MakeLocKey("PROCEED.description"), EventOptionList{});
    }

    void OnRoomEnter() const {
        // The original override only refreshes score/UI state, binds the
        // combat-layout creature to presentation VFX, and clears/re-adds
        // options around speech-bubble animation. The Env has no UI nodes;
        // all deterministic dialogue choices are built in GenerateInitialOptions.
    }

private:
    static int32_t DialogueLineCount(CharacterId character, int32_t totalWins) {
        // The source's DialogueSet has only visit-indexed character entries,
        // no first-ever or character-agnostic dialogue, and no repeating pool.
        // A missing index therefore yields a null dialogue and a direct
        // PROCEED option.
        switch (character) {
        case CharacterId::IRONCLAD:
            if (totalWins == 0) return 2;
            if (totalWins == 1 || totalWins == 2) return 3;
            return 0;
        case CharacterId::SILENT:
            return totalWins >= 0 && totalWins < 4 ? 1 : 0;
        case CharacterId::DEFECT:
            return totalWins >= 0 && totalWins < 3 ? 3 : 0;
        case CharacterId::NECROBINDER:
            if (totalWins >= 0 && totalWins < 3) return 2;
            if (totalWins == 3) return 3;
            return 0;
        case CharacterId::REGENT:
            return totalWins >= 0 && totalWins < 3 ? 3 : 0;
        default: return 0;
        }
    }

    EventOption CreateOptionForCurrentLine() const {
        const int32_t lineCount = extra(0);
        const int32_t lineIndex = extra(1);
        if (lineCount <= 0 || lineIndex >= lineCount - 1) return CreateProceedOption();
        const std::string key = "THE_ARCHITECT.dialogue." + std::to_string(lineIndex);
        return EventOption(SIM_OPTION(TheArchitect, AdvanceDialogue), key);
    }

    EventOption CreateProceedOption() const {
        // The source uses a selectable EventOption with the PROCEED key; it
        // is not NEventRoom.Proceed (IsProceed remains false).
        return EventOption(SIM_OPTION(TheArchitect, WinRun), "PROCEED", false, false);
    }
};

}  // namespace sts2
