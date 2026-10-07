// Models/Encounters/TestSubjectBoss.cs
#pragma once
#include "game/Models/EncounterModel.h"
namespace sts2 {
struct TestSubjectBoss : EncounterModel {
    SIM_MODEL(TestSubjectBoss, EncounterModel)
    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::TEST_SUBJECT), SlotIndex("")}};
    }
};
}  // namespace sts2
