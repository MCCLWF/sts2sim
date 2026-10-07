// Runs/RunState.cs
#include "game/Runs/RunState.h"

#include "engine/sim/alloc.h"
#include "game/All.h"

#include <algorithm>

namespace sts2 {

namespace {
constexpr uint8_t kPlayer = 1;  // single player: player index + 1
}  // namespace

StaticVec<Player, 4> RunState::Players() const {
    StaticVec<Player, 4> players;
    const uint8_t count = std::clamp<uint8_t>(st().player_count, 1, 4);
    for (uint8_t i = 0; i < count; ++i) players.push_back(Player(sim_, static_cast<uint8_t>(kPlayer + i)));
    return players;
}

int32_t RunState::CurrentMapCol() const {
    return st().visited_map_coords.empty() ? -1 : st().visited_map_coords.back().col;
}
int32_t RunState::CurrentMapRow() const {
    return st().visited_map_coords.empty() ? -1 : st().visited_map_coords.back().row;
}

bool RunState::AddVisitedMapCoord(MapCoord coord) {
    if (st().visited_map_coords.contains(coord)) return false;
    st().visited_map_coords.push_back(coord);
    return true;  // NextRoomId: networking only
}

// CreateCard(canonicalCard, owner): ToMutable, AddCard, AfterCreated.
CardModel RunState::CreateCard(CardId canonicalCard, Player owner) {
    CardModel card(sim_, alloc_card(*sim_->g, canonicalCard, 0));
    AddCard(card, owner);
    card.AfterCreated();
    return card;
}

CardModel RunState::CloneCard(CardModel mutableCard) {
    CardRef ref = sim_->g->cards.alloc();
    CardState& state = sim_->g->cards[ref];
    state = mutableCard.st();
    if (state.affliction_instance)
        state.affliction_instance = clone_affliction(*sim_->g, state.affliction_instance, ref);
    state.bing_bong_skip_relic = 0;  // HashSet membership is object identity, not a model property.
    state.deck_version = 0;  // CardModel.AfterCloned clears the persistent deck link.
    state.current_target = 0;
    state.current_play_index = 0;
    state.removed = false;
    // A run clone is a new CardModel and has no CardPlayFinishedEntry.
    state.last_played_turn = -1;
    state.pile = PileType::None;
    state.in_combat = false;
    state.in_run = false;
    CardModel clone(sim_, ref);
    AddCard(clone);
    return clone;
}

void RunState::AddCard(CardModel card, Player owner) {
    if (!card.HasBeenRemovedFromState()) card.SetOwner(owner);
    AddCard(card);
}

void RunState::AddCard(CardModel card) {
    if (card.HasBeenRemovedFromState()) {
        if (!ContainsCard(card)) {
            throw GameError("Tried to add card to RunState that has HasBeenRemovedFromState set as true, but it does not belong to this state!");
        }
        card.SetHasBeenRemovedFromState(false);
    } else {
        card.st().in_run = true;
    }
}

void RunState::RemoveCard(CardModel card) {
    card.st().in_run = false;
    card.st().owner = 0;
}

bool RunState::ContainsCard(CardModel card) const { return card.st().in_run; }

// The list part of IterateHookListeners (Contains is checked per item as the
// enumeration reaches it; see HookListenerRange in Hook.cpp).
HookListenerList RunState::IterateHookListeners(sts2::CombatState childCombatState) const {
    HookListenerList list;
    const GameState& g = *sim_->g;
    Player player(sim_, kPlayer);
    if (player.IsActiveForHooks()) {
        for (CardRef c : g.run.player.deck) {
            list.push_back(CardModel(sim_, c));
            if (g.cards[c].enchantment != EnchantmentId::NONE) list.push_back(EnchantmentModel(sim_, c));
        }
    }
    if (!childCombatState) {
        if (player.IsActiveForHooks()) {
            for (RelicRef r : g.run.player.relics) {
                if (!g.relics[r].is_melted) list.push_back(RelicModel(sim_, r));
            }
            for (int i = 0; i < g.run.player.max_potion_slots; ++i) {
                if (g.run.player.potions[i] != 0) list.push_back(PotionModel(sim_, g.run.player.potions[i]));
            }
        }
        // Modifiers (none in a standard run), badges (statistics) and the
        // MultiplayerScalingModel (no effect in single player).
    }
    return list;
}

// RunState.Contains (private static): by model type. Owner.IsActiveForHooks:
// the only player.
bool RunState::Contains(AbstractModel model) const {
    const GameState& g = *sim_->g;
    bool active = g.run.player.active_for_hooks;
    switch (model.Kind()) {
    case ModelKind::Relic: return !g.relics[model.Index()].removed && active;
    case ModelKind::Potion: return !g.potions[model.Index()].removed && active;
    case ModelKind::Card: return !g.cards[model.Index()].removed && active;
    case ModelKind::Enchantment: return !g.cards[model.Index()].removed && active;  // HasCard && ...
    case ModelKind::Modifier: return true;
    default: throw GameError("Invalid model type in RunState.Contains");
    }
}

}  // namespace sts2
