// Models/Encounters/KnowledgeDemonBoss.cs
#pragma once
#include "game/Models/EncounterModel.h"
namespace sts2 {
struct KnowledgeDemonBoss : EncounterModel {
    SIM_MODEL(KnowledgeDemonBoss, EncounterModel)
    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::KNOWLEDGE_DEMON), -1}};
    }
};
}  // namespace sts2
