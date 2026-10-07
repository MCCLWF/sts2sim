#include "engine/run_setup.h"

#include "engine/sim/alloc.h"

#include <cstring>
#include <stdexcept>
#include <string>

namespace sts2 {

namespace {

void require_supported(const ProfileConfig& profile) {
    if (!profile.all_unlocked) throw std::invalid_argument("only fully unlocked profiles are supported");
}

// ---------------------------------------------------------------- relic grab bags

bool relic_unlocked_by_epoch(RelicId relic, uint64_t unlocked_epoch_bits);

void add_to_bag(RelicGrabBag& bag, RelicId relic) {
    RelicRarity rarity = data(relic).rarity;
    RelicGrabBag::Deque* d = bag.find(rarity);
    if (d == nullptr) {
        bag.deques.push_back(RelicGrabBag::Deque{rarity, {}});
        d = &bag.deques.back();
    }
    d->relics.push_back(relic);
}

// RelicGrabBag.Populate(IEnumerable<RelicModel>, Rng)
void populate_bag(RelicGrabBag& bag, std::span<const RelicId> relics, Rng& rng, uint64_t unlocked_epoch_bits) {
    if (bag.populated()) throw std::logic_error("Grab bag was already populated.");
    bag.original.clear();
    for (RelicId r : relics) {
        if (!relic_unlocked_by_epoch(r, unlocked_epoch_bits)) continue;
        bag.original.push_back(r);
        add_to_bag(bag, r);
    }
    for (RelicGrabBag::Deque& d : bag.deques) rng.shuffle(d.relics.span());
}

bool player_bag_rarity(RelicRarity r) {
    return r == RelicRarity::Common || r == RelicRarity::Uncommon || r == RelicRarity::Rare ||
           r == RelicRarity::Shop;
}

// RelicGrabBag.Populate(Player, Rng): shared pool + character pool, regular rarities only.
bool relic_unlocked_by_epoch(RelicId relic, uint64_t unlocked_epoch_bits) {
    int32_t epoch = -1;
    switch (relic) {
    case RelicId::BONE_FLUTE:
    case RelicId::FUNERARY_MASK:
    case RelicId::BOOK_REPAIR_KNIFE: epoch = 28; break;  // Necrobinder3Epoch
    case RelicId::IVORY_TILE:
    case RelicId::BIG_HAT:
    case RelicId::BOOKMARK: epoch = 31; break;  // Necrobinder6Epoch
    case RelicId::FENCING_MANUAL:
    case RelicId::GALACTIC_DUST:
    case RelicId::LUNAR_PASTRY: epoch = 39; break;  // Regent3Epoch
    case RelicId::MINI_REGENT:
    case RelicId::ORANGE_DOUGH:
    case RelicId::REGALITE: epoch = 42; break;  // Regent6Epoch
    case RelicId::TOUGH_BANDAGES:
    case RelicId::TINGSHA:
    case RelicId::PAPER_KRANE: epoch = 51; break;  // Silent3Epoch
    case RelicId::TWISTED_FUNNEL:
    case RelicId::SNECKO_SKULL:
    case RelicId::HELICAL_DART: epoch = 54; break;  // Silent6Epoch
    default: return true;
    }
    return (unlocked_epoch_bits & (uint64_t{1} << epoch)) != 0;
}

void populate_player_bag(PlayerData& player, Rng& rng, uint64_t unlocked_epoch_bits) {
    RelicGrabBag& bag = player.relic_grab_bag;
    if (bag.populated()) throw std::logic_error("Grab bag was already populated.");
    StaticVec<RelicId, 320> list;
    for (RelicId r : data(RelicPoolId::SHARED_RELIC_POOL).relics) list.push_back(r);
    for (RelicId r : data(data(player.character).relic_pool).relics)
        if (relic_unlocked_by_epoch(r, unlocked_epoch_bits)) list.push_back(r);
    list.remove_all([](RelicId r) { return !player_bag_rarity(data(r).rarity); });
    populate_bag(bag, list, rng, unlocked_epoch_bits);
}

// ---------------------------------------------------------------- room generation

// Helpers/GrabBag.cs specialised for encounters.
struct EncounterGrabBag {
    StaticVec<EncounterId, 32> entries;
    double total_weight = 0.0;

    void add(EncounterId e) {
        entries.push_back(e);
        total_weight += 1.0;
    }

    int grab_index(Rng& rng) {
        double r = rng.next_double() * total_weight;
        double acc = 0.0;
        for (size_t i = 0; i < entries.size(); ++i) {
            acc += 1.0;
            if (r < acc) return static_cast<int>(i);
        }
        return -1;
    }

    template <class Pred>
    EncounterId grab_and_remove(Rng& rng, Pred pred) {
        bool any = false;
        for (EncounterId e : entries) any |= pred(e);
        if (!any) return EncounterId::NONE;
        int i;
        do {
            i = grab_index(rng);
        } while (i >= 0 && !pred(entries[static_cast<size_t>(i)]));
        return take(i);
    }

    EncounterId grab_and_remove(Rng& rng) { return take(grab_index(rng)); }

    EncounterId take(int i) {
        if (i < 0) return EncounterId::NONE;
        EncounterId e = entries[static_cast<size_t>(i)];
        total_weight -= 1.0;
        entries.erase_at(static_cast<size_t>(i));
        return e;
    }
};

bool shares_tags(EncounterId a, EncounterId b) {
    if (b == EncounterId::NONE) return false;
    return (data(a).tags & data(b).tags) != 0;
}

// ActModel.AddWithoutRepeatingTags
template <size_t N>
void add_without_repeating_tags(StaticVec<EncounterId, N>& encounters, EncounterGrabBag& bag, Rng& rng) {
    EncounterId last = encounters.empty() ? EncounterId::NONE : encounters.back();
    EncounterId e = bag.grab_and_remove(rng, [&](EncounterId c) { return !shares_tags(c, last) && c != last; });
    if (e == EncounterId::NONE) e = bag.grab_and_remove(rng);
    if (e != EncounterId::NONE) encounters.push_back(e);
}

int number_of_weak_encounters(ActId act) {
    switch (act) {
        case ActId::OVERGROWTH:
        case ActId::UNDERDOCKS: return 3;
        case ActId::HIVE:
        case ActId::GLORY: return 2;
        default: throw std::invalid_argument("number_of_weak_encounters: unsupported act");
    }
}

// ActModel.GenerateRooms, for a fully unlocked profile.
void generate_act_rooms(ActState& act, Rng& rng, bool is_multiplayer) {
    const ActData& a = data(act.id);
    RoomSet& rooms = act.rooms;

    StaticVec<EventId, kMaxEvents> events;
    for (EventId e : a.events) events.push_back(e);
    for (EventId e : modeldb::all_shared_events()) events.push_back(e);
    rng.shuffle(events.span());
    for (EventId e : events) rooms.events.push_back(e);

    const int weak = number_of_weak_encounters(act.id);
    EncounterGrabBag weak_bag;
    for (int i = 0; i < weak; ++i) {
        if (weak_bag.entries.empty()) {
            for (EncounterId e : a.weak) weak_bag.add(e);
        }
        add_without_repeating_tags(rooms.normal_encounters, weak_bag, rng);
    }
    EncounterGrabBag regular_bag;
    for (int i = weak; i < act_number_of_rooms(act.id, is_multiplayer); ++i) {
        if (regular_bag.entries.empty()) {
            for (EncounterId e : a.regular) regular_bag.add(e);
        }
        add_without_repeating_tags(rooms.normal_encounters, regular_bag, rng);
    }
    EncounterGrabBag elite_bag;
    for (int i = 0; i < 15; ++i) {
        if (elite_bag.entries.empty()) {
            for (EncounterId e : a.elite) elite_bag.add(e);
        }
        add_without_repeating_tags(rooms.elite_encounters, elite_bag, rng);
    }
    rooms.boss = a.boss[static_cast<size_t>(rng.next_item_index(static_cast<int32_t>(a.boss.size())))];

    // GetUnlockedAncients (all unlocked: every ancient) + the shared subset.
    StaticVec<EventId, 8> ancients;
    for (EventId e : a.ancients) ancients.push_back(e);
    for (EventId e : act.shared_ancient_subset) ancients.push_back(e);
    int ai = rng.next_item_index(static_cast<int32_t>(ancients.size()));
    rooms.ancient = ai < 0 ? EventId::NONE : ancients[static_cast<size_t>(ai)];
}

}  // namespace

StaticVec<ActId, kMaxActs> choose_acts(std::string_view canonical_seed, const ProfileConfig& profile) {
    require_supported(profile);
    Rng rng(deterministic_hash(canonical_seed), "act_selection");
    StaticVec<ActId, kMaxActs> acts;
    for (std::span<const ActId> candidates : modeldb::acts_by_index()) {
        // IsUnlocked is true for every act once all epochs are revealed; the
        // "undiscovered act first" rule never fires on a veteran profile.
        int i = rng.next_item_index(static_cast<int32_t>(candidates.size()));
        acts.push_back(candidates[static_cast<size_t>(i)]);
    }
    return acts;
}

void new_run(GameState& g, const NewRunOptions& options) {
    require_supported(options.profile);
    if (options.character != CharacterId::IRONCLAD && options.character != CharacterId::SILENT
        && options.character != CharacterId::REGENT && options.character != CharacterId::NECROBINDER
        && options.character != CharacterId::DEFECT)
        throw InvalidRequest("new run: character must be one of the five supported single-player characters");
    if (options.ascension < 0 || options.ascension > 10) throw InvalidRequest("new run: ascension must be 0..10");
    if (options.progress_wongo_points < 0) throw std::invalid_argument("negative Wongo progress points");
    if (options.progress_character_wins < 0) throw std::invalid_argument("negative character win count");
    g = GameState{};
    RunData& run = g.run;
    std::string seed = canonicalize_seed(options.seed);
    if (seed.size() >= sizeof(run.seed)) throw std::invalid_argument("seed too long");
    std::memcpy(run.seed, seed.data(), seed.size());
    run.ascension = options.ascension;
    run.progress_wongo_points = options.progress_wongo_points;
    run.progress_character_wins = options.progress_character_wins;
    run.progress_ancient_stats = options.progress_ancient_stats;
    run.profile.unlocked_epoch_bits = options.profile.unlocked_epoch_bits;
    run.profile.full_veteran_compatible = options.profile.all_unlocked &&
        options.profile.unlocked_epoch_bits == 0x1ffffffffffffffULL;

    // Player.CreateForNewRun
    const CharacterData& ch = data(options.character);
    PlayerData& p = run.player;
    p.profile.unlocked_epoch_bits = options.profile.unlocked_epoch_bits;
    p.profile.full_veteran_compatible = run.profile.full_veteran_compatible;
    p.character = options.character;
    {  // new Creature(player, StartingHp, StartingHp)
        CreatureRef c = g.creatures.alloc();
        CreatureState& cs = g.creatures[c];
        cs.player = 1;
        cs.side = CombatSide::Player;
        cs.current_hp = ch.starting_hp;
        cs.max_hp = ch.starting_hp;
        p.creature = c;
    }
    p.max_energy = ch.max_energy;
    p.gold = ch.starting_gold;
    p.max_potion_slots = 3;
    p.potions.resize(3);
    p.base_orb_slots = ch.orb_slots;
    constexpr uint8_t kOwner = 1;  // the only player
    for (CardId c : ch.starting_deck) {
        CardRef card = alloc_card(g, c, kOwner);
        g.cards[card].floor_added = 1;
        g.cards[card].pile = PileType::Deck;
        g.cards[card].in_run = true;
        p.deck.push_back(card);
    }
    for (RelicId r : ch.starting_relics) {
        RelicRef relic = alloc_relic(g, r, kOwner);
        g.relics[relic].floor_added = 1;
        p.relics.push_back(relic);
    }
    size_t slot = 0;
    for (PotionId pot : ch.starting_potions) p.potions[slot++] = alloc_potion(g, pot, kOwner);

    // RunState.CreateForNewRun
    for (ActId a : choose_acts(seed, options.profile)) run.acts.push_back(ActState{.id = a});
    run.run_seed = run_seed_from_string(seed);
    run.rng = RunRngSet(run.run_seed);
    run.odds = UnknownMapPointOdds{};
    run.shared_relic_grab_bag.refresh_allowed = true;
    p.rng = PlayerRngSet::for_player(seed, 0);
    p.odds = PlayerOdds{};

    // RunManager.InitializeNewRun
    Rng& up_front = run.rng[RunRngType::UpFront];
    populate_bag(run.shared_relic_grab_bag, data(RelicPoolId::SHARED_RELIC_POOL).relics, up_front,
                 options.profile.unlocked_epoch_bits);
    populate_player_bag(p, up_front, options.profile.unlocked_epoch_bits);
    run.started_with_neow = true;  // NeowEpoch revealed
    if (run.has_ascension(AscensionLevel::TightBelt)) { p.max_potion_slots -= 1; p.potions.resize(p.max_potion_slots); }
    if (run.has_ascension(AscensionLevel::AscendersBane)) {
        CardRef bane = alloc_card(g, CardId::ASCENDERS_BANE, kOwner);
        g.cards[bane].floor_added = 1;
        g.cards[bane].pile = PileType::Deck;
        g.cards[bane].in_run = true;
        p.deck.push_back(bane);
    }

    // RunManager.GenerateRooms
    StaticVec<EventId, 4> shared_ancients;
    for (EventId e : modeldb::all_shared_ancients()) shared_ancients.push_back(e);
    up_front.shuffle(shared_ancients.span());
    for (size_t i = 1; i < run.acts.size(); ++i) {
        int count = up_front.next_int(static_cast<int32_t>(shared_ancients.size()) + 1);
        ActState& act = run.acts[i];
        act.has_shared_ancient_subset = true;
        for (int k = 0; k < count; ++k) act.shared_ancient_subset.push_back(shared_ancients[static_cast<size_t>(k)]);
        StaticVec<EventId, 4> rest;
        for (size_t k = static_cast<size_t>(count); k < shared_ancients.size(); ++k) {
            if (!rest.contains(shared_ancients[k])) rest.push_back(shared_ancients[k]);  // Except() dedups
        }
        shared_ancients = rest;
    }
    for (size_t i = 0; i < run.acts.size(); ++i) {
        ActState& act = run.acts[i];
        generate_act_rooms(act, up_front, /*is_multiplayer=*/false);
        // Tutorial ordering (ApplyDiscoveryOrderModifications) is a no-op when
        // every boss has been seen and the profile has completed runs.
        if (i == run.acts.size() - 1 && run.has_ascension(AscensionLevel::DoubleBoss)) {
            const ActData& a = data(act.id);
            StaticVec<EncounterId, 8> others;
            for (EncounterId e : a.boss) {
                if (e != act.rooms.boss) others.push_back(e);
            }
            int bi = up_front.next_item_index(static_cast<int32_t>(others.size()));
            act.rooms.second_boss = bi < 0 ? EncounterId::NONE : others[static_cast<size_t>(bi)];
        }
    }
}

ActMap generate_map_for_act(const RunData& run, int act_index) {
    const ActState& act = run.acts[static_cast<size_t>(act_index)];
    return generate_act_map(run.run_seed, act_index, act.id, /*is_multiplayer=*/false,
                            /*replace_treasure_with_elites=*/false, act.rooms.second_boss != EncounterId::NONE,
                            run.has_ascension(AscensionLevel::SwarmingElites));
}

}  // namespace sts2
