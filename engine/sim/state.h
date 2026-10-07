// The whole mutable simulation state, as plain data. Objects the C# code
// allocates (cards, powers, creatures, ...) live in fixed pools and are
// referenced by slot index; see docs/design-engine.md §2.
#pragma once

#include <cstdint>
#include <string_view>
#include <type_traits>

#include "content/generated/tables.h"
#include "core/dec.h"
#include "core/rng.h"
#include "engine/containers.h"
#include "engine/run_state.h"
#include "engine/sim/pool.h"

namespace sts2 {

// Slot indices into the pools below; 0 is null.
using CardRef = uint32_t;
using PowerRef = uint32_t;
using CreatureRef = uint32_t;
using RelicRef = uint32_t;
using PotionRef = uint32_t;
using OrbRef = uint32_t;
using RewardRef = uint32_t;
using AfflictionRef = uint32_t;

inline constexpr uint16_t kCardPool = 1024;
inline constexpr uint16_t kPowerPool = 192;
inline constexpr uint16_t kCreaturePool = 24;
inline constexpr uint16_t kRelicPool = 72;
inline constexpr uint16_t kPotionPool = 40;
inline constexpr uint16_t kOrbPool = 256;
inline constexpr uint16_t kRewardPool = 16;
// AfflictionModel is a heap object in the game.  It remains distinct from
// the card that carries it, including while it is a pending ToMutable object.
inline constexpr uint16_t kAfflictionPool = 256;
// A pile may contain every allocated card. Long Ironclad Anger fights can
// move more than 512 generated copies into one discard/draw pile.
inline constexpr int kPileCapacity = kCardPool;
inline constexpr int kMaxVarOverrides = 3;  // card DynamicVars changed at run time
inline constexpr int kMaxModelVars = 3;     // per power / relic (data max: 2 / 3)
inline constexpr int kExtraSlots = 4;       // subclass fields (see design §2)
inline constexpr int kMaxLocalCostModifiers = 32;  // source uses a growable list; cost effects can stack
inline constexpr int kMaxPowersPerCreature = 32;
inline constexpr int kMaxMonsterStates = 24;
// Model-specific amounts captured around an in-flight card play. Powers keep
// these keyed by power and card identity so nested play hooks remain snapshotable.
inline constexpr int kMaxActiveCardPlayPowerCaptures = kCardPool * (kCreaturePool + 2);

// ------------------------------------------------------------------ cards

// Entities/Cards/LocalCostModifier.cs
struct LocalCostModifier {
    int32_t amount = 0;
    LocalCostType type{};
    bool reduce_only = false;
    LocalCostModifierExpiration expiration{};
};

// Entities/Cards/TemporaryCardCost.cs (star costs).
struct TemporaryCardCost {
    int32_t cost = 0;
    bool clears_when_turn_ends = false;
    bool clears_when_card_is_played = false;
};

// A DynamicVar whose BaseValue no longer equals the table value for the
// card's upgrade level (see CardModel::DynamicVars).
struct VarOverride {
    Dec value;
    uint8_t index = 0;  // into data(id).vars
};

// One CardModel instance: deck cards and combat cards alike.
struct CardState {
    CardId id = CardId::NONE;
    uint8_t upgrade = 0;           // _currentUpgradeLevel
    uint8_t owner = 0;             // player index + 1; 0 = no owner
    PileType pile = PileType::None;  // Pile?.Type
    bool in_combat = false;        // registered in CombatState._allCards
    bool in_run = false;           // registered in RunState._allCards
    bool removed = false;          // HasBeenRemovedFromState
    bool is_dupe = false;
    bool exhaust_on_next_play = false;
    bool single_turn_retain = false;
    bool single_turn_sly = false;
    bool star_cost_set = false;
    bool was_star_cost_just_upgraded = false;
    bool cost_was_just_upgraded = false;  // CardEnergyCost.WasJustUpgraded
    bool is_enchantment_preview = false;
    EnchantmentStatus enchantment_status{};
    int32_t floor_added = -1;      // FloorAddedToDeck; -1 = null
    // CardEnergyCost
    int32_t cost_base = 0;
    int32_t captured_x = 0;
    StaticVec<LocalCostModifier, kMaxLocalCostModifiers> cost_modifiers;
    // Star cost
    int32_t base_star_cost = 0;
    int32_t last_stars_spent = 0;
    StaticVec<TemporaryCardCost, 3> temporary_star_costs;
    int32_t base_replay_count = 0;
    int32_t current_play_index = 0;
    int32_t last_played_turn = -1;  // CardPlayFinishedEntry turn snapshot, -1 = never played
    int32_t calamity_pending_amount = -1;  // CalamityPower snapshot for the current card play
    uint64_t keywords = 0;         // bit per CardKeyword
    StaticVec<VarOverride, kMaxVarOverrides> var_overrides;
    CardRef clone_of = 0;
    CardRef deck_version = 0;
    // Membership in one BingBong instance's CardsToSkip identity set. A card
    // is inserted only by that relic when it creates this new clone; model
    // clones are different identities and must clear this membership.
    RelicRef bing_bong_skip_relic = 0;
    CreatureRef current_target = 0;
    // Enchantment / affliction (one each, owned by the card)
    EnchantmentId enchantment = EnchantmentId::NONE;
    AfflictionId affliction = AfflictionId::NONE;
    int32_t enchantment_amount = 0;
    int32_t affliction_amount = 0;
    // Pool identity of the mutable AfflictionModel on this card. The enum and
    // amount above remain denormalized rule fields for old call sites; this
    // reference is the object identity used by the model handle.
    AfflictionRef affliction_instance = 0;
    Dec enchantment_var;           // EnchantmentModel.DynamicVars (data max: 1)
    int32_t enchantment_extra[2] = {};
    Dec growth_damage;            // Rampage._extraDamageFromPlays / Thrash._extraDamage
    int32_t extra[5] = {};  // model-private CardModel state; Regent SovereignBlade stores four Dec words plus repeats
};

// ----------------------------------------------------------------- powers

struct PowerState {
    PowerId id = PowerId::NONE;
    int32_t amount = 0;
    int32_t amount_on_turn_start = 0;
    bool skip_next_duration_tick = false;
    CreatureRef owner = 0;
    CreatureRef applier = 0;
    CreatureRef target = 0;
    StaticVec<CreatureRef, 4> dampen_casters;
    struct DowngradedCard { CardRef card = 0; int32_t level = 0; };
    StaticVec<DowngradedCard, 4> dampen_downgraded_cards;
    Dec vars[kMaxModelVars] = {};
    int32_t extra[kExtraSlots] = {};
};

struct ActiveCardPlayPowerCapture {
    PowerRef power = 0;
    CardRef card = 0;
    int32_t amount = 0;
};

// ------------------------------------------------------- relics & potions

struct RelicState {
    RelicId id = RelicId::NONE;
    uint8_t owner = 0;             // player index + 1
    bool removed = false;          // HasBeenRemovedFromState
    bool is_melted = false;
    bool is_wax = false;
    RelicStatus status{};
    int32_t floor_added = 0;       // RelicModel._floorAddedToDeck is a nonnullable int
    Dec vars[kMaxModelVars] = {};
    int32_t extra[kExtraSlots] = {};
    StaticVec<CardRef, 5> paels_tooth_cards;
};

struct PotionState {
    PotionId id = PotionId::NONE;
    uint8_t owner = 0;
    bool removed = false;
    int32_t extra[2] = {};
};

// Models/OrbModel.cs. An evoked orb stays allocated until combat ends so a
// pending Hook listener snapshot can still test HasBeenRemovedFromState.
struct OrbState {
    OrbId id = OrbId::NONE;
    uint8_t owner = 0;
    bool removed = false;
    Dec dark_evoke_val = Dec(6);
    Dec glass_passive_val = Dec(4);
};

// -------------------------------------------------------------- creatures

// Exact sufficient statistics for every v0.111.0 StateLog consumer. The
// native List itself is unbounded; Contains, a trailing repetition count,
// distances in the IsMove subsequence, and Last never need the full list.
// This is a rule-continuation representation, not an exported history list.
struct MonsterMoveHistory {
    uint32_t seen = 0;
    uint32_t seen_moves = 0;
    uint32_t count = 0;
    uint32_t move_count = 0;
    uint32_t consecutive = 0;
    uint32_t move_age[kMaxMonsterStates] = {};
    int8_t last = -1;

    bool empty() const { return count == 0; }
    bool Contains(int8_t state) const { return (seen & (uint32_t{1} << state)) != 0; }
    void Append(int8_t state, bool is_move) {
        if (state < 0 || state >= kMaxMonsterStates) throw GameError("invalid logged monster state");
        // List.Count is an int32 in the source. This guard is far beyond any
        // observed trace and prevents the summary from wrapping silently.
        if (count == 0x7fffffffu) throw CapacityError("monster history exceeds native int32 Count");
        ++count;
        consecutive = last == state ? consecutive + 1 : 1;
        last = state;
        seen |= uint32_t{1} << state;
        if (is_move) {
            ++move_count;
            for (int i = 0; i < kMaxMonsterStates; ++i)
                if ((seen_moves & (uint32_t{1} << i)) != 0) ++move_age[i];
            move_age[state] = 0;
            seen_moves |= uint32_t{1} << state;
        }
    }
    float RepeatMultiplier(int8_t state, float threshold) const {
        // The native Count/num3 loop returns zero iff the same-state suffix
        // reaches its float threshold. This retains float rounding even for
        // thresholds above 2^24 and also preserves zero/negative maxTimes.
        uint32_t suffix = last == state ? consecutive : 0;
        return static_cast<float>(suffix) < threshold ? 1.0f : 0.0f;
    }
    bool InMoveCooldown(int8_t state, int32_t cooldown) const {
        return cooldown > 0 && (seen_moves & (uint32_t{1} << state)) != 0
            && move_age[state] < static_cast<uint32_t>(cooldown);
    }
};
static_assert(kMaxMonsterStates < 32);
static_assert(std::is_trivially_copyable_v<MonsterMoveHistory>);

// MonsterMoves/MonsterMoveStateMachine. The machine's definition (states,
// moves, weights) is code, built per monster type; this is its run-time part.
struct MoveStateMachineState {
    bool exists = false;              // MoveStateMachine != null
    uint16_t definition = 0;          // interned machine definition
    int8_t current = -1;              // _currentState
    bool performed_first_move = false;
    uint32_t performed_at_least_once = 0;    // MoveState._performedAtLeastOnce, bit per state
    int8_t follow_up[kMaxMonsterStates] = {};  // FollowUpState set at run time; -1 = the definition's
    MonsterMoveHistory log;                   // StateLog's rule predicates
};

// MonsterModel.NextMove: a state of the machine, or the STUNNED move built by
// Creature.StunInternal.
struct NextMoveState {
    int8_t state = -1;          // index into the machine definition; -1 = UNSET_MOVE
    bool stunned = false;       // the STUNNED MoveState built by Creature.StunInternal
    uint16_t stun_move = 0;     // registered stun callback (register_move_fn)
    int8_t stun_follow_up = -1; // FollowUpStateId resolved; -1 = not a state of the machine
    bool stun_performed = false;
};

struct MonsterState {
    MonsterId id = MonsterId::NONE;
    Rng rng;                    // MonsterModel.Rng
    bool is_performing_move = false;
    bool spawned_this_turn = false;
    MoveStateMachineState machine;
    NextMoveState next_move;
    Dec vars[kMaxModelVars] = {};
    int32_t extra[kExtraSlots] = {};
};

// Entities/Creatures/HpDisplay; also read by Hellraiser's autoplay cap.
enum class HpDisplay : uint8_t { Normal, InfiniteWithNumbers, InfiniteWithoutNumbers };

struct CreatureState {
    int32_t block = 0;
    int32_t current_hp = 0;
    int32_t max_hp = 0;
    HpDisplay hp_display = HpDisplay::Normal;
    int32_t monster_max_hp_before_modification = -1;  // null = -1
    int32_t combat_id = -1;       // CombatId; null = -1
    CombatSide side = CombatSide::Player;
    bool attached = false;        // CombatState != null
    uint8_t player = 0;           // player index + 1; 0 = a monster
    uint8_t pet_owner = 0;        // player index + 1
    int8_t slot = -1;             // index into the encounter's Slots; -1 = null
    StaticVec<PowerRef, kMaxPowersPerCreature> powers;
    int32_t attacks_this_turn = 0;
    struct PoweredDamageCount {
        CreatureRef dealer = 0;
        int32_t count = 0;
    };
    StaticVec<PoweredDamageCount, 4> powered_damage_this_turn;
    int32_t PoweredDamageFrom(CreatureRef dealer) const {
        for (const auto& entry : powered_damage_this_turn) if (entry.dealer == dealer) return entry.count;
        return 0;
    }
    void RecordPoweredDamage(CreatureRef dealer, int32_t amount = 1) {
        if (!dealer || amount <= 0) throw GameError("invalid powered damage history entry");
        for (auto& entry : powered_damage_this_turn) if (entry.dealer == dealer) {
            if (amount > INT32_MAX - entry.count) throw CapacityError("damage history count exceeds original int32 range");
            entry.count += amount;
            return;
        }
        powered_damage_this_turn.push_back({dealer, amount});
    }
    MonsterState monster;
};

// ------------------------------------------------------------------ combat

struct PlayerCombatData {
    bool exists = false;          // Player.PlayerCombatState != null
    int32_t turn_number = 1;
    PlayerTurnPhase phase = PlayerTurnPhase::None;
    int32_t energy = 0;
    int32_t stars = 0;
    StaticVec<CardRef, kPileCapacity> piles[5];  // Hand, Draw, Discard, Exhaust, Play
    StaticVec<CreatureRef, 8> pets;
    // OrbQueue
    int32_t orb_capacity = 0;
    StaticVec<OrbRef, 10> orbs;
};

struct CombatData {
    bool exists = false;
    bool precreated_for_event_layout = false;
    EncounterId encounter = EncounterId::NONE;
    bool should_resume_parent_event_after_combat = false;
    bool battleworn_dummy_ran_out_of_time = false; // BattlewornDummyEventEncounter.RanOutOfTime
    int32_t round_number = 1;
    CombatSide current_side = CombatSide::Player;
    int32_t next_creature_id = 0;
    StaticVec<CreatureRef, 12> allies;
    StaticVec<CreatureRef, 12> enemies;
    StaticVec<CreatureRef, 12> escaped;
    StaticVec<ActiveCardPlayPowerCapture, kMaxActiveCardPlayPowerCaptures> active_card_play_power_captures;
    // CombatTurnState / CombatManager
    bool is_in_progress = false;
    bool is_starting = false;
    bool pending_loss = false;
    bool player_actions_disabled = false;
    bool ending_player_turn_phase_one = false;
    bool ending_player_turn_phase_two = false;
    int32_t executing_card_or_potion_effect = 0;  // nesting depth of Begin/EndCardOrPotionEffect
    // CombatTurnState, single player
    // EncounterModel instance
    Rng encounter_rng;                           // EncounterModel.Rng
    StaticVec<MonsterId, 16> spawned_enemies;    // _spawnedEnemies (canonical monsters, first spawn order)
    bool gremlin_merc_gold_was_stolen = false;   // GremlinMercNormal.GoldWasStolen
    bool player_played_skill_this_turn = false;  // CombatHistory.CardPlayStarted (single player)
    int32_t cards_played_this_combat = 0;         // CombatHistory.CardPlayFinished
    // Last non-dupe player Attack CardPlayFinished in the two most recent
    // player turn numbers with such an entry. Initial draws may autoplay an
    // Attack before HistoryCourse queries TurnNumber - 1 at AutoPrePlay.
    CardRef player_last_nondupe_attack_card = 0;
    int32_t player_last_nondupe_attack_turn = -1;
    CardRef player_previous_nondupe_attack_card = 0;
    int32_t player_previous_nondupe_attack_turn = -1;
    int32_t player_last_non_auto_card_play_turn = -1;
    int32_t player_attack_cards_this_turn = 0;   // CardPlayFinished entries for Stomp
    int32_t player_attack_cards_started_this_turn = 0;  // JugglingPower's current-turn CardPlayStarted Attack count
    int32_t player_attack_skill_cards_started_this_turn = 0;  // NostalgiaPower's current-turn CardPlayStarted count
    int32_t player_cards_started_this_turn = 0;  // first-in-series player CardPlayStarted entries for EchoFormPower
    int32_t player_all_cards_started_this_turn = 0;  // all player CardPlayStarted entries for Normality
    int32_t player_card_block_entries_this_turn = 0;  // Move BlockGainedEntry with this player's CardPlay
    int32_t player_card_block_history_round = -1;
    int32_t player_card_block_history_turn = -1;
    CombatSide player_card_block_history_side = CombatSide::Player;
    uint32_t player_card_block_history_epoch = 0;
    int32_t player_cards_finished_this_turn = 0;  // player CardPlayFinished entries for Ftl and Regent relics
    int32_t player_skill_cards_finished_this_turn = 0;  // player Skill CardPlayFinished entries for Regent/Silent formulas
    int32_t player_shiv_cards_finished_this_turn = 0;  // player Shiv CardPlayFinished entries for Silent power
    int32_t player_cards_discarded_this_turn = 0;  // player CardDiscardedEntry count for MementoMori
    int32_t player_cards_drawn_this_combat = 0;  // player CardDrawnEntry count for Murder
    int32_t player_cards_drawn_this_turn = 0;  // non-hand-draw player CardDrawnEntry count
    int64_t player_energy_spent_this_turn = 0;  // monotone history sum; original int32 Sum checks at query
    int32_t player_status_cards_drawn_this_turn = 0;  // player Status CardDrawnEntry count for IterationPower
    int32_t player_zero_cost_attacks_started_this_turn = 0;  // player Attack starts with EnergyValue == 0 for FeralPower
    int32_t player_lightning_orbs_channeled_this_combat = 0;  // all player Lightning OrbChanneledEntry count for Voltaic
    int32_t player_cards_generated_this_combat = 0;  // player CardGeneratedEntry count for Supermassive
    int32_t player_ethereal_cards_played_this_combat = 0;  // player CardPlayFinished entries with Ethereal
    bool player_applied_doom_this_turn = false;  // a DoomPowerReceivedEntry attributed to this player
    int64_t player_stars_gained_this_turn = 0;  // positive history sum; original int32 Sum checks at query
    int32_t player_damage_received_entries_this_combat = 0;  // cumulative DamageReceivedEntry count for TearAsunder
    bool player_lost_hp_this_turn = false;    // Spite's DamageReceivedEntry.HappenedThisTurn (UnblockedDamage > 0)
    bool player_lost_hp_previous_turn = false;
    // EmotionChip uses DamageReceivedEntry.HappenedLastPlayerTurn together
    // with !DamageResult.WasFullyBlocked. Buffer and similar late HP-loss
    // modifiers can leave UnblockedDamage at zero while still satisfying that
    // predicate, so keep the history predicate separately from real HP loss.
    bool player_nonfully_blocked_damage_this_turn = false;
    bool player_nonfully_blocked_damage_previous_turn = false;
    int32_t cards_exhausted_this_combat = 0;     // CardExhausted entries for Midnight
    int32_t player_bound_cards_afflicted_this_turn = 0; // CardAfflictedEntry for ChainsOfBindingPower
    int32_t player_cards_exhausted_this_turn = 0; // CardExhaustedEntry for the player's current turn
    bool player_ready_to_end_turn = false;       // PlayersReadyToEndTurn
    bool player_taking_extra_turn = false;       // PlayersTakingExtraTurn
    bool is_enemy_turn_started = false;
    float gold_proportion = 1.0f;                // CombatRoom.GoldProportion (set by OnCombatEnded)
};
static_assert(std::is_copy_constructible_v<CombatData>);

// ---------------------------------------------------------------- rewards

// Runs/CardCreationOptions.cs (a record; the filter delegate is an id).
struct CardCreationOptionsState {
    StaticVec<CardPoolId, 8> pools;
    CardCreationSource source = CardCreationSource::None;
    CardRarityOddsType rarity_odds = CardRarityOddsType::None;
    CardCreationFlags flags{};
    uint8_t filter = 0;  // CardPoolFilter: 0 = null
    CardRarity required_rarity = CardRarity::None;  // optional rarity predicate
    CardType required_type = CardType::None;         // optional card-type predicate
    StaticVec<CardId, 8> excluded_ids;               // reward-specific pool filter (Lasting Candy)
};

// One Reward (Rewards/*.cs); `type` selects the subclass and its fields.
struct RewardState {
    RewardType type = RewardType::None;
    bool successfully_selected = false;  // SuccessfullySelected
    // GoldReward
    int32_t gold_min = 0, gold_max = 0, gold_amount = -1;
    bool gold_stolen_back = false;
    // PotionReward
    PotionRef potion = 0;
    bool potion_was_taken = false;
    // RelicReward
    RelicRarity relic_rarity = RelicRarity::None;
    RelicRef relic = 0;
    bool relic_predetermined = false;
    bool relic_was_taken = false;
    // CardReward
    StaticVec<CardRef, 6> cards;         // _cards
    CardCreationOptionsState options;    // Options
    CardCreationOptionsState reroll_options;
    int32_t option_count = 0;
    bool cards_manually_set = false;
    bool has_been_rerolled = false;
    bool can_reroll = false;
    bool can_skip = true;
    bool relic_obtained_subscribed = false;  // Player.RelicObtained += OnRelicObtained
    bool upgrade_after_generated = false;    // CardReward.AfterGenerated event
    bool use_event_rng = false;             // Crystal Sphere reward RNG override
};

// Rewards/RewardsSet.cs: the set on the rewards screen (one at a time in
// single player; RewardsSetSynchronizer's stack never holds more). The
// rewards themselves live in GameState::rewards.
struct RewardsSetState {
    bool active = false;                 // being offered
    RoomType room = RoomType::Unassigned;  // Room?.RoomType; Unassigned = no room
    bool allow_empty = false;
    bool disallow_skipping = false;
    bool generated = false;
    StaticVec<RewardRef, 16> rewards;    // Rewards; Crystal Sphere can reveal 14 items
};

// ----------------------------------------------------------------- events

// Events/EventOption.cs. OnChosen is a C# closure: here a registered
// callback (game/Events/EventOption.h) plus what it captured.
struct EventOptionState {
    uint32_t text_key = 0;         // TextKey (LocKey hash)
    uint16_t on_chosen = 0;        // registered callback; 0 = null (a locked option)
    bool is_locked = false;
    bool is_proceed = false;
    bool was_chosen = false;
    bool disable_on_chosen = true;
    RelicRef relic = 0;            // Relic (WithRelic / FromRelic)
    int32_t arg = 0;               // a value the closure captured
    uint32_t done_page = 0;        // AncientEventModel.RelicOption customDonePage; 0 = null
    uint16_t will_kill_player = 0; // registered predicate; 0 = null
    Dec will_kill_value;           // ThatDoesDamage / ThatDecreasesMaxHp amount
};

// Models/EventModel.cs (and AncientEventModel.cs): an event instance.
struct EventState {
    EventId id = EventId::NONE;
    bool began = false;            // Owner != null
    bool is_finished = false;
    bool cleanup_called = false;
    Rng rng;
    uint32_t description = 0;      // Description (LocKey hash)
    StaticVec<EventOptionState, 24> options;  // CurrentOptions (Neow builds lists of up to 17)
    Dec vars[12] = {};             // DynamicVars (CanonicalVars order)
    int32_t extra[8] = {};         // subclass fields
    StaticVec<CardRef, 128> slippery_skipped_removals;
    // CrystalSphereMinigame logical board; 0 hidden empty, 1 clear empty,
    // 2..16 hidden item cells, 18..32 clear item cells.
    StaticVec<uint8_t, 121> crystal_sphere_cells;
    StaticVec<uint8_t, 15> crystal_sphere_revealed_order;
    // AncientEventModel
    uint32_t custom_done_page = 0;
    int32_t healed_amount = 0;
};

// EventSynchronizer single-player bookkeeping, retained after EventRoom.Exit
// and reset by BeginEvent. No live task can cross an Env decision boundary.
struct EventSynchronizerState {
    uint8_t event_count = 0;
    EventId canonical_event = EventId::NONE;
    bool has_player_vote_slot = false;
    bool has_player_vote = false;
    uint32_t player_vote = 0;
    uint32_t page_index = 0;
};

// ------------------------------------------------------ non-combat rooms

// Entities/RestSite: the RestSiteOption subclasses (OptionId).
enum class RestSiteOptionId : uint8_t { Heal, Smith, Mend, Lift, Dig, Cook, Clone, Hatch, Kindle };

// RestSiteSynchronizer.PlayerRestSite (single player).
struct RestSiteState {
    StaticVec<RestSiteOptionId, 8> options;  // options (chosen ones are removed)
    bool completed = false;                  // completionTaskSource done
    int8_t last_chosen = -1;                 // lastChosenOptionIndex
    bool proceed_enabled = false;            // NRestSiteRoom's proceed button (an option succeeded)
};

// TreasureRoomRelicSynchronizer (single player).
struct TreasureState {
    bool picking = false;                    // _currentRelics != null
    StaticVec<RelicId, 4> current_relics;    // _currentRelics (canonical)
    bool singleplayer_skipped = false;       // _singleplayerSkipped
};

// Entities/Merchant: MerchantInventory and its entries. An entry's item is
// null (0) once bought and not restocked.
struct MerchantCardEntryState {
    CardRef card = 0;        // CreationResult.Card
    bool by_type = false;    // made with a CardType (a character card) or a CardRarity (colorless)
    CardType type = CardType::None;
    CardRarity rarity = CardRarity::None;
    bool on_sale = false;    // IsOnSale
    int32_t cost = 0;        // _cost
};
struct MerchantRelicEntryState {
    RelicRef relic = 0;      // Model
    int32_t cost = 0;
};
struct MerchantPotionEntryState {
    PotionRef potion = 0;    // Model
    int32_t cost = 0;
};
struct MerchantState {
    bool exists = false;     // the room's inventory (MerchantRoom.Inventories[0])
    StaticVec<MerchantCardEntryState, 5> character_cards;
    StaticVec<MerchantCardEntryState, 2> colorless_cards;
    StaticVec<MerchantRelicEntryState, 6> relics;  // FakeMerchant stocks six relics.
    StaticVec<MerchantPotionEntryState, 4> potions;
    bool has_card_removal = false;  // CardRemovalEntry != null
    bool card_removal_used = false;
    int32_t card_removal_cost = 0;
};

// Where the run waits for the player (the top-level decision points).
enum class RunPhase : uint8_t {
    None,      // not started / executing
    Map,       // choose the next map point
    Combat,    // combat play phase
    Rewards,   // the rewards screen
    Event,     // event options
    RestSite,  // rest site options
    Shop,      // the merchant
    Treasure,  // the treasure room
    Ended,     // the run is over (death, or the stop point was reached)
};

// -------------------------------------------------------------------- run

struct PlayerData {
    UnlockProfileState profile;
    CharacterId character = CharacterId::NONE;
    uint64_t net_id = 1;
    CreatureRef creature = 0;     // Player.Creature (holds the current / max HP)
    int32_t max_energy = 0;
    int32_t max_potion_slots = 3;
    int32_t base_orb_slots = 0;
    int32_t gold = 0;
    StaticVec<CardRef, kPileCapacity> deck;  // same list type as the combat piles
    StaticVec<RelicRef, kMaxRelics> relics;
    StaticVec<PotionRef, kMaxPotionSlots> potions;  // one entry per current slot; 0 = empty
    PlayerRngSet rng;
    PlayerOdds odds;
    RelicGrabBag relic_grab_bag;
    bool active_for_hooks = true;  // IsActiveForHooks (DeactivateHooks on death)
    bool can_use_or_remove_potions = true;  // Player.CanUseOrRemovePotions
    // ExtraPlayerFields
    int32_t card_shop_removals_used = 0;
    int32_t damage_dealt = 0;
    int32_t wongo_points_earned = 0;
};

struct RunData {
    UnlockProfileState profile;
    // SaveManager.Progress.WongoPoints at run capture time. This progression
    // value affects WelcomeToWongos badge rewards but is not an UnlockState.
    int32_t progress_wongo_points = 0;
    // SaveManager.Progress.GetStatsForCharacter(currentCharacter).TotalWins.
    // The Architect uses this count to select its per-character dialogue.
    int32_t progress_character_wins = 0;
    AncientEventProgressState progress_ancient_stats;
    char seed[24] = {};  // canonical seed string
    uint64_t run_seed = 0;
    int32_t ascension = 0;
    RunRngSet rng;
    UnknownMapPointOdds odds;
    StaticVec<ActState, kMaxActs> acts;
    int32_t current_act_index = 0;
    uint8_t player_count = 1;  // The simulator stores one local PlayerData.
    ActMap map;                                // RunState.Map (the current act's)
    StaticVec<MapCoord, 24> visited_map_coords;  // this act's; CurrentMapCoord is the last
    int32_t act_floor = 0;                     // ActFloor
    int32_t total_floor = 0;                   // TotalFloor: MapPointHistory entries over all acts
    uint16_t current_entry_rooms = 0;          // CurrentMapPointHistoryEntry.Rooms: bit per RoomType
    // Fur Coat's saved map quest state.  The original stores these on the
    // relic and on each map point; fixed arrays keep the same data in the
    // Env's trivially-copyable snapshot without a dynamic collection.
    int32_t fur_coat_act_index = -1;
    int8_t fur_coat_coord_cols[8] = {};
    int8_t fur_coat_coord_rows[8] = {};
    uint8_t fur_coat_coord_count = 0;
    bool fur_coat_coords_set = false;
    bool current_map_affected_by_fur_coat = false;
    // The room stack (RunState.PushRoom / PopCurrentRoom): only the types.
    StaticVec<RoomType, 4> rooms;
    RunPhase phase = RunPhase::None;
    bool is_victory = false;  // RunManager.OnEnded(isVictory: true), distinct from death.
    bool is_abandoned = false;  // RunManager.AbandonInternal, before forced death.
    RewardsSetState rewards;
    StaticVec<RewardRef, 8> combat_extra_rewards;
    // EventSynchronizer._events (single player: one) and a scratch instance
    // for calls on canonical events (EventModel.IsAllowed).
    EventState events[2];
    EventSynchronizerState event_synchronizer;
    StaticVec<EventId, 96> visited_event_ids;  // RunState.VisitedEventIds
    EventId room_event = EventId::NONE;          // EventRoom.CanonicalEvent of the room being entered
    RestSiteState rest_site;
    TreasureState treasure;
    MerchantState merchant;
    RelicGrabBag shared_relic_grab_bag;
    PlayerData player;
    // ExtraRunFields
    bool started_with_neow = false;
    int32_t test_subject_kills = 0;
    bool freed_repy = false;

    std::string_view seed_string() const { return std::string_view(seed); }
    bool has_ascension(AscensionLevel level) const { return ascension >= static_cast<int>(level); }
};

// A scoped card-selection policy that must travel with GameState snapshots.
// Default delegates to the environment choice prefix; FirstN mirrors the
// deterministic selector used during WhisperingEarring's auto-play.
enum class CardSelectorMode : uint8_t { Default, FirstN, Count };

// Models/AfflictionModel.cs. The card reference is nullable and is cleared
// by ClearInternal; pending instances keep their own writable Amount.
struct AfflictionState {
    AfflictionId id = AfflictionId::NONE;
    CardRef card = 0;
    int32_t amount = 0;
    uint64_t uid = 0;
    bool pending = false;
};
static_assert(std::is_trivially_copyable_v<AfflictionState>);

// ------------------------------------------------------------------- state

struct GameState {
    RunData run;
    CombatData combat;
    PlayerCombatData player_combat;
    Pool<CardState, kCardPool> cards;
    Pool<PowerState, kPowerPool> powers;
    Pool<CreatureState, kCreaturePool> creatures;
    Pool<RelicState, kRelicPool> relics;
    Pool<PotionState, kPotionPool> potions;
    Pool<OrbState, kOrbPool> orbs;
    Pool<RewardState, kRewardPool> rewards;
    Pool<struct AfflictionState, kAfflictionPool> afflictions;
    uint64_t next_affliction_uid = 1;
    CardSelectorMode card_selector_mode = CardSelectorMode::Default;
};

static_assert(std::is_copy_constructible_v<GameState>);
static_assert(std::is_trivially_copyable_v<CardSelectorMode>);
static_assert(std::is_trivially_copyable_v<EventSynchronizerState>);
static_assert(std::is_copy_constructible_v<CardState>);
static_assert(std::is_trivially_copyable_v<OrbState>);


}  // namespace sts2
