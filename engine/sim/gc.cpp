// Pool reclamation (see gc.h).
#include "engine/sim/gc.h"

#include <vector>

namespace sts2 {
namespace {
class Marks {
public:
    explicit Marks(size_t count) : values_(count) {}
    void set(size_t index) { if (index >= values_.size()) throw GameError("GC root outside pool"); values_[index] = 1; }
    bool operator[](size_t index) const { return index < values_.size() && values_[index]; }
    friend bool operator==(const Marks&, const Marks&) = default;
private:
    std::vector<uint8_t> values_;
};
} }

namespace sts2 {

namespace {

template <class T, uint16_t N>
int Sweep(Pool<T, N>& pool, const Marks& marked) {
    int freed = 0;
    for (uint32_t i = 1; i <= pool.capacity(); ++i) {
        if (pool.live(i) && !marked[i]) {
            pool.free(i);
            ++freed;
        }
    }
    return freed;
}

}  // namespace

int CollectGarbage(GameState& g) {
    if (g.combat.exists) return 0;
    Marks cards(size_t{g.cards.capacity()} + 1);
    Marks powers(size_t{g.powers.capacity()} + 1);
    Marks creatures(size_t{g.creatures.capacity()} + 1);
    Marks relics(size_t{g.relics.capacity()} + 1);
    Marks potions(size_t{g.potions.capacity()} + 1);
    Marks rewards(size_t{g.rewards.capacity()} + 1);
    Marks orbs(size_t{g.orbs.capacity()} + 1);
    Marks afflictions(size_t{g.afflictions.capacity()} + 1);

    const PlayerData& p = g.run.player;
    for (CardRef c : p.deck) cards.set(c);
    for (uint32_t ref = 1; ref <= g.powers.capacity(); ++ref)
        if (g.powers.live(ref)) for (const auto& entry : g.powers[ref].dampen_downgraded_cards) cards.set(entry.card);
    for (RelicRef r : p.relics) {
        relics.set(r);
        if (g.relics[r].id == RelicId::PAELS_TOOTH)
            for (CardRef card : g.relics[r].paels_tooth_cards) cards.set(card);
    }
    // BingBong's identity set is a strong reference, including cards stolen
    // from the deck which may later be returned by a SpecialCardReward.
    for (CardRef ref = 1; ref <= g.cards.capacity(); ++ref) {
        if (!g.cards.live(ref)) continue;
        RelicRef& skip = g.cards[ref].bing_bong_skip_relic;
        if (!skip) continue;
        if (skip <= g.relics.capacity() && relics[skip] && g.relics[skip].id == RelicId::BING_BONG)
            cards.set(ref);
        else
            skip = 0;  // The containing relic object is being reclaimed.
    }
    for (int i = 0; i < p.max_potion_slots; ++i) {
        if (p.potions[i] != 0) potions.set(p.potions[i]);
    }
    if (p.creature != 0) {
        creatures.set(p.creature);
        for (PowerRef pw : g.creatures[p.creature].powers) powers.set(pw);
    }
    for (const auto& pile : g.player_combat.piles) {
        for (CardRef c : pile) cards.set(c);
    }
    for (OrbRef o : g.player_combat.orbs) orbs.set(o);
    if (g.run.rewards.active) {
        for (RewardRef r : g.run.rewards.rewards) {
            rewards.set(r);
            const RewardState& s = g.rewards[r];
            for (CardRef c : s.cards) cards.set(c);
            if (s.potion != 0) potions.set(s.potion);
            if (s.relic != 0) relics.set(s.relic);
        }
    }
    const MerchantState& shop = g.run.merchant;
    if (shop.exists) {
        for (const auto& e : shop.character_cards) cards.set(e.card);
        for (const auto& e : shop.colorless_cards) cards.set(e.card);
        for (const auto& e : shop.relics) relics.set(e.relic);
        for (const auto& e : shop.potions) potions.set(e.potion);
    }
    // An AfflictionModel is owned by its card. Mark it only after every card
    // root (deck, pile, reward, shop, and retained relic card) is reachable;
    // otherwise sweeping an unreachable card would leave a live affliction
    // with a dangling Card reference.
    for (CardRef c = 1; c <= g.cards.capacity(); ++c)
        if (cards[c] && g.cards.live(c) && g.cards[c].affliction_instance
            && g.afflictions.live(g.cards[c].affliction_instance))
            afflictions.set(g.cards[c].affliction_instance);
    // Deck cards refer to nothing else; clone_of / deck_version only link
    // combat copies to deck cards.
    return Sweep(g.cards, cards) + Sweep(g.powers, powers) + Sweep(g.creatures, creatures) + Sweep(g.relics, relics) +
           Sweep(g.potions, potions) + Sweep(g.rewards, rewards) + Sweep(g.orbs, orbs) + Sweep(g.afflictions, afflictions);
}

int CollectDetachedCreatures(GameState& g) {
    if (!g.combat.exists || g.combat.executing_card_or_potion_effect != 0 ||
        (g.creatures.size() < kCreaturePool - 4 && g.powers.size() < kPowerPool - 16
            && g.afflictions.size() < kAfflictionPool - 16))
        return 0;
    Marks creatures(size_t{g.creatures.capacity()} + 1);
    Marks powers(size_t{g.powers.capacity()} + 1);
    Marks cards(size_t{g.cards.capacity()} + 1);
    Marks afflictions(size_t{g.afflictions.capacity()} + 1);
    auto mark = [&](CreatureRef ref) {
        if (g.creatures.live(ref)) creatures.set(ref);
    };
    auto mark_power = [&](PowerRef ref) {
        if (g.powers.live(ref)) powers.set(ref);
    };
    mark(g.run.player.creature);
    for (CardRef c : g.run.player.deck) cards.set(c);
    for (const auto& pile : g.player_combat.piles)
        for (CardRef c : pile) cards.set(c);
    for (uint32_t ref = 1; ref <= g.relics.capacity(); ++ref) {
        if (!g.relics.live(ref)) continue;
        const RelicState& relic = g.relics[ref];
        if (relic.id == RelicId::PAELS_TOOTH)
            for (CardRef c : relic.paels_tooth_cards) cards.set(c);
        if (relic.id == RelicId::MUSIC_BOX && relic.extra[1] > 0)
            cards.set(static_cast<CardRef>(relic.extra[1]));
    }
    if (g.combat.player_last_nondupe_attack_card)
        cards.set(g.combat.player_last_nondupe_attack_card);
    if (g.combat.player_previous_nondupe_attack_card)
        cards.set(g.combat.player_previous_nondupe_attack_card);
    // A pending CardPlay capture retains its card until the enclosing play
    // finishes, even if that card has left its visible pile.
    for (const auto& capture : g.combat.active_card_play_power_captures)
        if (capture.card != 0) cards.set(capture.card);
    for (uint32_t ref = 1; ref <= g.powers.capacity(); ++ref)
        if (g.powers.live(ref)) for (const auto& entry : g.powers[ref].dampen_downgraded_cards) cards.set(entry.card);
    for (CardRef c = 1; c <= g.cards.capacity(); ++c)
        if (cards[c] && g.cards.live(c) && g.cards[c].affliction_instance
            && g.afflictions.live(g.cards[c].affliction_instance))
            afflictions.set(g.cards[c].affliction_instance);
    for (CreatureRef ref : g.combat.allies) mark(ref);
    for (CreatureRef ref : g.combat.enemies) mark(ref);
    for (CreatureRef ref : g.combat.escaped) mark(ref);
    for (CreatureRef ref : g.player_combat.pets) mark(ref);
    for (uint32_t ref = 1; ref <= g.creatures.capacity(); ++ref)
        if (g.creatures.live(ref) && g.creatures[ref].attached) mark(ref);
    // A retained card target is also a root. Include it before traversing
    // powers, since that target may itself keep another creature alive.
    for (uint32_t ref = 1; ref <= g.cards.capacity(); ++ref)
        if (g.cards.live(ref)) mark(g.cards[ref].current_target);
    for (const auto& capture : g.combat.active_card_play_power_captures)
        mark_power(capture.power);
    // UnsettlingLamp remembers its first doubled power until combat ends,
    // even after that power has left its owner's active powers list.
    for (uint32_t ref = 1; ref <= g.relics.capacity(); ++ref)
        if (g.relics.live(ref) && g.relics[ref].id == RelicId::UNSETTLING_LAMP && g.relics[ref].extra[1] > 0)
            mark_power(static_cast<PowerRef>(g.relics[ref].extra[1]));
    // Active powers can retain the creature that applied them or a separate
    // target, including a monster already removed from the enemy list.
    bool changed;
    do {
        const auto creatures_before = creatures;
        const auto powers_before = powers;
        for (uint32_t ref = 1; ref <= g.creatures.capacity(); ++ref) {
            if (!creatures[ref]) continue;
            const CreatureState& creature = g.creatures[ref];
            for (const auto& entry : creature.powered_damage_this_turn) mark(entry.dealer);
            // Queen._amalgam is represented by monster.extra[1].
            if (creature.monster.id == MonsterId::QUEEN && creature.monster.extra[1] > 0)
                mark(static_cast<CreatureRef>(creature.monster.extra[1]));
            for (PowerRef power : creature.powers) mark_power(power);
        }
        for (uint32_t ref = 1; ref <= g.powers.capacity(); ++ref) {
            if (!powers[ref]) continue;
            const PowerState& power = g.powers[ref];
            mark(power.owner);
            mark(power.applier);
            mark(power.target);
            for (CreatureRef caster : power.dampen_casters) mark(caster);
        }
        changed = creatures != creatures_before || powers != powers_before;
    } while (changed);
    // RemoveInternal deliberately preserves the object while hooks execute.
    // Once the action completes, retired powers without a retained reference
    // are garbage, including those whose owner is still in combat.
    return Sweep(g.powers, powers) + Sweep(g.creatures, creatures) + Sweep(g.afflictions, afflictions);
}

}  // namespace sts2
