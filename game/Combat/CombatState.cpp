// Combat/CombatState.cs
#include "game/Combat/CombatState.h"

#include <algorithm>

#include "core/dotnet_sort.h"
#include "engine/sim/alloc.h"
#include "game/All.h"
#include "game/Models/AfflictionModel.h"
#include "game/Models/EncounterModel.h"

namespace sts2 {

namespace {
constexpr uint8_t kPlayer = 1;

CreatureList to_list(Sim* sim, const StaticVec<CreatureRef, 12>& refs) {
    CreatureList out;
    for (CreatureRef r : refs) out.push_back(Creature(sim, r));
    return out;
}
}  // namespace

sts2::RunState CombatState::RunState() const { return sts2::RunState(sim_); }
CreatureList CombatState::Allies() const { return to_list(sim_, st().allies); }
CreatureList CombatState::Enemies() const { return to_list(sim_, st().enemies); }
CreatureList CombatState::EscapedCreatures() const { return to_list(sim_, st().escaped); }

// Creatures => _allies.Concat(_enemies).ToList()
CreatureList CombatState::Creatures() const {
    CreatureList out = Allies();
    for (Creature c : Enemies()) out.push_back(c);
    return out;
}

CreatureList CombatState::PlayerCreatures() const {
    CreatureList out;
    for (Creature c : Creatures()) {
        if (c.IsPlayer()) out.push_back(c);
    }
    return out;
}

StaticVec<Player, 4> CombatState::Players() const {
    StaticVec<Player, 4> out;
    for (Creature c : PlayerCreatures()) out.push_back(c.Player());
    return out;
}

CreatureList CombatState::HittableEnemies() const {
    CreatureList out;
    for (Creature e : Enemies()) {
        if (e.IsHittable()) out.push_back(e);
    }
    return out;
}

CreatureList CombatState::CreaturesOnCurrentSide() const { return GetCreaturesOnSide(CurrentSide()); }

// CreateCard(canonicalCard, owner): ToMutable, AddCard, AfterCreated.
CardModel CombatState::CreateCard(CardId canonicalCard, Player owner) {
    CardModel card(sim_, alloc_card(*sim_->g, canonicalCard, 0));
    AddCard(card, owner);
    card.AfterCreated();
    return card;
}

// CloneCard(mutableCard): ClonePreservingMutability, then AddCard (the clone
// keeps the original's owner).
CardModel CombatState::CloneCard(CardModel mutableCard) {
    CardRef r = sim_->g->cards.alloc();
    CardState& c = sim_->g->cards[r];
    c = mutableCard.st();
    if (c.affliction_instance)
        c.affliction_instance = clone_affliction(*sim_->g, c.affliction_instance, r);
    c.bing_bong_skip_relic = 0;  // A new model is not in the relic's identity set.
    c.deck_version = 0;  // CardModel.AfterCloned clears the persistent deck link.
    c.current_target = 0;
    c.current_play_index = 0;
    c.removed = false;
    c.pile = PileType::None;  // a new object: in no pile yet
    // CardModel.ClonePreservingMutability copies model fields, but the
    // original card has no last-play history field. A clone therefore has no
    // CardPlayFinishedEntry and must not satisfy cards such as Bolas that
    // consult the previous-turn play history.
    c.last_played_turn = -1;
    c.in_combat = false;
    c.in_run = false;         // CombatState.AddCard does not add it to RunState._allCards
    CardModel clone(sim_, r);
    AddCard(clone);
    return clone;
}

void CombatState::AddCard(CardModel card, Player owner) {
    card.SetOwner(owner);
    AddCard(card);
}

void CombatState::AddCard(CardModel card) { card.st().in_combat = true; }

void CombatState::RemoveCard(CardModel card) {
    card.st().in_combat = false;
    card.st().owner = 0;
}

bool CombatState::ContainsCard(CardModel card) const { return card.st().in_combat; }

void CombatState::CaptureCardPlayPowerAmount(PowerModel power, CardModel card, int32_t amount) {
    auto& captures = st().active_card_play_power_captures;
    for (const ActiveCardPlayPowerCapture& capture : captures) {
        if (capture.power == power.Index() && capture.card == card.Index())
            throw GameError("CardPlay Power capture already exists for this power and card");
    }
    captures.push_back({power.Index(), card.Index(), amount});
}

bool CombatState::TakeCardPlayPowerAmount(PowerModel power, CardModel card, int32_t& amount) {
    auto& captures = st().active_card_play_power_captures;
    for (size_t i = 0; i < captures.size(); ++i) {
        const ActiveCardPlayPowerCapture& capture = captures[i];
        if (capture.power != power.Index() || capture.card != card.Index()) continue;
        amount = capture.amount;
        captures.erase_at(i);
        return true;
    }
    return false;
}

bool CombatState::AddCardPlayPowerAmount(PowerModel power, CardModel card, int32_t amount) {
    for (ActiveCardPlayPowerCapture& capture : st().active_card_play_power_captures) {
        if (capture.power != power.Index() || capture.card != card.Index()) continue;
        capture.amount = static_cast<int32_t>(static_cast<uint32_t>(capture.amount) + static_cast<uint32_t>(amount));
        return true;
    }
    return false;
}

void CombatState::ForgetCardPlayPowerCaptures(PowerModel power) {
    st().active_card_play_power_captures.remove_all(
        [&](const ActiveCardPlayPowerCapture& capture) { return capture.power == power.Index(); });
}

void CombatState::AddPlayer(Player player) {
    Creature c = player.Creature();
    AttachCreature(c);
    AddCreature(c);
}

// CreateCreature(monster, side, slot). The monster instance already lives in
// a creature pool slot (MonsterModel::ToMutable), with any fields the caller
// set on it; the new Creature is that slot.
Creature CombatState::CreateCreature(MonsterModel monster, CombatSide side, int8_t slot) {
    CreatureRef r = monster.Index();
    CreatureState& cs = sim_->g->creatures[r];
    if (cs.attached || cs.player != 0) throw GameError("CreateCreature: the monster already belongs to a creature");
    cs.side = side;
    cs.slot = slot;
    MonsterModel m = monster;
    // new Creature(monster, side, slot): hit points start at MaxInitialHp.
    int32_t minInitialHp = m.MinInitialHp();
    int32_t maxInitialHp = m.MaxInitialHp();
    if (minInitialHp > maxInitialHp) throw GameError("monster has min HP greater than its max");
    cs.max_hp = maxInitialHp;
    cs.current_hp = maxInitialHp;
    Creature creature(sim_, r);
    CreatureList creaturesOnSide = side == CombatSide::Player ? Allies() : Enemies();
    if (side == CombatSide::Enemy) {
        creature.SetUniqueMonsterHpValue(creaturesOnSide, RunState().Rng().Niche());
        creature.ScaleMonsterHpForMultiplayer(Encounter(), static_cast<int32_t>(Players().size()),
                                              RunState().CurrentActIndex());
    }
    AttachCreature(creature);
    const RunData& run = sim_->g->run;
    // C# CurrentMapCoord?.col/row ?? 0: a missing coordinate is zero,
    // while a present negative coordinate (e.g. the ancient row) is retained.
    int64_t col = run.visited_map_coords.empty() ? 0 : run.visited_map_coords.back().col;
    int64_t row = run.visited_map_coords.empty() ? 0 : run.visited_map_coords.back().row;
    cs.monster.rng = Rng(static_cast<uint64_t>(static_cast<int64_t>(run.rng.seed) + col + row +
                                               run.current_act_index + creature.CombatId()));
    if (st().encounter != EncounterId::NONE) EncounterModel(sim_).OnCreatureSpawned(creature);
    return creature;
}

void CombatState::AttachCreature(Creature creature) {
    creature.st().attached = true;
    creature.st().combat_id = st().next_creature_id;
    ++st().next_creature_id;
}

void CombatState::CreatureEscaped(Creature creature) {
    st().escaped.push_back(creature.Index());
    RemoveCreature(creature);
}

void CombatState::RemoveCreature(Creature creature, bool unattach) {
    if (!creature.st().attached) return;
    if (st().enemies.contains(creature.Index())) {
        st().enemies.remove(creature.Index());
    } else {
        if (!st().allies.contains(creature.Index())) throw GameError("Removed creature was not found.");
        st().allies.remove(creature.Index());
    }
    if (unattach) creature.st().attached = false;
}

bool CombatState::ContainsCreature(Creature creature) const {
    return st().allies.contains(creature.Index()) || st().enemies.contains(creature.Index());
}

bool CombatState::ContainsMonster(MonsterId monster) const {
    for (Creature c : Enemies()) {
        if (c.Monster().MonsterId_() == monster) return true;
    }
    return false;
}

Creature CombatState::GetCreature(int32_t combatId) const {
    if (combatId < 0) return {};
    for (Creature c : Creatures()) {
        if (c.CombatId() == combatId) return c;
    }
    return {};
}

CreatureList CombatState::GetCreaturesOnSide(CombatSide side) const {
    return side != CombatSide::Enemy ? Allies() : Enemies();
}

CreatureList CombatState::GetOpponentsOf(Creature creature) const {
    return GetCreaturesOnSide(creature.Side() == CombatSide::Player ? CombatSide::Enemy : CombatSide::Player);
}

CreatureList CombatState::GetTeammatesOf(Creature creature) const { return GetCreaturesOnSide(creature.Side()); }

// _enemies.Sort(Slots.IndexOf(a) - Slots.IndexOf(b)): List.Sort is .NET's
// unstable introsort.
void CombatState::SortEnemiesBySlotName() {
    if (st().encounter == EncounterId::NONE) return;
    Sim* sim = sim_;
    auto& e = st().enemies;
    dotnet_sort(std::span<CreatureRef>(e.data(), e.size()), [sim](CreatureRef a, CreatureRef b) {
        return static_cast<int>(sim->g->creatures[a].slot) - static_cast<int>(sim->g->creatures[b].slot);
    });
}

void CombatState::SetEnemyIndex(Creature creature, int32_t index) {
    if (!EncounterModel(sim_).Slots().empty()) throw GameError("Cannot modify turn order of a combat with pre-set slots");
    if (!st().enemies.contains(creature.Index())) throw GameError("Creature must be a valid enemy to change its turn order.");
    st().enemies.remove(creature.Index());
    size_t i = std::min(static_cast<size_t>(index), st().enemies.size() - 1);
    st().enemies.insert(i, creature.Index());
}

void CombatState::AddCreature(Creature creature) {
    if (!creature.st().attached) throw GameError("Creature was created for a different combat.");
    if (ContainsCreature(creature)) throw GameError("Creature is already in this combat.");
    (creature.Side() == CombatSide::Player ? st().allies : st().enemies).push_back(creature.Index());
}

// ------------------------------------------------------------ hook listeners

HookListenerList CombatState::IterateHookListeners() const {
    HookListenerList list;
    auto& g = *sim_->g;
    auto add_creature = [&](Creature creature) {
        for (PowerRef p : creature.st().powers) list.push_back(PowerModel(sim_, p));
        if (!creature.IsPlayer()) {
            list.push_back(MonsterModel(sim_, creature.Index()));
            return;
        }
        Player player = creature.Player();
        if (!player.IsActiveForHooks()) return;
        for (RelicRef r : g.run.player.relics) {
            if (!g.relics[r].is_melted) list.push_back(RelicModel(sim_, r));
        }
        for (int i = 0; i < g.run.player.max_potion_slots; ++i) {
            if (g.run.player.potions[i] != 0) list.push_back(PotionModel(sim_, g.run.player.potions[i]));
        }
        if (!player.PlayerCombatState()) return;
        for (OrbRef o : g.player_combat.orbs) list.push_back(OrbModel(sim_, o));
        for (const auto& pile : g.player_combat.piles) {  // AllPiles: Hand, Draw, Discard, Exhaust, Play
            for (CardRef c : pile) {
                list.push_back(CardModel(sim_, c));
                if (g.cards[c].affliction_instance != 0) list.push_back(AfflictionModel(sim_, c));
                if (g.cards[c].enchantment != EnchantmentId::NONE) list.push_back(EnchantmentModel(sim_, c));
            }
        }
    };
    for (CreatureRef c : st().allies) add_creature(Creature(sim_, c));
    for (CreatureRef c : st().enemies) add_creature(Creature(sim_, c));
    // Modifiers (none in a standard run), badges (statistics only) and the
    // MultiplayerScalingModel (a factor of 1 in single player) have no effect.
    return list;
}

bool CombatState::Contains(AbstractModel model) const {
    const GameState& g = *sim_->g;
    switch (model.Kind()) {
    case ModelKind::Power: {
        const PowerState& p = g.powers[model.Index()];
        return g.creatures[p.owner].attached;  // Owner.CombatState != null; players are always active
    }
    case ModelKind::Relic: return !g.relics[model.Index()].removed;
    case ModelKind::Potion: return !g.potions[model.Index()].removed;
    case ModelKind::Orb: return !g.orbs[model.Index()].removed && Player(sim_, g.orbs[model.Index()].owner).IsActiveForHooks();
    case ModelKind::Card: return !g.cards[model.Index()].removed;
    case ModelKind::Affliction: {
        const uint32_t encoded = model.Index();
        if ((encoded & AfflictionModel::kHandle) == 0) return false;
        const AfflictionRef ref = static_cast<AfflictionRef>(encoded & ~AfflictionModel::kHandle);
        if (!g.afflictions.live(ref)) return false;
        const CardRef card = g.afflictions[ref].card;
        return card != 0 && g.cards.live(card) && !g.cards[card].removed;
    }
    case ModelKind::Enchantment: return !g.cards[model.Index()].removed;  // HasCard && !Card.HasBeenRemovedFromState
    case ModelKind::Monster: return g.creatures[model.Index()].attached;
    case ModelKind::Modifier: return true;
    default: throw GameError("Invalid model type in CombatState.Contains");
    }
}

}  // namespace sts2
