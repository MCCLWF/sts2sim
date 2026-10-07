// Models/Encounters/SlitheringStranglerNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct SlitheringStranglerNormal : EncounterModel {
    SIM_MODEL(SlitheringStranglerNormal, EncounterModel)

    enum class SecondaryEnemyType : uint8_t { SnappingJaxfruit, MediumSlime, SmallSlimes };

    MonsterSpawnList GenerateMonsters() const {
        StaticVec<MonsterId, 2> small{MonsterId::LEAF_SLIME_S, MonsterId::TWIG_SLIME_S};
        StaticVec<MonsterId, 2> medium{MonsterId::LEAF_SLIME_M, MonsterId::TWIG_SLIME_M};
        StaticVec<MonsterId, 3> secondary;
        switch (Rng().NextItem(StaticVec<SecondaryEnemyType, 3>{SecondaryEnemyType::SnappingJaxfruit,
                                                               SecondaryEnemyType::MediumSlime,
                                                               SecondaryEnemyType::SmallSlimes})) {
        case SecondaryEnemyType::SnappingJaxfruit:
            secondary.push_back(MonsterId::SNAPPING_JAXFRUIT);
            break;
        case SecondaryEnemyType::MediumSlime:
            secondary.push_back(Rng().NextItem(medium));
            break;
        case SecondaryEnemyType::SmallSlimes:
            secondary.push_back(Rng().NextItem(small));
            secondary.push_back(Rng().NextItem(small));
            break;
        }
        MonsterSpawnList result;
        for (MonsterId id : secondary) result.push_back({MonsterModel::ToMutable(sim_, id), -1});
        result.push_back({MonsterModel::ToMutable(sim_, MonsterId::SLITHERING_STRANGLER), -1});
        return result;
    }
};

}  // namespace sts2
