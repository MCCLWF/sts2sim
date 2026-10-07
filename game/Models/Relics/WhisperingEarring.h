// Models/Relics/WhisperingEarring.cs (v0.111.0).
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Entities/Cards/CardPile.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct WhisperingEarring : RelicModel {
    SIM_MODEL(WhisperingEarring, RelicModel)

    static constexpr int kMaxCardsToPlay = 13;

    Dec ModifyMaxEnergy(Player player, Dec amount) const {
        return player == Owner() ? amount + DynamicVars().Energy().BaseValue() : amount;
    }

    void AfterAutoPrePlayPhaseEnteredLate(PlayerChoiceContext& choiceContext, Player player) const {
        if (player != Owner() || player.PlayerCombatState().TurnNumber() > 1) return;

        CombatState combatState = player.Creature().CombatState();
        CardSelectCmd::ScopedDeterministicSelector selector(&player.sim());
        const int startTurn = player.PlayerCombatState().TurnNumber();
        for (int cardsPlayed = 0; cardsPlayed < kMaxCardsToPlay; ++cardsPlayed) {
            if (CombatManager(sim_).IsOverOrEnding()) break;
            if (sim_->g->combat.player_ready_to_end_turn) break;
            if (player.PlayerCombatState().TurnNumber() != startTurn) break;

            CardModel card;
            for (CardModel candidate : player.PlayerCombatState().Hand().Cards()) {
                if (candidate.CanPlay()) {
                    card = candidate;
                    break;
                }
            }
            if (!card) break;

            Creature target;
            switch (card.TargetType()) {
            case TargetType::AnyEnemy: {
                CreatureList enemies = combatState.HittableEnemies();
                if (!enemies.empty()) target = enemies[0];
                break;
            }
            case TargetType::AnyAlly: {
                CreatureList allies;
                for (Creature candidate : combatState.Allies()) {
                    if (candidate && candidate.IsAlive() && candidate.IsPlayer() && candidate != player.Creature())
                        allies.push_back(candidate);
                }
                target = player.RunState().Rng().CombatTargets().NextItem(allies);
                break;
            }
            case TargetType::AnyPlayer:
                target = player.Creature();
                break;
            default:
                break;
            }

            card.SpendResources();
            CardCmd::AutoPlay(choiceContext, card, target, AutoPlayType::Default, /*skipXCapture=*/true);
        }
        // Flash and TalkCmd only affect presentation in the original game.
    }
};

}  // namespace sts2
