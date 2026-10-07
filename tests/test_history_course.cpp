#include <stdexcept>

#include "env/env.h"
#include "game/All.h"
#include "game/Combat/CombatManager.h"
#include "game/Combat/History/CombatHistory.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Hooks/Hook.h"

using namespace sts2;

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

int main() {
    Env env;
    env.ResetCombat(NewRunOptions{.seed = "HISTORYCOURSE", .character = CharacterId::IRONCLAD},
                    EncounterId::CULTISTS_NORMAL);
    Player player(&env.sim(), 1);
    CombatState combat(&env.sim());
    require(!combat.Enemies().empty(), "History Course test needs living enemies");
    Creature enemy = combat.Enemies()[0];
    RelicCmd::Obtain(RelicId::HISTORY_COURSE, player);

    CardModel strike = player.RunState().CreateCard(CardId::STRIKE_IRONCLAD, player);
    CardPileCmd::Add(strike, PileType::Discard);
    CardPlay play{strike, player, enemy, PileType::Discard};
    CombatManager(&env.sim()).History().CardPlayFinished(combat, play);
    require(env.state().combat.player_last_nondupe_attack_card == strike.Index() &&
                env.state().combat.player_last_nondupe_attack_turn == 1,
            "CardPlayFinished did not retain the latest eligible Attack for History Course");

    // The previous-turn history fact, FreedRepy and card reference must
    // survive an owned Env snapshot before the automatic replay starts.
    env.state().run.freed_repy = true;
    player.PlayerCombatState().IncrementTurnNumber();
    // Hellraiser can finish an Attack during the new turn's initial draw,
    // before History Course queries the previous player's turn. The native
    // full-run regression executes this ordering with Twin Strike.
    CardModel current = combat.CreateCard(CardId::TWIN_STRIKE, player);
    CardPileCmd::Add(current, PileType::Discard);
    CardPlay current_play{current, player, enemy, PileType::Discard};
    current_play.IsAutoPlay = true;
    CombatManager(&env.sim()).History().CardPlayFinished(combat, current_play);
    const std::string snapshot = env.ExportSnapshotJson();
    auto restored = Env::ImportSnapshotJson(snapshot);
    require(restored->state().run.freed_repy &&
                restored->state().combat.player_last_nondupe_attack_card == current.Index() &&
                restored->state().combat.player_last_nondupe_attack_turn == 2 &&
                restored->state().combat.player_previous_nondupe_attack_card == strike.Index() &&
                restored->state().combat.player_previous_nondupe_attack_turn == 1 &&
                restored->ExportSnapshotJson() == snapshot,
            "History Course or Repy run facts failed snapshot round-trip");

    PlayerChoiceContext context = PlayerChoiceContext::Throwing();
    int hp = 0;
    for (Creature creature : combat.Enemies()) hp += creature.CurrentHp();
    Hook::AfterAutoPrePlayPhaseEntered(context, combat, player);
    int hp_after = 0;
    for (Creature creature : combat.Enemies()) hp_after += creature.CurrentHp();
    require(hp_after < hp && env.state().combat.cards_played_this_combat == 3 &&
                env.state().combat.player_last_nondupe_attack_card == current.Index() &&
                env.state().combat.player_previous_nondupe_attack_card == strike.Index(),
            "History Course did not replay last turn's non-dupe Attack exactly once");
    // An intervening turn with no eligible Attack must not replay an older
    // entry. Dupe completion from History Course does not replace either fact.
    player.PlayerCombatState().IncrementTurnNumber();
    Hook::AfterAutoPrePlayPhaseEntered(context, combat, player);
    require(env.state().combat.cards_played_this_combat == 4,
            "History Course did not select turn two's latest Attack on turn three");
    player.PlayerCombatState().IncrementTurnNumber();
    const int32_t finished = env.state().combat.cards_played_this_combat;
    Hook::AfterAutoPrePlayPhaseEntered(context, combat, player);
    require(env.state().combat.cards_played_this_combat == finished,
            "History Course replayed an Attack from before the previous turn");
}
