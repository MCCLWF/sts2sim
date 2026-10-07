#include "engine/run_setup.h"
#include "engine/sim/alloc.h"
#include "engine/sim/state_hash.h"
#include "engine/sim/errors.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Entities/Cards/CardPlay.h"
#include "game/Commands/PotionCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Hooks/Hook.h"
#include "game/Odds/UnknownMapPointOdds.h"
#include "game/Rooms/AbstractRoom.h"
#include "game/Rewards/Reward.h"
#include "game/Entities/Merchant/MerchantInventory.h"
#include "game/Entities/RestSite/RestSiteOption.h"
#include "game/Runs/RunManager.h"
#include "game/Runs/RunFlow.h"
#include "game/Runs/CardCreationOptions.h"
#include "game/Models/Relics/Girya.h"
#include "game/Models/Relics/GoldenCompass.h"
#include "game/Models/Relics/MawBank.h"
#include "game/Models/Relics/ForgottenSoul.h"
#include "game/Models/Enchantments/SoulsPower.h"
#include "game/Models/Cards/LanternKey.h"
#include "game/Models/Events/HungryForMushrooms.h"
#include "game/Models/Events/CrystalSphere.h"

#include <algorithm>
#include <array>
#include <cstdio>

using namespace sts2;

struct Fixture {
    GameState state;
    Sim sim;
    Player player;
    Fixture() : sim(state), player(&sim, 1) {
        new_run(state, NewRunOptions{.seed = "RELICBATCH", .character = CharacterId::IRONCLAD, .ascension = 0});
        RunManager::SetActInternal(&sim, 0);
    }
};

int main() {
    {
        Fixture f;
        RelicCmd::Obtain(RelicId::DRAGON_FRUIT, f.player);
        PlayerCmd::GainGold(100, f.player);
        if (f.player.Creature().MaxHp() != 81 || f.player.Gold() != 199) return 1;
    }
    {
        Fixture f;
        RelicModel relic(&f.sim, alloc_relic(f.state, RelicId::MASSIVE_SCROLL, 0));
        if (relic.IsAllowed(RunState(&f.sim))) return 217;
        CardCreationOptionsState options = CardCreationOptions::Make(
            {CardPoolId::IRONCLAD_CARD_POOL, CardPoolId::COLORLESS_CARD_POOL},
            CardCreationSource::Other, CardRarityOddsType::RegularEncounter);
        options.filter = CardCreationOptions::OnlyMultiplayerCards;
        StaticVec<CardId, 640> candidates = CardCreationOptions::GetPossibleCards(options, f.player);
        if (candidates.empty()) return 218;
        for (CardId id : candidates)
            if (data(id).multiplayer != CardMultiplayerConstraint::MultiplayerOnly) return 219;
    }
    {
        Fixture f;
        RelicCmd::Obtain(RelicId::BOWLER_HAT, f.player);
        PlayerCmd::GainGold(100, f.player);
        if (f.player.Gold() != 224) return 2;
    }
    {
        Fixture f;
        RelicModel relic = RelicCmd::Obtain(RelicId::PLANISPHERE, f.player);
        MapCoord unknown{};
        bool found = false;
        for (const MapPoint& point : f.state.run.map.points) {
            if (point.type == MapPointType::Unknown) {
                unknown = point.coord;
                found = true;
                break;
            }
        }
        if (!found) return 3;
        f.state.run.visited_map_coords.push_back(unknown);
        f.state.run.rooms.push_back(RoomType::Event);
        f.player.Creature().SetCurrentHpInternal(70);
        relic.AfterRoomEntered(AbstractRoom(&f.sim, RoomType::Event));
        if (f.player.Creature().CurrentHp() != 75) return 4;
    }
    {
        Fixture f;
        RelicCmd::Obtain(RelicId::MANGO, f.player);
        if (f.player.Creature().MaxHp() != 94) return 5;
    }
    {
        Fixture f;
        f.player.Creature().SetCurrentHpInternal(50);
        RelicCmd::Obtain(RelicId::LEES_WAFFLE, f.player);
        if (f.player.Creature().MaxHp() != 87 || f.player.Creature().CurrentHp() != 87) return 6;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::BAG_OF_MARBLES, f.player);
        PlayerChoiceContext ctx;
        CombatState combat(&f.sim);
        relic.BeforeSideTurnStart(ctx, CombatSide::Player, combat.PlayerCreatures(), combat);
        for (Creature enemy : combat.Enemies()) {
            if (enemy.GetPowerAmount(PowerId::VULNERABLE_POWER) != 1) return 7;
        }
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::BAG_OF_PREPARATION, f.player);
        if (relic.ModifyHandDraw(f.player, Dec(5)).to_int() != 7) return 8;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::TUNGSTEN_ROD, f.player);
        if (relic.ModifyHpLostAfterOsty(f.player.Creature(), Dec(3), ValueProp::Unpowered, Creature{}, CardModel{}).to_int() != 2) return 9;
    }
    {
        Fixture f;
        RelicCmd::Obtain(RelicId::AKABEKO, f.player);
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        if (f.player.Creature().GetPowerAmount(PowerId::VIGOR_POWER) < 8) return 10;
    }
    {
        Fixture f;
        RelicModel relic = RelicCmd::Obtain(RelicId::BREAD, f.player);
        if (relic.ModifyMaxEnergy(f.player, Dec(3)).to_int() != 4) return 11;
    }
    {
        Fixture f;
        RelicModel relic = RelicCmd::Obtain(RelicId::MINIATURE_CANNON, f.player);
        CardModel card = f.player.Deck().Cards().front();
        if (relic.ModifyDamageAdditive(Creature{}, Dec(6), ValueProp::Move, f.player.Creature(), card, nullptr) != Dec(0)) return 12;
        CardCmd::Upgrade(card);
        if (relic.ModifyDamageAdditive(Creature{}, Dec(6), ValueProp::Move, f.player.Creature(), card, nullptr) != Dec(3)) return 13;
    }
    {
        Fixture f;
        RelicModel relic = RelicCmd::Obtain(RelicId::INTIMIDATING_HELMET, f.player);
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        CardPlay play{};
        play.Card = f.player.Deck().Cards().front();
        play.Resources.EnergyValue = 2;
        int before = f.player.Creature().Block();
        relic.BeforeCardPlayed(&play);
        if (f.player.Creature().Block() != before + 4) return 14;
    }
    {
        Fixture f;
        RelicModel relic = RelicCmd::Obtain(RelicId::SLING_OF_COURAGE, f.player);
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        relic.AfterRoomEntered(AbstractRoom(&f.sim, RoomType::Elite));
        if (f.player.Creature().GetPowerAmount(PowerId::STRENGTH_POWER) != 2) return 15;
    }
    {
        Fixture f;
        RelicCmd::Obtain(RelicId::VERY_HOT_COCOA, f.player);
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        if (f.player.PlayerCombatState().Energy() < 7) return 16;
    }
    {
        Fixture f;
        RelicModel relic = RelicCmd::Obtain(RelicId::RINGING_TRIANGLE, f.player);
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        if (relic.ShouldFlush(f.player)) return 17;
        f.player.PlayerCombatState().IncrementTurnNumber();
        if (!relic.ShouldFlush(f.player)) return 18;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::THE_ABACUS, f.player);
        int before = f.player.Creature().Block();
        PlayerChoiceContext ctx;
        relic.AfterShuffle(ctx, f.player);
        if (f.player.Creature().Block() != before + 6) return 19;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::UNCEASING_TOP, f.player);
        f.player.PlayerCombatState().SetPhase(PlayerTurnPhase::Play);
        int before = f.player.PlayerCombatState().Hand().Count();
        PlayerChoiceContext ctx;
        relic.AfterHandEmptied(ctx, f.player);
        if (f.player.PlayerCombatState().Hand().Count() != before + 1) return 20;
        f.player.PlayerCombatState().SetPhase(PlayerTurnPhase::End);
        relic.AfterHandEmptied(ctx, f.player);
        if (f.player.PlayerCombatState().Hand().Count() != before + 1) return 21;
    }
    {
        Fixture f;
        RelicModel relic = RelicCmd::Obtain(RelicId::MINIATURE_TENT, f.player);
        if (relic.ShouldDisableRemainingRestSiteOptions(f.player)) return 22;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::RAZOR_TOOTH, f.player);
        CardModel card;
        for (CardModel candidate : f.player.Deck().Cards()) {
            if ((candidate.Type() == CardType::Attack || candidate.Type() == CardType::Skill) && candidate.IsUpgradable()) {
                card = candidate;
                break;
            }
        }
        if (!card) return 23;
        CardPlay play{};
        play.Card = card;
        PlayerChoiceContext ctx;
        relic.AfterCardPlayed(ctx, &play);
        if (card.CurrentUpgradeLevel() != 1) return 24;
    }
    {
        Fixture f;
        RelicModel relic = RelicCmd::Obtain(RelicId::PRAYER_WHEEL, f.player);
        RewardList rewards(&f.sim);
        if (!relic.TryModifyRewards(f.player, rewards, AbstractRoom(&f.sim, RoomType::Monster))) return 25;
        if (rewards.size() != 1 || rewards[0].Type() != RewardType::Card) return 26;
        if (relic.TryModifyRewards(f.player, rewards, AbstractRoom(&f.sim, RoomType::Event))) return 27;
        if (rewards.size() != 1) return 28;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::PETRIFIED_TOAD, f.player);
        size_t before = f.player.Potions().size();
        relic.BeforeCombatStartLate();
        if (f.player.Potions().size() != before + 1) return 29;
        if (f.player.Potions().back().PotionId_() != PotionId::POTION_SHAPED_ROCK) return 30;
    }
    {
        Fixture f;
        RelicModel relic = RelicCmd::Obtain(RelicId::TINY_MAILBOX, f.player);
        RewardList rewards(&f.sim);
        if (!relic.TryModifyRestSiteHealRewards(f.player, rewards, false)) return 31;
        if (rewards.size() != 2 || rewards[0].Type() != RewardType::Potion || rewards[1].Type() != RewardType::Potion)
            return 32;
    }
    {
        Fixture f;
        RelicModel relic = RelicCmd::Obtain(RelicId::LIZARD_TAIL, f.player);
        Creature creature = f.player.Creature();
        creature.SetCurrentHpInternal(1);
        if (relic.ShouldDieLate(creature)) return 33;
        relic.AfterPreventingDeath(creature);
        if (!relic.IsUsedUp() || relic.Status() != RelicStatus::Disabled || creature.CurrentHp() != 41) return 34;
        if (!relic.ShouldDieLate(creature)) return 35;
    }
    {
        Fixture f;
        RelicModel relic = RelicCmd::Obtain(RelicId::LOOMING_FRUIT, f.player);
        if (f.player.Creature().MaxHp() != 111 || f.player.Creature().CurrentHp() != 111) return 36;
        if (!relic.HasUponPickupEffect()) return 37;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::KUNAI, f.player);
        CardModel attack;
        for (CardModel candidate : f.player.Deck().Cards()) {
            if (candidate.Type() == CardType::Attack) { attack = candidate; break; }
        }
        if (!attack) return 38;
        CardPlay play{};
        play.Card = attack;
        PlayerChoiceContext ctx;
        relic.AfterCardPlayed(ctx, &play);
        relic.AfterCardPlayed(ctx, &play);
        if (f.player.Creature().GetPowerAmount(PowerId::DEXTERITY_POWER) != 0) return 39;
        relic.AfterCardPlayed(ctx, &play);
        if (f.player.Creature().GetPowerAmount(PowerId::DEXTERITY_POWER) != 1) return 40;
        CombatState combat(&f.sim);
        relic.BeforeSideTurnStart(ctx, CombatSide::Player, combat.PlayerCreatures(), combat);
        relic.AfterCardPlayed(ctx, &play);
        relic.AfterCardPlayed(ctx, &play);
        if (f.player.Creature().GetPowerAmount(PowerId::DEXTERITY_POWER) != 1) return 41;
        relic.AfterCardPlayed(ctx, &play);
        if (f.player.Creature().GetPowerAmount(PowerId::DEXTERITY_POWER) != 2) return 42;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::REPTILE_TRINKET, f.player);
        if (!PotionCmd::TryToProcure(PotionId::STRENGTH_POTION, f.player).success) return 43;
        PotionModel potion = f.player.Potions().back();
        relic.AfterPotionUsed(potion, Creature{});
        if (f.player.Creature().GetPowerAmount(PowerId::REPTILE_TRINKET_POWER) != 3 ||
            f.player.Creature().GetPowerAmount(PowerId::STRENGTH_POWER) != 3) return 44;
        PowerModel temporaryStrength = f.player.Creature().GetPower(PowerId::REPTILE_TRINKET_POWER);
        PlayerChoiceContext ctx;
        CombatState combat(&f.sim);
        temporaryStrength.AfterSideTurnEnd(ctx, CombatSide::Player, combat.PlayerCreatures());
        if (f.player.Creature().GetPowerAmount(PowerId::REPTILE_TRINKET_POWER) != 0 ||
            f.player.Creature().GetPowerAmount(PowerId::STRENGTH_POWER) != 0) return 45;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::SCREAMING_FLAGON, f.player);
        CardList hand = f.player.PlayerCombatState().Hand().Cards();
        PlayerChoiceContext ctx;
        CardCmd::Discard(ctx, hand);
        CombatState combat(&f.sim);
        CreatureList enemies = combat.HittableEnemies();
        int expectedDamage = 0;
        int beforeHp = 0;
        for (Creature enemy : enemies) {
            beforeHp += enemy.CurrentHp();
            expectedDamage += std::min(20, enemy.CurrentHp());
        }
        relic.BeforeSideTurnEnd(ctx, CombatSide::Player, combat.PlayerCreatures());
        int afterHp = 0;
        for (Creature enemy : enemies) afterHp += enemy.CurrentHp();
        if (f.player.PlayerCombatState().Hand().Count() != 0 || beforeHp - afterHp != expectedDamage) return 46;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::STONE_CALENDAR, f.player);
        for (int i = 1; i < 7; ++i) f.player.PlayerCombatState().IncrementTurnNumber();
        CombatState combat(&f.sim);
        CreatureList enemies = combat.HittableEnemies();
        int expectedDamage = 0;
        int beforeHp = 0;
        for (Creature enemy : enemies) {
            beforeHp += enemy.CurrentHp();
            expectedDamage += std::min(52, enemy.CurrentHp());
        }
        PlayerChoiceContext ctx;
        relic.AfterSideTurnStart(CombatSide::Player, combat.PlayerCreatures(), combat);
        if (relic.Status() != RelicStatus::Active) return 47;
        relic.BeforeSideTurnEnd(ctx, CombatSide::Player, combat.PlayerCreatures());
        int afterHp = 0;
        for (Creature enemy : enemies) afterHp += enemy.CurrentHp();
        if (beforeHp - afterHp != expectedDamage || relic.Status() != RelicStatus::Normal) return 48;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::STURDY_CLAMP, f.player);
        Creature creature = f.player.Creature();
        CreatureCmd::GainBlock(creature, Dec(18), ValueProp::Unpowered, nullptr);
        if (relic.ShouldClearBlock(creature)) return 49;
        relic.AfterPreventingBlockClear(relic, creature);
        if (creature.Block() != 10) return 50;
    }
    {
        Fixture f;
        RelicModel relic = RelicCmd::Obtain(RelicId::DINGY_RUG, f.player);
        CardCreationOptionsState options = CardCreationOptions::ForRoom(f.player, RoomType::Monster);
        options.flags = options.flags | CardCreationFlags::IsCardReward;
        size_t before = options.pools.size();
        CardCreationOptionsState modified = relic.ModifyCardRewardCreationOptions(f.player, options);
        if (modified.pools.size() != before + 1 || !modified.pools.contains(CardPoolId::COLORLESS_CARD_POOL)) return 51;
        options.flags = options.flags | CardCreationFlags::NoCardPoolModifications;
        CardCreationOptionsState unchanged = relic.ModifyCardRewardCreationOptions(f.player, options);
        if (unchanged.pools.size() != before || unchanged.pools.contains(CardPoolId::COLORLESS_CARD_POOL)) return 52;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::BURNING_STICKS, f.player);
        CardModel skill;
        for (CardPile pile : f.player.PlayerCombatState().AllPiles()) {
            for (CardModel candidate : pile.Cards()) {
                if (candidate.Type() == CardType::Skill) { skill = candidate; break; }
            }
            if (skill) break;
        }
        if (!skill) return 53;
        int before = f.player.PlayerCombatState().Hand().Count();
        PlayerChoiceContext ctx;
        relic.AfterCardExhausted(ctx, skill, false);
        if (f.player.PlayerCombatState().Hand().Count() != before + 1) return 54;
        relic.AfterCardExhausted(ctx, skill, false);
        if (f.player.PlayerCombatState().Hand().Count() != before + 1) return 55;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::POCKETWATCH, f.player);
        CardModel attack;
        for (CardModel candidate : f.player.Deck().Cards()) {
            if (candidate.Type() == CardType::Attack) { attack = candidate; break; }
        }
        if (!attack) return 56;
        CardPlay play{};
        play.Card = attack;
        PlayerChoiceContext ctx;
        for (int i = 0; i < 3; ++i) relic.AfterCardPlayed(ctx, &play);
        f.player.PlayerCombatState().IncrementTurnNumber();
        CombatState combat(&f.sim);
        relic.BeforeSideTurnStart(ctx, CombatSide::Player, combat.PlayerCreatures(), combat);
        relic.AfterSideTurnStart(CombatSide::Player, combat.PlayerCreatures(), combat);
        if (relic.ModifyHandDraw(f.player, Dec(5)) != Dec(8)) return 57;
        for (int i = 0; i < 4; ++i) relic.AfterCardPlayed(ctx, &play);
        f.player.PlayerCombatState().IncrementTurnNumber();
        relic.BeforeSideTurnStart(ctx, CombatSide::Player, combat.PlayerCreatures(), combat);
        if (relic.ModifyHandDraw(f.player, Dec(5)) != Dec(5)) return 58;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::VAMBRACE, f.player);
        relic.BeforeCombatStart();
        CardModel card;
        for (CardModel candidate : f.player.Deck().Cards()) {
            if (candidate.Type() == CardType::Skill) { card = candidate; break; }
        }
        if (!card) return 59;
        CardPlay play{};
        play.Card = card;
        Creature creature = f.player.Creature();
        int before = creature.Block();
        CreatureCmd::GainBlock(creature, Dec(7), ValueProp::Move, &play);
        if (creature.Block() != before + 14) return 60;
        PlayerChoiceContext ctx;
        relic.AfterCardPlayed(ctx, &play);
        if (relic.ModifyBlockMultiplicative(creature, Dec(7), ValueProp::Move, card, &play) != Dec(1)) return 61;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::RIPPLE_BASIN, f.player);
        CombatState combat(&f.sim);
        PlayerChoiceContext ctx;
        Creature creature = f.player.Creature();
        relic.BeforeSideTurnStart(ctx, CombatSide::Player, combat.PlayerCreatures(), combat);
        int before = creature.Block();
        relic.BeforeSideTurnEnd(ctx, CombatSide::Player, combat.PlayerCreatures());
        if (creature.Block() != before + 4) return 62;
        relic.BeforeSideTurnStart(ctx, CombatSide::Player, combat.PlayerCreatures(), combat);
        CardModel attack;
        for (CardModel candidate : f.player.Deck().Cards()) {
            if (candidate.Type() == CardType::Attack) { attack = candidate; break; }
        }
        if (!attack) return 63;
        CardPlay play{};
        play.Card = attack;
        relic.AfterCardPlayed(ctx, &play);
        relic.BeforeSideTurnEnd(ctx, CombatSide::Player, combat.PlayerCreatures());
        if (creature.Block() != before + 4) return 64;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::LAVA_LAMP, f.player);
        AbstractRoom combatRoom(&f.sim, RoomType::Monster);
        relic.AfterRoomEntered(combatRoom);
        CardModel candidate;
        for (CardModel card : f.player.Deck().Cards()) {
            if (card.IsUpgradable()) { candidate = card; break; }
        }
        if (!candidate) return 65;
        CardList options{candidate};
        PlayerChoiceContext ctx;
        DamageResult damage(f.player.Creature(), ValueProp::Unblockable);
        damage.UnblockedDamage = 5;
        relic.AfterDamageReceived(ctx, f.player.Creature(), damage, ValueProp::Unblockable, Creature{}, CardModel{});
        if (!relic.TryModifyCardRewardOptionsLate(f.player, options, CardCreationOptionsState{})) return 66;
        if (options[0] == candidate || options[0].CurrentUpgradeLevel() != 1 || candidate.CurrentUpgradeLevel() != 0) return 67;
        relic.AfterRoomEntered(combatRoom);
        damage.Props = ValueProp::Move;
        relic.AfterDamageReceived(ctx, f.player.Creature(), damage, ValueProp::Move, Creature{}, CardModel{});
        CardList unchanged{candidate};
        if (relic.TryModifyCardRewardOptionsLate(f.player, unchanged, CardCreationOptionsState{})) return 68;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::STONE_CRACKER, f.player);
        CardList draw = f.player.PlayerCombatState().DrawPile().Cards();
        int eligible = 0;
        for (CardModel card : draw) if (card.IsUpgradable()) ++eligible;
        relic.AfterRoomEntered(AbstractRoom(&f.sim, RoomType::Monster));
        int upgraded = 0;
        for (CardModel card : draw) if (card.CurrentUpgradeLevel() > 0) ++upgraded;
        if (upgraded != std::min(2, eligible)) return 69;
    }
    {
        Fixture f;
        RelicModel relic = RelicCmd::Obtain(RelicId::WING_CHARM, f.player);
        CardList options;
        for (CardModel card : f.player.Deck().Cards()) {
            if (card.Type() == CardType::Attack || card.Type() == CardType::Skill) options.push_back(card);
            if (options.size() == 3) break;
        }
        if (options.size() != 3) return 70;
        CardList originals = options;
        if (!relic.TryModifyCardRewardOptionsLate(f.player, options, CardCreationOptionsState{})) return 71;
        int swiftCount = 0;
        for (CardModel card : options) {
            if (card.Enchantment() && card.Enchantment().EnchantmentId_() == EnchantmentId::SWIFT &&
                card.Enchantment().Amount() == 1) ++swiftCount;
        }
        if (swiftCount != 1) return 72;
        for (CardModel card : originals) if (card.Enchantment()) return 73;
    }
    {
        Fixture f;
        const std::array<int32_t, 1> choices{5};  // Proceed past all five rewards.
        f.sim.set_prefix(choices);
        RelicCmd::Obtain(RelicId::CAULDRON, f.player);
        RewardsSet rewards(&f.sim);
        if (rewards.Rewards().size() != 5) return 74;
        for (size_t i = 0; i < rewards.Rewards().size(); ++i)
            if (rewards.Rewards()[i].Type() != RewardType::Potion) return 75;
    }
    {
        Fixture f;
        const std::array<int32_t, 1> choices{5};  // Proceed past all five rewards.
        f.sim.set_prefix(choices);
        RelicCmd::Obtain(RelicId::ORRERY, f.player);
        RewardsSet rewards(&f.sim);
        if (rewards.Rewards().size() != 5) return 76;
        for (size_t i = 0; i < rewards.Rewards().size(); ++i) {
            Reward reward = rewards.Rewards()[i];
            if (reward.Type() != RewardType::Card || reward.Cards().size() != 3) return 77;
            for (CardModel card : reward.Cards())
                if (card.Pool() != CardPoolId::IRONCLAD_CARD_POOL) return 78;
        }
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relic = RelicCmd::Obtain(RelicId::TOOLBOX, f.player);
        const std::array<int32_t, 2> choices{1, 0};  // Select one card, then choose the first option.
        f.sim.set_prefix(choices);
        PlayerChoiceContext context;
        CombatState combat(&f.sim);
        int before = f.player.PlayerCombatState().Hand().Count();
        relic.BeforeHandDraw(f.player, context, combat);
        if (f.player.PlayerCombatState().Hand().Count() != before + 1) return 79;
        if (f.player.PlayerCombatState().Hand().Cards().back().Pool() != CardPoolId::COLORLESS_CARD_POOL) return 80;
    }
    {
        Fixture f;
        CardModel original = f.player.Deck().Cards().front();
        size_t before = f.player.Deck().Cards().size();
        const std::array<int32_t, 2> choices{1, 0};
        f.sim.set_prefix(choices);
        RelicCmd::Obtain(RelicId::DOLLYS_MIRROR, f.player);
        CardList deck = f.player.Deck().Cards();
        if (deck.size() != before + 1 || deck.back().CardId_() != original.CardId_()) return 81;
    }
    {
        Fixture f;
        const std::array<int32_t, 2> choices{1, 0};
        f.sim.set_prefix(choices);
        RelicCmd::Obtain(RelicId::GNARLED_HAMMER, f.player);
        CardModel enchanted;
        for (CardModel card : f.player.Deck().Cards())
            if (card.Enchantment() && card.Enchantment().EnchantmentId_() == EnchantmentId::SHARP) enchanted = card;
        if (!enchanted || enchanted.Enchantment().Amount() != 3 || enchanted.Type() != CardType::Attack) return 82;
    }
    {
        Fixture f;
        const std::array<int32_t, 2> choices{1, 0};
        f.sim.set_prefix(choices);
        RelicCmd::Obtain(RelicId::PUNCH_DAGGER, f.player);
        CardModel enchanted;
        for (CardModel card : f.player.Deck().Cards())
            if (card.Enchantment() && card.Enchantment().EnchantmentId_() == EnchantmentId::MOMENTUM) enchanted = card;
        if (!enchanted || enchanted.Enchantment().Amount() != 5 || enchanted.Type() != CardType::Attack) return 83;
    }
    {
        Fixture f;
        const std::array<int32_t, 2> choices{1, 0};
        f.sim.set_prefix(choices);
        RelicCmd::Obtain(RelicId::ROYAL_STAMP, f.player);
        CardModel enchanted;
        for (CardModel card : f.player.Deck().Cards())
            if (card.Enchantment() && card.Enchantment().EnchantmentId_() == EnchantmentId::ROYALLY_APPROVED)
                enchanted = card;
        if (!enchanted || enchanted.Enchantment().Amount() != 1 ||
            (enchanted.Type() != CardType::Attack && enchanted.Type() != CardType::Skill)) return 84;
    }
    {
        Fixture f;
        RelicCmd::Obtain(RelicId::MEMBERSHIP_CARD, f.player);
        MapCoord shop{};
        bool foundShop = false;
        for (const MapPoint& point : f.state.run.map.points) {
            if (point.type == MapPointType::Shop) {
                shop = point.coord;
                foundShop = true;
                break;
            }
        }
        if (!foundShop) return 85;
        RunManager::EnterMapCoord(&f.sim, shop);
        Merchant::EntryRef entry{Merchant::EntryRef::CharacterCard, 0};
        int rawCost = f.state.run.merchant.character_cards[entry.index].cost;
        int discountedCost = (Dec(rawCost) * (Dec(50) / Dec(100))).to_int();
        if (Merchant::Cost(&f.sim, entry) != discountedCost) return 86;
        int goldBefore = f.player.Gold();
        if (!Merchant::TryPurchase(&f.sim, entry)) return 87;
        if (f.player.Gold() != goldBefore - discountedCost) return 88;
    }
    {
        Fixture f;
        RelicCmd::Obtain(RelicId::THE_COURIER, f.player);
        MapCoord shop{};
        for (const MapPoint& point : f.state.run.map.points) {
            if (point.type == MapPointType::Shop) {
                shop = point.coord;
                break;
            }
        }
        RunManager::EnterMapCoord(&f.sim, shop);
        Merchant::EntryRef entry{Merchant::EntryRef::CharacterCard, 0};
        int rawCost = f.state.run.merchant.character_cards[entry.index].cost;
        int discountedCost = (Dec(rawCost) * (Dec(1) - Dec(20) / Dec(100))).to_int();
        if (Merchant::Cost(&f.sim, entry) != discountedCost) return 89;
        f.player.SetGold(10000);
        uint16_t old_card = f.state.run.merchant.character_cards[entry.index].card;
        if (!Merchant::TryPurchase(&f.sim, entry)) return 114;
        const MerchantCardEntryState& refilled_card = f.state.run.merchant.character_cards[entry.index];
        if (!refilled_card.card || refilled_card.card == old_card || refilled_card.on_sale) return 115;

        Merchant::EntryRef colorless{Merchant::EntryRef::ColorlessCard, 0};
        old_card = f.state.run.merchant.colorless_cards[colorless.index].card;
        if (!Merchant::TryPurchase(&f.sim, colorless)) return 116;
        if (!f.state.run.merchant.colorless_cards[colorless.index].card ||
            f.state.run.merchant.colorless_cards[colorless.index].card == old_card) return 117;
    }
    {
        Fixture f;
        RelicCmd::Obtain(RelicId::THE_COURIER, f.player);
        MapCoord shop{};
        for (const MapPoint& point : f.state.run.map.points)
            if (point.type == MapPointType::Shop) { shop = point.coord; break; }
        RunManager::EnterMapCoord(&f.sim, shop);
        f.player.SetGold(10000);
        Merchant::EntryRef entry{Merchant::EntryRef::Relic, 0};
        RelicRef old_relic = f.state.run.merchant.relics[entry.index].relic;
        if (!Merchant::TryPurchase(&f.sim, entry)) return 118;
        if (!f.state.run.merchant.relics[entry.index].relic ||
            f.state.run.merchant.relics[entry.index].relic == old_relic) return 119;
    }
    {
        Fixture f;
        RelicCmd::Obtain(RelicId::THE_COURIER, f.player);
        MapCoord shop{};
        for (const MapPoint& point : f.state.run.map.points)
            if (point.type == MapPointType::Shop) { shop = point.coord; break; }
        RunManager::EnterMapCoord(&f.sim, shop);
        f.player.SetGold(10000);
        Merchant::EntryRef entry{Merchant::EntryRef::Potion, 0};
        PotionRef old_potion = f.state.run.merchant.potions[entry.index].potion;
        if (!Merchant::TryPurchase(&f.sim, entry)) return 120;
        if (!f.state.run.merchant.potions[entry.index].potion ||
            f.state.run.merchant.potions[entry.index].potion == old_potion) return 121;
    }
    {
        Fixture f;
        MapCoord shop{};
        for (const MapPoint& point : f.state.run.map.points)
            if (point.type == MapPointType::Shop) { shop = point.coord; break; }
        RunManager::EnterMapCoord(&f.sim, shop);
        f.player.SetGold(10000);
        Merchant::EntryRef entry{Merchant::EntryRef::CharacterCard, 0};
        if (!Merchant::TryPurchase(&f.sim, entry)) return 122;
        if (f.state.run.merchant.character_cards[entry.index].card != 0) return 123;
    }
    {
        Fixture f;
        RelicCmd::Obtain(RelicId::THE_COURIER, f.player);
        MapCoord shop{};
        for (const MapPoint& point : f.state.run.map.points)
            if (point.type == MapPointType::Shop) { shop = point.coord; break; }
        RunManager::EnterMapCoord(&f.sim, shop);
        f.player.SetGold(10000);
        const std::array<int32_t, 2> remove_first_card{1, 0};
        f.sim.set_prefix(remove_first_card);
        Merchant::EntryRef entry{Merchant::EntryRef::CardRemoval, 0};
        if (!Merchant::TryPurchase(&f.sim, entry)) return 124;
        if (!f.state.run.merchant.card_removal_used || f.player.st().card_shop_removals_used != 1) return 125;
    }
    {
        Fixture f;
        RelicModel relic = RelicCmd::Obtain(RelicId::UNSETTLING_LAMP, f.player);
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        if (relic.Status() != RelicStatus::Active) return 90;

        CombatState combat(&f.sim);
        Creature enemy = combat.Enemies().front();
        CardList deck = f.player.Deck().Cards();
        CardModel source = deck[0];
        CardModel otherSource = deck[1];
        PlayerChoiceContext ctx;
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, enemy, Dec(2), f.player.Creature(), source);
        if (enemy.GetPowerAmount(PowerId::VULNERABLE_POWER) != 4) return 91;
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, enemy, Dec(2), f.player.Creature(), source);
        if (enemy.GetPowerAmount(PowerId::WEAK_POWER) != 4) return 92;
        PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, enemy, Dec(2), f.player.Creature(), otherSource);
        if (enemy.GetPowerAmount(PowerId::FRAIL_POWER) != 2) return 93;

        CardPlay play{};
        play.Card = source;
        relic.AfterCardPlayed(ctx, &play);
        if (relic.Status() != RelicStatus::Normal) return 94;
        PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, enemy, Dec(2), f.player.Creature(), source);
        if (enemy.GetPowerAmount(PowerId::FRAIL_POWER) != 4) return 95;
        relic.AfterCombatEnd(CombatRoom(&f.sim, RoomType::Monster));
        if (relic.Status() != RelicStatus::Normal) return 102;
        relic.BeforeCombatStart();
        if (relic.Status() != RelicStatus::Active) return 103;
        PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, enemy, Dec(2), f.player.Creature(), source);
        if (enemy.GetPowerAmount(PowerId::FRAIL_POWER) != 8) return 104;
    }
    {
        Fixture f;
        RelicModel relic = RelicCmd::Obtain(RelicId::UNSETTLING_LAMP, f.player);
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        if (relic.Status() != RelicStatus::Active) return 100;
        CombatState combat(&f.sim);
        Creature enemy = combat.Enemies().front();
        CardModel source = f.player.Deck().Cards().front();
        PlayerChoiceContext ctx;
        PowerModel temporary = PowerCmd::Apply(PowerId::DARK_SHACKLES_POWER, ctx, enemy, Dec(2),
                                               f.player.Creature(), source);
        if (!temporary || temporary.Amount() != 4) return 96;
        if (enemy.GetPowerAmount(PowerId::STRENGTH_POWER) != -4) return 97;
    }
    {
        Fixture f;
        RelicModel relic = RelicCmd::Obtain(RelicId::UNSETTLING_LAMP, f.player);
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        if (relic.Status() != RelicStatus::Active) return 99;
        CombatState combat(&f.sim);
        Creature enemy = combat.Enemies().front();
        CardList deck = f.player.Deck().Cards();
        CardModel source = deck[0];
        CardModel otherSource = deck[1];
        PlayerChoiceContext ctx;
        PowerCmd::Apply(PowerId::ARTIFACT_POWER, ctx, enemy, Dec(1), enemy, CardModel{});
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, enemy, Dec(2), f.player.Creature(), source);
        if (enemy.GetPowerAmount(PowerId::VULNERABLE_POWER) != 0) return 98;
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, enemy, Dec(2), f.player.Creature(), otherSource);
        if (enemy.GetPowerAmount(PowerId::WEAK_POWER) != 4) return 101;
    }
    {
        Fixture f;
        RelicModel relic = RelicCmd::Obtain(RelicId::GIRYA, f.player);
        for (int lift = 1; lift <= Girya::kMaxLifts; ++lift) {
            RestSite::BeginRestSite(&f.sim);
            f.state.run.phase = RunPhase::RestSite;
            StaticVec<RunFlow::RestSiteOptionView, 8> options = RunFlow::RestSiteOptions(&f.sim);
            int32_t lift_index = -1;
            for (size_t i = 0; i < options.size(); ++i)
                if (options[i].id == RestSiteOptionId::Lift) lift_index = static_cast<int32_t>(i);
            if (lift_index < 0 || !options[static_cast<size_t>(lift_index)].enabled) return 105;
            if (!RunFlow::ChooseRestSiteOption(&f.sim, lift_index)) return 106;
            Girya girya(&f.sim, relic.Index());
            if (girya.TimesLifted() != lift || girya.DisplayAmount() != lift || !girya.ShowCounter()) return 107;
        }
        RestSite::BeginRestSite(&f.sim);
        f.state.run.phase = RunPhase::RestSite;
        for (const RunFlow::RestSiteOptionView& option : RunFlow::RestSiteOptions(&f.sim))
            if (option.id == RestSiteOptionId::Lift) return 108;
        f.state.run.phase = RunPhase::Map;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        if (f.player.Creature().GetPowerAmount(PowerId::STRENGTH_POWER) != 3) return 109;
    }
    {
        Fixture f;
        RelicCmd::Obtain(RelicId::SHOVEL, f.player);
        RestSite::BeginRestSite(&f.sim);
        f.state.run.phase = RunPhase::RestSite;
        StaticVec<RunFlow::RestSiteOptionView, 8> options = RunFlow::RestSiteOptions(&f.sim);
        int32_t dig_index = -1;
        for (size_t i = 0; i < options.size(); ++i)
            if (options[i].id == RestSiteOptionId::Dig) dig_index = static_cast<int32_t>(i);
        if (dig_index < 0 || !options[static_cast<size_t>(dig_index)].enabled) return 110;
        const size_t relic_count = f.player.Relics().size();
        if (!RunFlow::ChooseRestSiteOption(&f.sim, dig_index)) return 111;
        if (f.player.Relics().size() != relic_count + 1) return 112;
        bool dig_remaining = false;
        for (RestSiteOptionId option : f.state.run.rest_site.options)
            dig_remaining |= option == RestSiteOptionId::Dig;
        if (dig_remaining) return 113;
    }
    {
        Fixture f;
        RelicCmd::Obtain(RelicId::JUZU_BRACELET, f.player);
        RoomTypeSet candidates;
        candidates.Add(RoomType::Monster);
        candidates.Add(RoomType::Event);
        candidates.Add(RoomType::Shop);
        RoomTypeSet filtered = Hook::ModifyUnknownMapPointRoomTypes(RunState(&f.sim), candidates);
        if (filtered.Contains(RoomType::Monster) || !filtered.Contains(RoomType::Event) ||
            !filtered.Contains(RoomType::Shop)) return 126;
        for (int i = 0; i < 64; ++i)
            if (UnknownMapPointOddsOps::Roll(&f.sim, 0) == RoomType::Monster) return 127;
    }
    {
        Fixture f;
        RelicModel base = RelicCmd::Obtain(RelicId::GOLDEN_COMPASS, f.player);
        GoldenCompass relic(base);
        const ActMap& map = f.state.run.map;
        constexpr MapPointType expected[16] = {
            MapPointType::Monster, MapPointType::Unknown, MapPointType::Monster, MapPointType::RestSite,
            MapPointType::Monster, MapPointType::RestSite, MapPointType::Unknown, MapPointType::Treasure,
            MapPointType::Unknown, MapPointType::Treasure, MapPointType::Unknown, MapPointType::Shop,
            MapPointType::Elite, MapPointType::RestSite, MapPointType::Elite, MapPointType::RestSite,
        };
        if (!relic.HasUponPickupEffect() || relic.GoldenPathAct() != 0 || map.cols != 7 || map.rows != 17 ||
            map.points.size() != 18 || map.second_boss >= 0 || map.points[map.start].type != MapPointType::Ancient ||
            map.points[map.boss].type != MapPointType::Boss || map.points[map.boss].coord != MapCoord{3, 17} ||
            map.start_points.size() != 1 || map.points[map.start].children.first() != 1) return 128;
        for (int i = 0; i < 16; ++i) {
            int index = map.point_at(3, i + 1);
            if (index != i + 1 || map.points[index].type != expected[i] ||
                map.points[index].coord != MapCoord{3, static_cast<int8_t>(i + 1)} ||
                map.points[index].parents.size() != 1 || map.points[index].children.size() != 1) return 129;
            if (map.points[index].parents.first() != static_cast<uint8_t>(index - 1) ||
                map.points[index].children.first() != static_cast<uint8_t>(index + 1)) return 130;
        }

        RoomTypeSet candidates;
        for (RoomType type : {RoomType::Monster, RoomType::Elite, RoomType::Treasure, RoomType::Shop, RoomType::Event})
            candidates.Add(type);
        RoomTypeSet act1 = Hook::ModifyUnknownMapPointRoomTypes(RunState(&f.sim), candidates);
        if (act1.bits != (uint16_t{1} << static_cast<unsigned>(RoomType::Event))) return 131;
        RunManager::SetActInternal(&f.sim, 1);
        if (relic.GoldenPathAct() != 0 || f.state.run.map.rows == 17) return 132;
        RoomTypeSet act2 = Hook::ModifyUnknownMapPointRoomTypes(RunState(&f.sim), candidates);
        if (act2 != candidates) return 133;
    }
    {
        Fixture f;
        MawBank relic(RelicCmd::Obtain(RelicId::MAW_BANK, f.player));
        int32_t initialGold = f.player.Gold();
        MapCoord shop{};
        for (const MapPoint& point : f.state.run.map.points)
            if (point.type == MapPointType::Shop) { shop = point.coord; break; }
        RunManager::EnterMapCoord(&f.sim, shop);
        if (f.player.Gold() != initialGold + 12 || relic.IsUsedUp() || relic.Status() == RelicStatus::Disabled ||
            relic.ShouldFlashOnPlayer()) return 134;

        relic.AfterItemPurchased(f.player, Merchant::EntryRef{}, 0);
        if (relic.IsUsedUp() || relic.Status() == RelicStatus::Disabled) return 135;
        f.player.SetGold(10000);
        if (!Merchant::TryPurchase(&f.sim, Merchant::EntryRef{Merchant::EntryRef::CharacterCard, 0})) return 136;
        if (!relic.IsUsedUp() || relic.Status() != RelicStatus::Disabled) return 137;
        int32_t afterPurchaseGold = f.player.Gold();
        relic.AfterRoomEntered(AbstractRoom(&f.sim, RoomType::Monster));
        if (f.player.Gold() != afterPurchaseGold) return 138;
    }
    {
        Fixture f;
        RunState run(&f.sim);
        CardModel card = run.CreateCard(CardId::LANTERN_KEY, f.player);
        CardPileCmd::Add(card, PileType::Deck);
        LanternKey key(card);
        if (key.MaxUpgradeLevel() != 0) return 139;

        EventId originalId = f.state.run.acts[0].rooms.events.front();
        EventModel original = EventModel::Canonical(&f.sim, originalId);
        EventModel act1 = Hook::ModifyNextEvent(run, original);
        if (act1.EventId_() != originalId) return 140;
        RoomTypeSet candidates;
        candidates.Add(RoomType::Monster);
        candidates.Add(RoomType::Event);
        candidates.Add(RoomType::Shop);
        if (Hook::ModifyUnknownMapPointRoomTypes(run, candidates) != candidates) return 141;

        RunManager::SetActInternal(&f.sim, 2);
        EventId act3OriginalId = f.state.run.acts[2].rooms.events.front();
        EventModel act3Original = EventModel::Canonical(&f.sim, act3OriginalId);
        EventModel act3 = Hook::ModifyNextEvent(run, act3Original);
        if (act3.EventId_() != EventId::WAR_HISTORIAN_REPY) return 142;
        RoomTypeSet act3Rooms = Hook::ModifyUnknownMapPointRoomTypes(run, candidates);
        if (act3Rooms.bits != (uint16_t{1} << static_cast<unsigned>(RoomType::Event))) return 143;

        RunManager::GenerateMap(&f.sim);
        MapCoord unknown{};
        bool foundUnknown = false;
        for (const MapPoint& point : f.state.run.map.points) {
            if (point.type == MapPointType::Unknown) {
                unknown = point.coord;
                foundUnknown = true;
                break;
            }
        }
        if (!foundUnknown) return 144;
        RunManager::EnterMapCoord(&f.sim, unknown);
        EventModel historian(&f.sim, 1);
        if (historian.CurrentOptions().size() != 2 ||
            f.state.run.room_event != EventId::WAR_HISTORIAN_REPY ||
            f.state.run.rooms.empty() || f.state.run.rooms.back() != RoomType::Event ||
            !f.state.run.visited_event_ids.contains(EventId::WAR_HISTORIAN_REPY)) return 145;
    }
    {
        Fixture f;
        RelicCmd::Obtain(RelicId::BLACK_BLOOD, f.player);
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        f.player.Creature().SetCurrentHpInternal(30);
        Hook::AfterCombatVictory(RunState(&f.sim), CombatState(&f.sim), CombatRoom(&f.sim, RoomType::Monster));
        // The starting Burning Blood heals 6, then Black Blood heals 12.
        if (f.player.Creature().CurrentHp() != 48) return 146;
    }
    {
        Fixture f;
        RelicCmd::Obtain(RelicId::DARKSTONE_PERIAPT, f.player);
        int32_t initialMaxHp = f.player.Creature().MaxHp();
        CardModel curse = RunState(&f.sim).CreateCard(CardId::INJURY, f.player);
        CardPileCmd::Add(curse, PileType::Deck);
        if (f.player.Creature().MaxHp() != initialMaxHp + 6) return 147;
        CardModel attack = RunState(&f.sim).CreateCard(CardId::STRIKE_IRONCLAD, f.player);
        CardPileCmd::Add(attack, PileType::Deck);
        if (f.player.Creature().MaxHp() != initialMaxHp + 6) return 148;
    }
    {
        Fixture f;
        RelicCmd::Obtain(RelicId::SOZU, f.player);
        if (PotionCmd::TryToProcure(PotionId::STRENGTH_POTION, f.player).failureReason !=
            PotionProcureFailureReason::NotAllowed) return 149;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        if (f.player.PlayerCombatState().MaxEnergy() != 4) return 150;
    }
    {
        Fixture f;
        RelicCmd::Obtain(RelicId::ECTOPLASM, f.player);
        int32_t initialGold = f.player.Gold();
        PlayerCmd::GainGold(100, f.player);
        if (f.player.Gold() != initialGold) return 151;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        if (f.player.PlayerCombatState().MaxEnergy() != 4) return 152;
    }
    {
        Fixture f;
        RelicCmd::Obtain(RelicId::RUNIC_PYRAMID, f.player);
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        if (Hook::ShouldFlush(CombatState(&f.sim), f.player)) return 153;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicCmd::Obtain(RelicId::BRIMSTONE, f.player);
        CombatState combat(&f.sim);
        Hook::AfterSideTurnStart(combat, CombatSide::Player, combat.PlayerCreatures());
        if (f.player.Creature().GetPowerAmount(PowerId::STRENGTH_POWER) != 2) return 154;
        for (Creature enemy : combat.Enemies())
            if (enemy.GetPowerAmount(PowerId::STRENGTH_POWER) != 1) return 155;
        Hook::AfterSideTurnStart(combat, CombatSide::Enemy, combat.Enemies());
        if (f.player.Creature().GetPowerAmount(PowerId::STRENGTH_POWER) != 2) return 156;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicCmd::Obtain(RelicId::SELF_FORMING_CLAY, f.player);
        PlayerChoiceContext ctx;
        CombatState combat(&f.sim);
        DamageResult damage(f.player.Creature(), ValueProp::Move);
        damage.UnblockedDamage = 0;
        Hook::AfterDamageReceived(ctx, RunState(&f.sim), combat, f.player.Creature(), damage,
                                  ValueProp::Move, combat.Enemies().front(), CardModel{});
        if (f.player.Creature().GetPowerAmount(PowerId::SELF_FORMING_CLAY_POWER) != 0) return 157;
        damage.UnblockedDamage = 2;
        Hook::AfterDamageReceived(ctx, RunState(&f.sim), combat, f.player.Creature(), damage,
                                  ValueProp::Move, combat.Enemies().front(), CardModel{});
        Hook::AfterDamageReceived(ctx, RunState(&f.sim), combat, f.player.Creature(), damage,
                                  ValueProp::Move, combat.Enemies().front(), CardModel{});
        if (f.player.Creature().GetPowerAmount(PowerId::SELF_FORMING_CLAY_POWER) != 6) return 158;
        Hook::AfterBlockCleared(combat, f.player.Creature());
        if (f.player.Creature().Block() != 6 ||
            f.player.Creature().GetPowerAmount(PowerId::SELF_FORMING_CLAY_POWER) != 0) return 159;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicCmd::Obtain(RelicId::DEMON_TONGUE, f.player);
        CombatState combat(&f.sim);
        PlayerChoiceContext ctx;
        Creature owner = f.player.Creature();
        owner.SetCurrentHpInternal(50);
        DamageResult damage(owner, ValueProp::Unblockable);
        damage.UnblockedDamage = 5;
        Hook::AfterDamageReceived(ctx, RunState(&f.sim), combat, owner, damage,
                                  ValueProp::Unblockable, owner, CardModel{});
        if (owner.CurrentHp() != 55) return 160;
        Hook::AfterDamageReceived(ctx, RunState(&f.sim), combat, owner, damage,
                                  ValueProp::Unblockable, owner, CardModel{});
        if (owner.CurrentHp() != 55) return 161;
        Hook::BeforeSideTurnStart(combat, CombatSide::Player, combat.PlayerCreatures());
        Hook::AfterDamageReceived(ctx, RunState(&f.sim), combat, owner, damage,
                                  ValueProp::Unblockable, owner, CardModel{});
        if (owner.CurrentHp() != 60) return 162;
        combat.SetCurrentSide(CombatSide::Enemy);
        Hook::BeforeSideTurnStart(combat, CombatSide::Enemy, combat.Enemies());
        Hook::AfterDamageReceived(ctx, RunState(&f.sim), combat, owner, damage,
                                  ValueProp::Unblockable, owner, CardModel{});
        if (owner.CurrentHp() != 60) return 163;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicCmd::Obtain(RelicId::PAPER_PHROG, f.player);
        CombatState combat(&f.sim);
        Creature enemy = combat.Enemies().front();
        PlayerChoiceContext ctx;
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, enemy, Dec(2), f.player.Creature(), CardModel{});
        PowerModel vulnerable = enemy.GetPower(PowerId::VULNERABLE_POWER);
        if (!vulnerable || vulnerable.ModifyDamageMultiplicative(enemy, Dec(10), ValueProp::Move,
                f.player.Creature(), CardModel{}, nullptr) != Dec::lit(175, 2)) return 164;
        if (vulnerable.ModifyDamageMultiplicative(enemy, Dec(10), ValueProp::Unpowered,
                f.player.Creature(), CardModel{}, nullptr) != Dec(1)) return 165;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicCmd::Obtain(RelicId::RUINED_HELMET, f.player);
        PlayerChoiceContext ctx;
        Creature owner = f.player.Creature();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, owner, Dec(2), owner, CardModel{});
        if (owner.GetPowerAmount(PowerId::STRENGTH_POWER) != 4) return 166;
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, owner, Dec(2), owner, CardModel{});
        if (owner.GetPowerAmount(PowerId::STRENGTH_POWER) != 6) return 167;
        RelicModel helmet = f.player.GetRelic(RelicId::RUINED_HELMET);
        helmet.AfterCombatEnd(CombatRoom(&f.sim, RoomType::Monster));
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, owner, Dec(2), owner, CardModel{});
        if (owner.GetPowerAmount(PowerId::STRENGTH_POWER) != 10) return 168;
    }
    {
        Fixture f;
        int32_t beforeMaxHp = f.player.Creature().MaxHp();
        RelicCmd::Obtain(RelicId::BIG_MUSHROOM, f.player);
        if (f.player.Creature().MaxHp() != beforeMaxHp + 20) return 169;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        ModelList modifiers;
        if (Hook::ModifyHandDraw(CombatState(&f.sim), f.player, Dec(5), modifiers) != Dec(3)) return 170;
        f.player.PlayerCombatState().IncrementTurnNumber();
        modifiers.clear();
        if (Hook::ModifyHandDraw(CombatState(&f.sim), f.player, Dec(5), modifiers) != Dec(5)) return 171;
    }
    {
        Fixture f;
        CardModel first = f.player.Deck().Cards().front();
        CardModel second = f.player.Deck().Cards()[1];
        int32_t beforeHp = f.player.Creature().CurrentHp();
        RelicCmd::Obtain(RelicId::FRAGRANT_MUSHROOM, f.player);
        if (f.player.Creature().CurrentHp() != beforeHp - 15) return 172;
        int upgraded = 0;
        for (CardModel card : f.player.Deck().Cards()) upgraded += card.CurrentUpgradeLevel() > 0;
        if (upgraded != 2 || (first.CurrentUpgradeLevel() == 0 && second.CurrentUpgradeLevel() == 0)) return 173;
    }
    {
        Fixture f;
        EventModel event = EventModel::ToMutable(&f.sim, EventId::HUNGRY_FOR_MUSHROOMS);
        event.BeginEvent(f.player, false);
        if (event.CurrentOptions().size() != 2 || event.IsFinished()) return 174;
        event.ChooseOption(0);
        if (!f.player.GetRelic(RelicId::BIG_MUSHROOM) || !event.IsFinished()) return 175;
        if (f.player.Creature().MaxHp() != 100) return 176;
    }
    {
        Fixture f;
        EventModel event = EventModel::ToMutable(&f.sim, EventId::HUNGRY_FOR_MUSHROOMS);
        event.BeginEvent(f.player, false);
        f.player.Creature().SetCurrentHpInternal(50);
        event.ChooseOption(1);
        if (!f.player.GetRelic(RelicId::FRAGRANT_MUSHROOM) || !event.IsFinished() ||
            f.player.Creature().CurrentHp() != 35) return 177;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CONSTRUCT_MENAGERIE_NORMAL);
        CombatState combat(&f.sim);
        if (combat.Enemies().size() != 3 ||
            combat.Enemies()[0].Monster().MonsterId_() != MonsterId::PUNCH_CONSTRUCT ||
            combat.Enemies()[1].Monster().MonsterId_() != MonsterId::CUBEX_CONSTRUCT ||
            combat.Enemies()[2].Monster().MonsterId_() != MonsterId::CUBEX_CONSTRUCT) return 178;
        if (combat.Enemies()[0].GetPowerAmount(PowerId::ARTIFACT_POWER) != 1 ||
            combat.Enemies()[1].GetPowerAmount(PowerId::ARTIFACT_POWER) != 1 ||
            combat.Enemies()[2].GetPowerAmount(PowerId::ARTIFACT_POWER) != 1) return 179;
    }
    {
        Fixture f;
        CardList options;
        const CardList deck = f.player.Deck().Cards();
        for (size_t i = 0; i < 3; ++i) options.push_back(deck[i]);
        const uint64_t defaultHash = hash_state(f.state);
        {
            CardSelectCmd::ScopedDeterministicSelector selector(&f.sim);
            if (!CardSelectCmd::FirstSelectorActive(&f.sim) ||
                f.state.card_selector_mode != CardSelectorMode::FirstN) return 180;
            GameState snapshot = f.state;
            if (snapshot.card_selector_mode != CardSelectorMode::FirstN || hash_state(snapshot) == defaultHash) return 181;

            CardList selected = CardSelectCmd::Select(&f.sim, options, 1, 2, "selector-test");
            if (selected.size() != 2 || selected[0] != options[0] || selected[1] != options[1]) return 182;
            selected = CardSelectCmd::Select(&f.sim, options, 0, 7, "selector-test");
            if (selected.size() != options.size()) return 183;
            for (size_t i = 0; i < selected.size(); ++i) {
                if (selected[i] != options[i]) return 184;
            }
            {
                CardSelectCmd::ScopedDeterministicSelector nested(&f.sim);
            }
            if (!CardSelectCmd::FirstSelectorActive(&f.sim)) return 185;
        }
        if (CardSelectCmd::FirstSelectorActive(&f.sim) ||
            f.state.card_selector_mode != CardSelectorMode::Default) return 186;

        bool needChoiceCaught = false;
        try {
            CardSelectCmd::ScopedDeterministicSelector selector(&f.sim);
            ChoiceRequest request;
            request.kind = ChoiceKind::Option;
            (void)f.sim.Choose(request);
        } catch (const NeedChoice&) {
            needChoiceCaught = true;
        }
        if (!needChoiceCaught || CardSelectCmd::FirstSelectorActive(&f.sim) ||
            f.state.card_selector_mode != CardSelectorMode::Default) return 187;

        CardList rewardOptions;
        for (CardModel card : deck) {
            bool duplicate = false;
            for (CardModel offered : rewardOptions) duplicate |= offered.CardId_() == card.CardId_();
            if (!duplicate) rewardOptions.push_back(f.player.RunState().CreateCard(card.CardId_(), f.player));
            if (rewardOptions.size() == 3) break;
        }
        Reward cardReward = Reward::NewCardReward(f.player, rewardOptions);
        {
            CardSelectCmd::ScopedDeterministicSelector selector(&f.sim);
            if (!cardReward.SelectUnsynchronized()) return 188;
        }
        if (!cardReward.SuccessfullySelected() || rewardOptions[0].Pile().Type() != PileType::Deck ||
            rewardOptions[1].Pile().Type() == PileType::Deck) return 189;

        CardList noOptions;
        Reward emptyReward = Reward::NewCardReward(f.player, noOptions);
        {
            CardSelectCmd::ScopedDeterministicSelector selector(&f.sim);
            if (emptyReward.SelectUnsynchronized()) return 190;
        }
        if (emptyReward.SuccessfullySelected()) return 191;
    }
    {
        Fixture f;
        RunManager::EnterMapCoordDebug(&f.sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        RelicModel relicModel = RelicCmd::Obtain(RelicId::FORGOTTEN_SOUL, f.player);
        ForgottenSoul relic(relicModel);
        CombatState combat(&f.sim);
        CreatureList enemies = combat.HittableEnemies();
        if (enemies.empty()) return 192;

        PlayerChoiceContext ctx;
        Rng expectedRng = f.player.RunState().Rng().CombatTargets();
        const int32_t initialCounter = expectedRng.counter;
        Creature expectedTarget = expectedRng.NextItem(enemies);
        std::array<int, kMaxCreatureList> beforeTotals{};
        for (size_t i = 0; i < enemies.size(); ++i) beforeTotals[i] = enemies[i].CurrentHp() + enemies[i].Block();

        // A foreign/null card must neither draw a target nor deal damage.
        CardModel foreignCard(&f.sim, alloc_card(f.state, CardId::STRIKE_IRONCLAD, 0));
        relic.AfterCardExhausted(ctx, foreignCard, false);
        const Rng& afterForeignCard = f.player.RunState().Rng().CombatTargets();
        if (afterForeignCard.counter != initialCounter) return 193;

        CardModel ownedCard = f.player.PlayerCombatState().Hand().Cards().front();
        relic.AfterCardExhausted(ctx, ownedCard, false);
        const Rng& actualRng = f.player.RunState().Rng().CombatTargets();
        if (!expectedTarget || actualRng.counter != expectedRng.counter ||
            actualRng.random.s0 != expectedRng.random.s0 || actualRng.random.s1 != expectedRng.random.s1 ||
            actualRng.random.s2 != expectedRng.random.s2 || actualRng.random.s3 != expectedRng.random.s3) return 194;
        for (size_t i = 0; i < enemies.size(); ++i) {
            const Creature enemy = enemies[i];
            const int loss = beforeTotals[i] - enemy.CurrentHp() - enemy.Block();
            if (loss != (enemy == expectedTarget ? 1 : 0)) return 195;
        }
    }
    {
        Fixture f;
        CardModel card = f.player.Deck().Cards().front();
        card.AddKeyword(CardKeyword::Exhaust);
        if (!SoulsPower::CanEnchant(card)) return 196;
        CardCmd::Enchant(EnchantmentId::SOULS_POWER, card, Dec(1));
        if (card.Keywords().Contains(CardKeyword::Exhaust) ||
            !card.Enchantment() || card.Enchantment().EnchantmentId_() != EnchantmentId::SOULS_POWER)
            return 197;
        if (SoulsPower::CanEnchant(card)) return 198;
    }
    {
        Fixture f;
        PlayerCmd::SetGold(Dec(300), f.player);
        f.state.run.current_act_index = 1;
        EventModel event = EventModel::ToMutable(&f.sim, EventId::CRYSTAL_SPHERE);
        event.BeginEvent(f.player, false);
        if (event.CurrentOptions().size() != 2 || event.DynamicVars()["UncoverFutureCost"].IntValue() < 51 ||
            event.CurrentOptions()[0].text_key != MakeLocKey("CRYSTAL_SPHERE.pages.INITIAL.options.UNCOVER_FUTURE").hash ||
            event.CurrentOptions()[1].text_key != MakeLocKey("CRYSTAL_SPHERE.pages.INITIAL.options.PAYMENT_PLAN").hash)
            return 199;
        try {
            event.ChooseOption(0);
            return 200;
        } catch (const NeedChoice& need) {
            if (need.request.source != "crystal_sphere" || need.request.option_count != 123 ||
                need.request.options.size() != 123) return 201;
        }
    }
    {
        Fixture f;
        PlayerCmd::SetGold(Dec(300), f.player);
        f.state.run.current_act_index = 1;
        EventModel event = EventModel::ToMutable(&f.sim, EventId::CRYSTAL_SPHERE);
        event.BeginEvent(f.player, false);
        std::array<int32_t, 3> answers = {0, 0, 0};
        f.sim.set_prefix(answers);
        event.ChooseOption(0);
        if (!f.sim.prefix_consumed() || !event.IsFinished() || f.player.Gold() >= 300 || event.Rng().counter <= 1)
            return 202;
    }
    {
        // Golden fixture from the shipped C# CrystalSphereMinigame, SEED0002.
        GameState state;
        new_run(state, NewRunOptions{.seed = "SEED0002", .character = CharacterId::IRONCLAD, .ascension = 0});
        Sim sim(state);
        RunManager::SetActInternal(&sim, 0);
        state.run.current_act_index = 1;
        Player player(&sim, 1);
        PlayerCmd::SetGold(Dec(300), player);
        EventModel event = EventModel::ToMutable(&sim, EventId::CRYSTAL_SPHERE);
        event.BeginEvent(player, false);
        CrystalSphere sphere(&sim, event.Index());
        if (event.DynamicVars()["UncoverFutureCost"].IntValue() != 66 || event.Rng().counter != 1)
            return 203;
        CardPileCmd::AddCurseToDeck(CardId::DEBT, player);
        sphere.InitializeMinigame(6);
        constexpr std::array<std::pair<int, int>, 15> expected_positions = {{{3, 1}, {9, 5}, {0, 5}, {5, 5},
            {7, 7}, {1, 4}, {1, 2}, {6, 9}, {2, 8}, {9, 4}, {9, 8}, {3, 0}, {2, 9}, {3, 10}, {8, 2}}};
        if (event.Rng().counter != 16) return 204;
        for (size_t i = 0; i < expected_positions.size(); ++i)
            if (sphere.item_position_x(static_cast<int>(i)) != expected_positions[i].first ||
                sphere.item_position_y(static_cast<int>(i)) != expected_positions[i].second)
                return 204;
        constexpr std::array<const char*, 11> expected_fog = {{"...HHHHH...", "..HHHHHHH..", ".HHHHHHHHH.",
            "HHHHHHHHHHH", "HHHHHHHHHHH", "HHHHHHHHHHH", "HHHHHHHHHHH", "HHHHHHHHHHH",
            ".HHHHHHHHH.", "..HHHHHHH..", "...HHHHH..."}};
        for (int y = 0; y < 11; ++y) for (int x = 0; x < 11; ++x)
            if (CrystalSphere::hidden(sphere.cell(x, y)) != (expected_fog[static_cast<size_t>(y)][x] == 'H'))
                return 204;
        const std::array<std::pair<int, int>, 6> clicks = {{{5, 5}, {4, 2}, {5, 3}, {3, 4}, {6, 1}, {8, 2}}};
        for (size_t i = 0; i < clicks.size(); ++i) {
            sphere.tool() = i == 4 ? CrystalSphere::kToolSmall : CrystalSphere::kToolBig;
            sphere.RevealAt(CrystalSphere::cell_index(clicks[i].first, clicks[i].second));
            constexpr std::array<size_t, 6> expected_reveal_counts = {1, 1, 1, 1, 2, 3};
            if (sphere.st().crystal_sphere_revealed_order.size() != expected_reveal_counts[i]) return 205;
        }
        const auto& order = sphere.st().crystal_sphere_revealed_order;
        if (order.size() != 3 || order[0] != 3 || order[1] != 0 || order[2] != 14 ||
            event.Rng().counter != 16)
            return 205;
        StaticVec<Reward, 16> rewards;
        for (uint8_t index : order) rewards.push_back(sphere.ItemReward(index));
        if (event.Rng().counter != 17) return 206;
        for (Reward reward : rewards) reward.Populate();
        if (event.Rng().counter != 19 || rewards[0].Potion().PotionId_() != PotionId::FRUIT_JUICE ||
            rewards[1].Relic().RelicId_() != RelicId::VAJRA || rewards[2].Amount() != 30)
            return 207;
    }
    {
        // The C# payment-plan fixture exercises all card rarities, immediate
        // Doubt acquisition, and the event RNG across five rewards.
        GameState state;
        new_run(state, NewRunOptions{.seed = "SEED0002", .character = CharacterId::IRONCLAD, .ascension = 0});
        Sim sim(state);
        RunManager::SetActInternal(&sim, 0);
        state.run.current_act_index = 1;
        Player player(&sim, 1);
        PlayerCmd::SetGold(Dec(300), player);
        EventModel event = EventModel::ToMutable(&sim, EventId::CRYSTAL_SPHERE);
        event.BeginEvent(player, false);
        CardPileCmd::AddCurseToDeck(CardId::DEBT, player);
        CrystalSphere sphere(&sim, event.Index());
        sphere.InitializeMinigame(6);
        const std::array<std::pair<int, int>, 6> clicks = {{{7, 7}, {1, 4}, {1, 2}, {6, 9}, {9, 6}, {0, 6}}};
        for (size_t i = 0; i < clicks.size(); ++i) {
            auto [x, y] = clicks[i];
            sphere.RevealAt(CrystalSphere::cell_index(x, y));
            if (sphere.st().crystal_sphere_revealed_order.size() != i + 1) return 208;
        }
        const auto& order = sphere.st().crystal_sphere_revealed_order;
        constexpr std::array<uint8_t, 6> expected_order = {4, 5, 6, 7, 1, 2};
        if (order.size() != expected_order.size() ||
            !std::equal(order.begin(), order.end(), expected_order.begin())) return 208;
        int debt = 0, doubt = 0;
        for (CardModel card : player.Deck().Cards()) {
            debt += card.CardId_() == CardId::DEBT;
            doubt += card.CardId_() == CardId::DOUBT;
        }
        if (debt != 1 || doubt != 1 || event.Rng().counter != 16) return 209;
        StaticVec<Reward, 16> rewards;
        for (uint8_t index : order) if (index != 7) rewards.push_back(sphere.ItemReward(index));
        if (event.Rng().counter != 18 || rewards.size() != 5) return 210;
        for (Reward reward : rewards) reward.Populate();
        if (event.Rng().counter != 36) {
            std::fprintf(stderr, "Crystal Sphere second reward RNG counter=%llu\n",
                static_cast<unsigned long long>(event.Rng().counter));
            return 211;
        }
        constexpr std::array<std::array<CardId, 3>, 3> expected_cards = {{
            {CardId::BLOOD_WALL, CardId::HEADBUTT, CardId::BODY_SLAM},
            {CardId::HOWL_FROM_BEYOND, CardId::WHIRLWIND, CardId::INFLAME},
            {CardId::PYRE, CardId::STOKE, CardId::PACTS_END},
        }};
        for (size_t i = 0; i < 3; ++i) {
            CardList cards = rewards[i].Cards();
            if (cards.size() != 3) return 212;
            for (size_t j = 0; j < 3; ++j)
                if (cards[j].CardId_() != expected_cards[i][j]) return 212;
        }
        if (rewards[3].Potion().PotionId_() != PotionId::FIRE_POTION ||
            rewards[4].Potion().PotionId_() != PotionId::DEXTERITY_POTION) return 213;
    }
    return 0;
}
