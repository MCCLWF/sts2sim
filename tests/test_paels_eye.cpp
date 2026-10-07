#include <stdexcept>

#include "env/env.h"
#include "game/All.h"
#include "game/Combat/History/CombatHistory.h"
#include "game/Commands/RelicCmd.h"
#include "game/Models/Relics/PaelsEye.h"

using namespace sts2;

static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

int main() {
    {
        Env env;
        env.ResetCombat(NewRunOptions{.seed = "PAELEYEEMPTY", .character = CharacterId::IRONCLAD},
                        EncounterId::CULTISTS_NORMAL);
        Player player(&env.sim(), 1);
        RelicModel relic = RelicCmd::Obtain(RelicId::PAELS_EYE, player);
        PaelsEye eye(&env.sim(), relic.Index());
        int hand = static_cast<int>(player.PlayerCombatState().Hand().Count());
        require(hand > 0 && eye.ShouldTakeExtraTurn(player), "unused Eye did not request an extra turn");
        env.Step(Action{.kind = Action::EndTurn});
        require(player.PlayerCombatState().TurnNumber() == 2 && eye.UsedThisCombat() &&
                    !eye.ShouldTakeExtraTurn(player) &&
                    player.PlayerCombatState().ExhaustPile().Count() >= hand,
                "skipped first turn did not exhaust hand and grant exactly one extra turn");
        const std::string snapshot = env.ExportSnapshotJson();
        auto restored = Env::ImportSnapshotJson(snapshot);
        require(restored->ExportSnapshotJson() == snapshot, "Eye turn state did not survive snapshot");
    }
    {
        Env env;
        env.ResetCombat(NewRunOptions{.seed = "PAELEYEPLAYED", .character = CharacterId::IRONCLAD},
                        EncounterId::CULTISTS_NORMAL);
        Player player(&env.sim(), 1);
        RelicModel relic = RelicCmd::Obtain(RelicId::PAELS_EYE, player);
        PaelsEye eye(&env.sim(), relic.Index());
        CardModel card = player.PlayerCombatState().Hand().Cards()[0];
        CardPlay play{card, player};
        CombatManager(&env.sim()).History().CardPlayFinished(CombatState(&env.sim()), play);
        require(eye.AnyCardsPlayedThisTurn() && !eye.ShouldTakeExtraTurn(player),
                "manual card play did not suppress Eye's extra turn");
    }
}
