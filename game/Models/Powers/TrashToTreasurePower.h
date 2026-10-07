#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct TrashToTreasurePower : PowerModel { SIM_MODEL(TrashToTreasurePower, PowerModel)
    void AfterCardGeneratedForCombat(CardModel card, Player creator) const {
        if (!creator || creator.Creature() != Owner() || card.Type() != CardType::Status) return;
        static constexpr OrbId valid[] = {OrbId::LIGHTNING_ORB, OrbId::FROST_ORB, OrbId::DARK_ORB, OrbId::PLASMA_ORB, OrbId::GLASS_ORB};
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        Rng& rng = Owner().Player().RunState().Rng().CombatOrbs();
        for (int32_t i = 0; i < Amount(); ++i) OrbCmd::Channel(valid[rng.next_item_index(5)], ctx, Owner().Player());
    }
};
}
