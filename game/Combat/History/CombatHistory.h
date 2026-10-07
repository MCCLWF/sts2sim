// Combat/History/CombatHistory.cs.
//
// The history is written everywhere but only matters where a model reads it.
// Only history needed by currently ported readers is stored in POD state.
#pragma once

#include <cstdint>

#include "engine/sim/state.h"
#include "engine/sim/sim.h"
#include "game/Entities/Cards/CardPlay.h"
#include "game/fwd.h"
#include "game/Models/AfflictionModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

class CombatHistory {
public:
    CombatHistory() = default;
    explicit CombatHistory(Sim* sim) : sim_(sim) {}

    void Clear() {
        if (sim_) {
            sim_->g->combat.player_card_block_entries_this_turn = 0;
            ++sim_->g->combat.player_card_block_history_epoch;
        }
    }
    void CardEffectStarted(const CardPlay& play) {
        if (auto* boundary = sim_->execution_trace) {
            ExecutionEvent event;
            event.kind = ExecutionEventKind::CardEffectStarted;
            event.card = play.Card.Index();
            event.card_id = static_cast<uint16_t>(play.Card.CardId_());
            event.owner = play.Player ? play.Player.Index() : 0;
            FillCard(event, play.Card);
            FillPlay(event, play);
            boundary->Record(event);
        }
    }
    void CardPlayStarted(const CombatState&, const CardPlay& play) {
        if (auto* boundary = sim_->execution_trace) {
            ExecutionEvent event;
            event.kind = ExecutionEventKind::CardPlayStarted;
            event.card = play.Card.Index();
            event.card_id = static_cast<uint16_t>(play.Card.CardId_());
            event.owner = play.Player ? play.Player.Index() : 0;
            FillCard(event, play.Card);
            FillPlay(event, play);
            boundary->Record(event);
        }
        if (play.Card.Type() == CardType::Skill) sim_->g->combat.player_played_skill_this_turn = true;
        if (play.Card.Type() == CardType::Attack) sim_->g->combat.player_attack_cards_started_this_turn = list_count_add(sim_->g->combat.player_attack_cards_started_this_turn);
        if (play.Card.Type() == CardType::Attack || play.Card.Type() == CardType::Skill)
            sim_->g->combat.player_attack_skill_cards_started_this_turn = list_count_add(sim_->g->combat.player_attack_skill_cards_started_this_turn);
        if (play.Player) {
            sim_->g->combat.player_all_cards_started_this_turn = list_count_add(sim_->g->combat.player_all_cards_started_this_turn);
            if (play.IsFirstInSeries()) sim_->g->combat.player_cards_started_this_turn = list_count_add(sim_->g->combat.player_cards_started_this_turn);
            if (play.Card.Type() == CardType::Attack && play.Resources.EnergyValue == 0)
                sim_->g->combat.player_zero_cost_attacks_started_this_turn = list_count_add(sim_->g->combat.player_zero_cost_attacks_started_this_turn);
        }
    }
    void CardPlayFinished(const CombatState&, const CardPlay& play) {
        if (auto* boundary = sim_->execution_trace) {
            ExecutionEvent event;
            event.kind = ExecutionEventKind::CardPlayFinished;
            event.card = play.Card.Index();
            event.card_id = static_cast<uint16_t>(play.Card.CardId_());
            event.owner = play.Player ? play.Player.Index() : 0;
            FillCard(event, play.Card);
            FillPlay(event, play);
            event.flag = play.Card.Keywords().Contains(CardKeyword::Ethereal);
            boundary->Record(event);
        }
        sim_->g->combat.cards_played_this_combat = list_count_add(sim_->g->combat.cards_played_this_combat);
        if (play.Player) {
            if (play.Card.Keywords().Contains(CardKeyword::Ethereal))
                sim_->g->combat.player_ethereal_cards_played_this_combat = list_count_add(sim_->g->combat.player_ethereal_cards_played_this_combat);
            sim_->g->combat.player_cards_finished_this_turn = list_count_add(sim_->g->combat.player_cards_finished_this_turn);
            if (play.Card.Type() == CardType::Attack) sim_->g->combat.player_attack_cards_this_turn = list_count_add(sim_->g->combat.player_attack_cards_this_turn);
            if (play.Card.Type() == CardType::Skill) sim_->g->combat.player_skill_cards_finished_this_turn = list_count_add(sim_->g->combat.player_skill_cards_finished_this_turn);
            if (play.Card.CardId_() == CardId::SHIV) sim_->g->combat.player_shiv_cards_finished_this_turn = list_count_add(sim_->g->combat.player_shiv_cards_finished_this_turn);
        }
        if (play.Player && play.Player.PlayerCombatState()) {
            play.Card.st().last_played_turn = play.Player.PlayerCombatState().TurnNumber();
            if (!play.IsAutoPlay)
                sim_->g->combat.player_last_non_auto_card_play_turn = play.Player.PlayerCombatState().TurnNumber();
            if (play.Card.Type() == CardType::Attack && !play.Card.IsDupe()) {
                auto& history = sim_->g->combat;
                const int32_t turn = play.Player.PlayerCombatState().TurnNumber();
                if (history.player_last_nondupe_attack_turn != turn) {
                    history.player_previous_nondupe_attack_card = history.player_last_nondupe_attack_card;
                    history.player_previous_nondupe_attack_turn = history.player_last_nondupe_attack_turn;
                }
                history.player_last_nondupe_attack_card = play.Card.Index();
                history.player_last_nondupe_attack_turn = turn;
            }
        }
    }
    void DamageReceived(const CombatState&, const Creature& target, const Creature& dealer, const DamageResult& result, const CardModel& card) {
        if (auto* boundary = sim_->execution_trace) {
            ExecutionEvent event;
            event.kind = ExecutionEventKind::DamageReceived;
            event.card = card ? card.Index() : 0;
            event.card_id = card ? static_cast<uint16_t>(card.CardId_()) : 0;
            if (card) FillCard(event, card);
            event.creature = target.Index();
            event.dealer = dealer ? dealer.Index() : 0;
            event.blocked = result.BlockedDamage;
            event.unblocked = result.UnblockedDamage;
            event.overkill = result.OverkillDamage;
            event.props = static_cast<int32_t>(result.Props);
            event.block_broken = result.WasBlockBroken;
            event.fully_blocked = result.WasFullyBlocked;
            event.target_killed = result.WasTargetKilled;
            event.damage_source_kind = sim_->damage_source_kind;
            event.damage_source_id = sim_->damage_source_id;
            if (event.damage_source_kind == ExecutionDamageSourceKind::Unknown && card) {
                event.damage_source_kind = ExecutionDamageSourceKind::Card;
                event.damage_source_id = card.data().entry;
            }
            boundary->Record(event);
        }
        if (dealer && IsPoweredAttack(result.Props))
            sim_->g->creatures[target.Index()].RecordPoweredDamage(dealer.Index());
        if (target.IsPlayer() && result.UnblockedDamage > 0) {
            sim_->g->combat.player_damage_received_entries_this_combat = list_count_add(sim_->g->combat.player_damage_received_entries_this_combat);
            sim_->g->combat.player_lost_hp_this_turn = true;
        }
        if (target.IsPlayer() && !result.WasFullyBlocked)
            sim_->g->combat.player_nonfully_blocked_damage_this_turn = true;
    }
    void BlockGained(const CombatState& state, const Creature&, int32_t, ValueProp props, const CardPlay* play) {
        if (!play || !play->Player || !play->Player.Creature().IsPlayer() || !IsCardOrMonsterMove(props)) return;
        auto& combat = sim_->g->combat;
        const int32_t turn = play->Player.PlayerCombatState().TurnNumber();
        if (combat.player_card_block_history_round != state.RoundNumber() ||
            combat.player_card_block_history_side != state.CurrentSide() || combat.player_card_block_history_turn != turn) {
            combat.player_card_block_entries_this_turn = 0;
            combat.player_card_block_history_round = state.RoundNumber();
            combat.player_card_block_history_side = state.CurrentSide();
            combat.player_card_block_history_turn = turn;
        }
        combat.player_card_block_entries_this_turn = list_count_add(combat.player_card_block_entries_this_turn);
        play->RecordBlockEntry(state.RoundNumber(), state.CurrentSide(), turn, combat.player_card_block_history_epoch);
    }
    void EnergySpent(const CombatState&, int32_t amount, const Player& player) {
        if (player) sim_->g->combat.player_energy_spent_this_turn += amount;
    }
    void StarsModified(const CombatState&, int32_t amount, const Player& player) {
        if (player && amount > 0) sim_->g->combat.player_stars_gained_this_turn += amount;
    }
    void OrbChanneled(const CombatState&, const OrbModel& orb) {
        if (orb.Owner() && orb.OrbId_() == OrbId::LIGHTNING_ORB)
            sim_->g->combat.player_lightning_orbs_channeled_this_combat = list_count_add(sim_->g->combat.player_lightning_orbs_channeled_this_combat);
    }
    void MonsterPerformedMove(const CombatState&, const MonsterModel&) {}
    void PowerReceived(const CombatState&, const PowerModel& power, Dec, const Creature& applier) {
        if (power.PowerId_() == PowerId::DOOM_POWER && applier.IsPlayer())
            sim_->g->combat.player_applied_doom_this_turn = true;
    }
    int32_t CardDrawn(const CombatState&, const CardModel& card, bool fromHandDraw) {
        if (card.Owner() && card.Owner().Creature().IsPlayer()) {
            sim_->g->combat.player_cards_drawn_this_combat = list_count_add(sim_->g->combat.player_cards_drawn_this_combat);
            if (!fromHandDraw) sim_->g->combat.player_cards_drawn_this_turn = list_count_add(sim_->g->combat.player_cards_drawn_this_turn);
            if (card.Type() == CardType::Status) sim_->g->combat.player_status_cards_drawn_this_turn = list_count_add(sim_->g->combat.player_status_cards_drawn_this_turn);
        }
        if (auto* boundary = sim_->execution_trace) {
            ExecutionEvent event;
            event.kind = ExecutionEventKind::CardDrawn;
            event.card = card.Index();
            event.card_id = static_cast<uint16_t>(card.CardId_());
            FillCard(event, card);
            event.owner = card.Owner() ? card.Owner().Index() : 0;
            event.flag = fromHandDraw;
            return boundary->Record(event);
        }
        return -1;
    }
    void CardDrawResolved(const CardModel& card, int32_t originalIndex) {
        if (auto* boundary = sim_->execution_trace) {
            ExecutionEvent event;
            event.kind = ExecutionEventKind::CardDrawResolved;
            event.card = card.Index();
            event.card_id = static_cast<uint16_t>(card.CardId_());
            FillCard(event, card);
            event.owner = card.Owner() ? card.Owner().Index() : 0;
            event.original_index = originalIndex;
            boundary->Record(event);
        }
    }
    void CardExhausted(const CombatState&, const CardModel& card) {
        sim_->g->combat.cards_exhausted_this_combat = list_count_add(sim_->g->combat.cards_exhausted_this_combat);
        if (card.Owner() && card.Owner().Creature().IsPlayer()) sim_->g->combat.player_cards_exhausted_this_turn = list_count_add(sim_->g->combat.player_cards_exhausted_this_turn);
    }
    void CardDiscarded(const CombatState&, const CardModel& card) {
        if (card.Owner() && card.Owner().Creature().IsPlayer()) sim_->g->combat.player_cards_discarded_this_turn = list_count_add(sim_->g->combat.player_cards_discarded_this_turn);
    }
    int32_t CardGenerated(const CombatState&, const CardModel& card, const Player& creator) {
        if (creator) sim_->g->combat.player_cards_generated_this_combat = list_count_add(sim_->g->combat.player_cards_generated_this_combat);
        if (auto* boundary = sim_->execution_trace) {
            ExecutionEvent event;
            event.kind = ExecutionEventKind::CardGenerated;
            event.card = card.Index();
            event.card_id = static_cast<uint16_t>(card.CardId_());
            FillCard(event, card);
            event.owner = creator ? creator.Index() : 0;
            return boundary->Record(event);
        }
        return -1;
    }
    void CardGenerationResolved(const CardModel& card, int32_t originalIndex) {
        if (auto* boundary = sim_->execution_trace) {
            ExecutionEvent event;
            event.kind = ExecutionEventKind::CardGenerationResolved;
            event.card = card.Index();
            event.card_id = static_cast<uint16_t>(card.CardId_());
            FillCard(event, card);
            event.owner = card.Owner() ? card.Owner().Index() : 0;
            event.original_index = originalIndex;
            boundary->Record(event);
        }
    }
    void CardAfflicted(const CombatState& state, const CardModel& card, const AfflictionModel& affliction) {
        if (state.CurrentSide() == CombatSide::Player && card.Owner().Creature().IsPlayer() &&
            affliction.AfflictionId_() == AfflictionId::BOUND) sim_->g->combat.player_bound_cards_afflicted_this_turn = list_count_add(sim_->g->combat.player_bound_cards_afflicted_this_turn);
    }

private:
    static void FillCard(ExecutionEvent& event, const CardModel& card) {
        event.card_facts = ExecutionCardFacts{
            static_cast<uint16_t>(card.Owner() ? card.Owner().Index() : 0),
            card.CurrentUpgradeLevel(), static_cast<uint8_t>(card.Type())};
    }
    static void FillPlay(ExecutionEvent& event, const CardPlay& play) {
        event.play_facts = ExecutionPlayFacts{
            static_cast<uint32_t>(play.Target ? play.Target.Index() : 0),
            static_cast<uint8_t>(play.ResultPile),
            play.Resources.EnergySpent, play.Resources.EnergyValue,
            play.Resources.StarsSpent, play.Resources.StarValue,
            play.IsAutoPlay, play.PlayIndex, play.PlayCount, static_cast<uint8_t>(play.Card.TargetType())};
    }
    Sim* sim_ = nullptr;
};

}  // namespace sts2
