#include <stdexcept>

#include "env/env.h"
#include "game/All.h"
#include "game/Models/Powers/BattlewornDummyTimeLimitPower.h"

using namespace sts2;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void check_timeout(EncounterId encounter) {
    Env env;
    env.ResetCombat(NewRunOptions{.seed = "DUMMYTIME", .character = CharacterId::IRONCLAD}, encounter);

    CombatState combat(&env.sim());
    require(combat.Enemies().size() == 1, "Battleworn event did not create its dummy");
    Creature dummy = combat.Enemies().front();
    PowerModel power = dummy.GetPower(PowerId::BATTLEWORN_DUMMY_TIME_LIMIT_POWER);
    require(static_cast<bool>(power), "dummy has no time limit power");
    BattlewornDummyTimeLimitPower timeout(power);
    CreatureList participants;
    participants.push_back(dummy);

    CreatureList other_participants;
    other_participants.push_back(Player(&env.sim(), 1).Creature());
    PlayerChoiceContext context = PlayerChoiceContext::Throwing();
    timeout.AfterSideTurnEnd(context, CombatSide::Enemy, other_participants);
    require(timeout.Amount() == 3, "time limit changed when the dummy did not participate");
    require(!env.state().combat.battleworn_dummy_ran_out_of_time,
            "timeout flag set before the dummy's final turn");

    for (int expected : {2, 1}) {
        timeout.AfterSideTurnEnd(context, CombatSide::Enemy, participants);
        require(timeout.Amount() == expected, "time limit counter decremented incorrectly");
        require(!env.state().combat.battleworn_dummy_ran_out_of_time,
                "timeout flag set before the counter reached one");
        require(combat.Enemies().contains(dummy), "dummy escaped before its time limit expired");
    }

    timeout.AfterSideTurnEnd(context, CombatSide::Enemy, participants);
    require(env.state().combat.battleworn_dummy_ran_out_of_time,
            "final time limit turn did not set the encounter flag");
    require(combat.Enemies().empty(), "dummy did not escape after time ran out");
    require(combat.EscapedCreatures().size() == 1 && combat.EscapedCreatures().front() == dummy,
            "dummy escape was not recorded");
}

void check_snapshot_roundtrip() {
    Env env;
    env.ResetCombat(NewRunOptions{.seed = "DUMMYTIME", .character = CharacterId::IRONCLAD},
                    EncounterId::CULTISTS_NORMAL);
    env.state().combat.battleworn_dummy_ran_out_of_time = true;
    const std::string snapshot = env.ExportSnapshotJson();
    auto restored = Env::ImportSnapshotJson(snapshot);
    require(restored->state().combat.battleworn_dummy_ran_out_of_time,
            "snapshot restore dropped Battleworn timeout state");
    require(restored->ExportSnapshotJson() == snapshot, "Battleworn timeout snapshot did not round-trip exactly");
}

}  // namespace

int main() {
    check_timeout(EncounterId::BATTLEWORN_DUMMY_EVENT_V1_ENCOUNTER);
    check_timeout(EncounterId::BATTLEWORN_DUMMY_EVENT_V2_ENCOUNTER);
    check_timeout(EncounterId::BATTLEWORN_DUMMY_EVENT_V3_ENCOUNTER);
    check_snapshot_roundtrip();
}
