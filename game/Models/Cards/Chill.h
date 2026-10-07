#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Chill : CardModel { SIM_MODEL(Chill, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CreatureList enemies = CombatState().HittableEnemies();
        for (Creature enemy : enemies) { (void)enemy; OrbCmd::Channel(OrbId::FROST_ORB, ctx, Owner()); }
    }
};
}
