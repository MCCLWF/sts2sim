// Models/Relics/TeaOfDiscourtesy.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct TeaOfDiscourtesy : RelicModel {
    SIM_MODEL(TeaOfDiscourtesy, RelicModel)
    bool IsUsedUp() const { return extra(0) >= 1; }
    bool ShowCounter() const { return false; }
    void BeforeCombatStart() const {
        if (IsUsedUp()) return;
        CardPileCmd::AddToCombatAndPreview(CardId::DAZED, Owner().Creature(), PileType::Draw,
                                           DynamicVars()["DazedCount"].IntValue(), Owner(), CardPilePosition::Random);
        extra(0) = 1;
        DynamicVars()["Combats"].SetBaseValue(Dec(0));
        SetStatus(RelicStatus::Disabled);
    }
};

}  // namespace sts2
