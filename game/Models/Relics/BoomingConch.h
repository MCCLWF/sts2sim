// Models/Relics/BoomingConch.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Rooms/AbstractRoom.h"
namespace sts2 {
struct BoomingConch : RelicModel {
    SIM_MODEL(BoomingConch, RelicModel)
    Dec ModifyHandDraw(Player player, Dec count) const {
        if (player != Owner() || !player.PlayerCombatState() || player.PlayerCombatState().TurnNumber() > 1) return count;
        AbstractRoom room = player.RunState().st().rooms.empty() ? AbstractRoom{} : AbstractRoom(&sim(), player.RunState().st().rooms.back());
        if (room.RoomType() != RoomType::Elite) return count;
        return count + DynamicVars().Cards().BaseValue();
    }
    void AfterSideTurnStart(CombatSide side, const CreatureList& participants, CombatState combatState) const {
        (void)side;
        bool present = false;
        for (Creature c : participants) if (c == Owner().Creature()) present = true;
        if (!present || !Owner().PlayerCombatState() || Owner().PlayerCombatState().TurnNumber() > 1) return;
        AbstractRoom room = Owner().RunState().st().rooms.empty() ? AbstractRoom{} : AbstractRoom(&sim(), Owner().RunState().st().rooms.back());
        if (room.RoomType() == RoomType::Elite) PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
    }
};
}
