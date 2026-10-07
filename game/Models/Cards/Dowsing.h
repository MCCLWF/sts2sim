// Models/Cards/Dowsing.cs
#pragma once
#include "game/Commands/CardCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Dowsing : CardModel {
    SIM_MODEL(Dowsing, CardModel)
    int32_t MaxUpgradeLevel() const { return 0; }
    void BeforeRoomEntered(AbstractRoom) const {
        const CardPile pile = Pile();
        RunState run = Owner().RunState();
        if (!pile || pile.Type() != PileType::Deck || run.CurrentRoomCount() > 1) return;
        const int point = run.st().map.get_point(run.CurrentMapCol(), run.CurrentMapRow());
        if (point < 0 || run.st().map.points[static_cast<size_t>(point)].type != MapPointType::Unknown) return;
        ++extra(0);  // SavedProperty RoomsEntered.
        DynamicVars()["Rooms"].SetBaseValue(Dec(5 - extra(0)));
        if (extra(0) >= 5)
            CardCmd::Transform(*this, run.CreateCard(CardId::ABUNDANCE, Owner()));
    }
};
}
