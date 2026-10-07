// Models/Cards/Regret.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Regret : CardModel {
    SIM_MODEL(Regret, CardModel)
    int32_t MaxUpgradeLevel() const { return 0; }
    bool HasTurnEndInHandEffect() const { return true; }
    void BeforeSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner().Creature())) return;
        CardPile pile = Pile();
        if (!pile || pile.Type() != PileType::Hand) return;
        st().extra[0] = pile.Count();
    }
    void OnTurnEndInHand(PlayerChoiceContext& ctx) const {
        CreatureCmd::Damage(ctx, Owner().Creature(), Dec(st().extra[0]),
                            ValueProp::Unblockable | ValueProp::Unpowered | ValueProp::Move, *this, nullptr);
        st().extra[0] = 0;
    }
};

}  // namespace sts2
