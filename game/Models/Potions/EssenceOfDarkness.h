#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct EssenceOfDarkness : PotionModel { SIM_MODEL(EssenceOfDarkness, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        if (!target || !target.Player()) throw GameError("Essence of Darkness requires a player target");
        Player player = target.Player();
        int32_t count = player.PlayerCombatState().st().orb_capacity;
        for (int32_t i = 0; i < count; ++i) OrbCmd::Channel(OrbId::DARK_ORB, ctx, player);
    }
};
}
