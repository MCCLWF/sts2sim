#include <stdexcept>

#include "env/env.h"
#include "game/All.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Models/Relics/PaelsLegion.h"

using namespace sts2;

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    Env env;
    env.ResetCombat(NewRunOptions{.seed = "PAELLEGIONTEST", .character = CharacterId::IRONCLAD},
                    EncounterId::CULTISTS_NORMAL);
    Player player(&env.sim(), 1);
    RelicModel relic = RelicCmd::Obtain(RelicId::PAELS_LEGION, player);
    PaelsLegionRelic legion(&env.sim(), relic.Index());
    Creature pet = player.PlayerCombatState().GetPet(MonsterId::PAELS_LEGION);
    require(pet && pet.CurrentHp() == 9999 && pet.Side() == CombatSide::Player,
            "Pael's Legion did not summon its pet when obtained in combat");
    require(!legion.ShowCounter() && legion.DisplayAmount() == -1, "initial counter visible");

    CardModel defend;
    for (CardModel card : player.Deck().Cards()) {
        if (card.CardId_() == CardId::DEFEND_IRONCLAD) { defend = card; break; }
    }
    require(static_cast<bool>(defend), "missing starting Defend");
    CardPlay play{defend, player};
    Dec first = CreatureCmd::GainBlock(player.Creature(), Dec(5), ValueProp::Move, &play);
    require(first == Dec(10) && legion.st().extra[1] == defend.Index(), "ready block not doubled");
    PlayerChoiceContext context = PlayerChoiceContext::Throwing();
    legion.AfterCardPlayed(context, &play);
    require(legion.DisplayAmount() == 2 && legion.ShowCounter() &&
                legion.Status() == RelicStatus::Normal, "cooldown not started after card play");

    Dec cooldownBlock = CreatureCmd::GainBlock(player.Creature(), Dec(5), ValueProp::Move, &play);
    require(cooldownBlock == Dec(5), "block doubled during cooldown");
    CombatState combat(&env.sim());
    legion.AfterSideTurnStart(CombatSide::Player, combat.PlayerCreatures(), combat);
    require(legion.DisplayAmount() == 1, "first cooldown tick incorrect");
    legion.AfterSideTurnStart(CombatSide::Player, combat.PlayerCreatures(), combat);
    require(legion.DisplayAmount() == -1 && legion.Status() == RelicStatus::Active,
            "relic did not wake after two owner turns");
    Dec readyAgain = CreatureCmd::GainBlock(player.Creature(), Dec(5), ValueProp::Move, &play);
    require(readyAgain == Dec(10), "block not doubled after cooldown");
    legion.AfterCombatEnd(CombatRoom{});
    require(legion.st().extra[0] == 0 && legion.st().extra[1] == 0 &&
                legion.Status() == RelicStatus::Normal, "combat cleanup incomplete");
}
