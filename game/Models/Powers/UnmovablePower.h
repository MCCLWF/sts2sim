#pragma once
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"
namespace sts2 {
struct UnmovablePower : PowerModel { SIM_MODEL(UnmovablePower, PowerModel)
    Dec ModifyBlockMultiplicative(Creature target, Dec, ValueProp props, CardModel card, const CardPlay* play) const {
        if (target.IsMonster() || !IsCardOrMonsterMove(props) || (card && card.Owner().Creature() != Owner())) return Dec(1);
        const sts2::CombatState state = Owner().CombatState();
        const auto& combat = sim().g->combat;
        const int32_t turn = Owner().IsPlayer() ? Owner().Player().PlayerCombatState().TurnNumber() : -1;
        const int32_t ownEntries = play && play->Player && play->Player.Creature() == Owner()
            ? play->BlockEntriesAt(state.RoundNumber(), state.CurrentSide(), turn, combat.player_card_block_history_epoch) : 0;
        const int32_t totalEntries = Owner().IsPlayer() && combat.player_card_block_history_round == state.RoundNumber() &&
            combat.player_card_block_history_side == state.CurrentSide() && combat.player_card_block_history_turn == turn
            ? combat.player_card_block_entries_this_turn : 0;
        const int32_t otherEntries = totalEntries - ownEntries;
        return otherEntries >= Amount() ? Dec(1) : Dec(2);
    }
}; }
