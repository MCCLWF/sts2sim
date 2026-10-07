// Shared structural combat inspection for ordinary and prediction sessions.
// It reads a private branch when used by the prediction API.
#pragma once

#include <string>

#include "engine/json.h"
#include "env/env.h"
#include "game/All.h"

namespace sts2 {
inline void write_combat_root(sts2::JsonWriter& writer, sts2::Env& env) {
    sts2::Sim* sim = &env.sim();
    sts2::Player player(sim, 1);
    sts2::PlayerCombatState combat = player.PlayerCombatState();
    auto write_cards = [&](const char* key, sts2::CardPile pile) {
        writer.key(key).begin_array();
        for (sts2::CardModel card : pile.Cards()) writer.value(sts2::name(card.CardId_()));
        writer.end_array();
        writer.key(std::string(key) + "_states").begin_array();
        for (sts2::CardModel card : pile.Cards()) {
            sts2::EnchantmentModel enchantment = card.Enchantment();
            sts2::AfflictionModel affliction = card.Affliction();
            std::string signature = std::string(sts2::name(card.CardId_())) + "+"
                + std::to_string(card.CurrentUpgradeLevel()) + "/"
                + (card.EnergyCost().CostsX() ? "1" : "0") + ":"
                + std::to_string(card.EnergyCost().GetWithModifiers(sts2::CostModifiers::Local)) + "/"
                + (enchantment ? std::string(sts2::name(enchantment.EnchantmentId_())) : "-") + ":"
                + std::to_string(enchantment ? enchantment.Amount() : 0) + "/"
                + (affliction ? std::string(sts2::name(affliction.AfflictionId_())) : "-") + ":"
                + std::to_string(affliction ? affliction.Amount() : 0);
            writer.value(signature);
        }
        writer.end_array();
    };
    auto write_powers = [&](const char* key, sts2::Creature creature) {
        writer.key(key).begin_array();
        for (sts2::PowerModel power : creature.Powers()) {
            writer.value(std::string(sts2::name(power.PowerId_())) + ":" + std::to_string(power.Amount()));
        }
        writer.end_array();
    };
    writer.key("root").begin_object();
    write_cards("hand", combat.Hand());
    write_cards("draw", combat.DrawPile());
    write_cards("discard", combat.DiscardPile());
    write_cards("exhaust", combat.ExhaustPile());
    writer.key("relics").begin_array();
    for (sts2::RelicModel relic : player.Relics()) {
        writer.value(std::string(sts2::name(relic.RelicId_())) + (relic.IsMelted() ? ":1" : ":0"));
    }
    writer.end_array();
    writer.key("potions").begin_array();
    for (sts2::PotionModel potion : player.PotionSlots()) {
        writer.value(potion ? sts2::name(potion.PotionId_()) : std::string_view{});
    }
    writer.end_array();
    write_powers("player_powers", player.Creature());
    writer.key("enemies").begin_array();
    for (sts2::Creature enemy : sts2::CombatState(sim).Enemies()) {
        writer.begin_object().field("id", enemy.CombatId())
            .field("monster", sts2::name(enemy.Monster().MonsterId_()))
            .field("hp", enemy.CurrentHp()).field("max_hp", enemy.MaxHp())
            .field("block", enemy.Block());
        write_powers("powers", enemy);
        writer.end_object();
    }
    writer.end_array();
    writer.field("energy", combat.Energy()).field("hp", player.Creature().CurrentHp())
        .field("max_hp", player.Creature().MaxHp()).field("player_block", player.Creature().Block()).end_object();
}

}  // namespace sts2
