// See resample.h.
#include "nosl/resample.h"

#include "core/rng.h"
#include "game/All.h"

namespace sts2::nosl {

void ResampleHidden(Env& world, uint64_t seed) {
    GameState& g = world.state();
    uint64_t k = seed;
    auto fresh = [&k] { k += 0x9E3779B97F4A7C15ull; return Rng(k); };
    for (RunRngType type : {RunRngType::Shuffle, RunRngType::CombatCardGeneration, RunRngType::CombatPotionGeneration,
                            RunRngType::CombatCardSelection, RunRngType::CombatEnergyCosts, RunRngType::CombatTargets,
                            RunRngType::MonsterAi, RunRngType::Niche, RunRngType::CombatOrbs})
        g.run.rng[type] = fresh();
    g.combat.encounter_rng = fresh();
    for (Creature enemy : CombatState(&world.sim()).Enemies()) enemy.st().monster.rng = fresh();
    world.Determinize(seed);  // last: it also takes the snapshot a choice inside an effect replays from
}

}  // namespace sts2::nosl
