// Models/Encounters/RubyRaidersNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct RubyRaidersNormal : EncounterModel {
    SIM_MODEL(RubyRaidersNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        static constexpr MonsterId raiders[5] = {
            MonsterId::AXE_RUBY_RAIDER,
            MonsterId::ASSASSIN_RUBY_RAIDER,
            MonsterId::BRUTE_RUBY_RAIDER,
            MonsterId::CROSSBOW_RUBY_RAIDER,
            MonsterId::TRACKER_RUBY_RAIDER,
        };
        StaticVec<MonsterId, 3> current;
        MonsterSpawnList result;
        for (int i = 0; i < 3; ++i) {
            StaticVec<MonsterId, 5> candidates;
            for (MonsterId id : raiders) {
                if (!current.contains(id)) candidates.push_back(id);
            }
            MonsterId chosen = Rng().NextItem(candidates);
            current.push_back(chosen);
            result.push_back({MonsterModel::ToMutable(sim_, chosen), -1});
        }
        return result;
    }
};

}  // namespace sts2
