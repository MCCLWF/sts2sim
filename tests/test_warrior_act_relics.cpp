#include <stdexcept>
#include <array>
#include <memory>

#include "engine/run_setup.h"
#include "engine/sim/alloc.h"
#include "game/All.h"
#include "game/Combat/CombatManager.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Hooks/Hook.h"
#include "game/Models/Enchantments/Imbued.h"
#include "game/Models/Relics/AlchemicalCoffer.h"
#include "game/Models/Relics/BingBong.h"
#include "game/Models/Relics/BeautifulBracelet.h"
#include "game/Models/Relics/Claws.h"
#include "game/Models/Relics/DelicateFrond.h"
#include "game/Models/Relics/EmptyCage.h"
#include "game/Models/Relics/ElectricShrymp.h"
#include "game/Models/Relics/JewelryBox.h"
#include "game/Models/Relics/MrStruggles.h"
#include "game/Models/Relics/PhilosophersStone.h"
#include "game/Models/Relics/PrismaticGem.h"
#include "game/Models/Relics/RadiantPearl.h"
#include "game/Models/Relics/SereTalon.h"
#include "game/Models/Relics/SealOfGold.h"
#include "game/Models/Relics/SeaGlass.h"
#include "game/Models/Relics/SignetRing.h"
#include "game/Models/Relics/SneckoEye.h"
#include "game/Models/Relics/Storybook.h"
#include "game/Models/Relics/ThrowingAxe.h"
#include "game/Models/Relics/TanxsWhistle.h"
#include "game/Models/Relics/ToyBox.h"
#include "game/Models/Relics/YummyCookie.h"
#include "game/Runs/CardCreationOptions.h"
#include "game/Runs/RunManager.h"

using namespace sts2;

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

int main() {
    auto touchState = std::make_unique<GameState>();
    new_run(*touchState, NewRunOptions{.seed = "WARRIOROROBAS", .character = CharacterId::IRONCLAD});
    Sim touchSim(*touchState);
    Player player(&touchSim, 1);
    require(IsUsable(ModelKind::Relic, static_cast<uint16_t>(RelicId::TOUCH_OF_OROBAS)),
            "Touch of Orobas was not marked usable");
    RelicCmd::Obtain(RelicId::TOUCH_OF_OROBAS, player);
    require(!player.GetRelic(RelicId::BURNING_BLOOD) && player.GetRelic(RelicId::BLACK_BLOOD),
            "Touch of Orobas did not replace Ironclad's starter relic");

    auto boomerangState = std::make_unique<GameState>();
    new_run(*boomerangState, NewRunOptions{.seed = "WARRIORBOOMERANG", .character = CharacterId::IRONCLAD});
    Sim boomerangSim(*boomerangState);
    Player boomerangPlayer(&boomerangSim, 1);
    require(IsUsable(ModelKind::Relic, static_cast<uint16_t>(RelicId::TRI_BOOMERANG)),
            "Tri Boomerang was not marked usable");
    {
        CardSelectCmd::ScopedDeterministicSelector selector(&boomerangSim);
        RelicCmd::Obtain(RelicId::TRI_BOOMERANG, boomerangPlayer);
    }
    int instinctAttacks = 0;
    for (CardModel card : boomerangPlayer.Deck().Cards()) {
        if (card.Type() != CardType::Attack || !card.Enchantment() ||
            card.Enchantment().EnchantmentId_() != EnchantmentId::INSTINCT) continue;
        ++instinctAttacks;
        require(card.Enchantment().EnchantDamageMultiplicative(Dec(10), ValueProp::Move) == Dec(2),
                "Instinct did not double powered attack damage");
        require(card.Enchantment().EnchantDamageMultiplicative(Dec(10), ValueProp::Move | ValueProp::Unpowered) == Dec(1),
                "Instinct modified unpowered damage");
    }
    require(instinctAttacks == 3, "Tri Boomerang did not enchant three Ironclad attacks");

    auto cofferState = std::make_unique<GameState>();
    new_run(*cofferState, NewRunOptions{.seed = "WARRIORCOFFER", .character = CharacterId::IRONCLAD});
    Sim cofferSim(*cofferState);
    Player cofferPlayer(&cofferSim, 1);
    RelicCmd::Obtain(RelicId::ALCHEMICAL_COFFER, cofferPlayer);
    require(cofferPlayer.MaxPotionCount() == 7 && cofferPlayer.Potions().size() == 4,
            "Alchemical Coffer did not add four slots and procure four potions");

    auto storyState = std::make_unique<GameState>();
    new_run(*storyState, NewRunOptions{.seed = "WARRIORSTORY", .character = CharacterId::IRONCLAD});
    Sim storySim(*storyState);
    Player storyPlayer(&storySim, 1);
    size_t deckBeforeStory = storyPlayer.Deck().Cards().size();
    RelicCmd::Obtain(RelicId::STORYBOOK, storyPlayer);
    require(storyPlayer.Deck().Cards().size() == deckBeforeStory + 1 &&
                storyPlayer.Deck().Cards().back().CardId_() == CardId::BRIGHTEST_FLAME,
            "Storybook did not add Brightest Flame to the deck");

    auto cookieState = std::make_unique<GameState>();
    new_run(*cookieState, NewRunOptions{.seed = "WARRIORCOOKIE", .character = CharacterId::IRONCLAD});
    Sim cookieSim(*cookieState);
    Player cookiePlayer(&cookieSim, 1);
    {
        CardSelectCmd::ScopedDeterministicSelector selector(&cookieSim);
        RelicCmd::Obtain(RelicId::YUMMY_COOKIE, cookiePlayer);
    }
    int upgraded = 0;
    for (CardModel card : cookiePlayer.Deck().Cards()) upgraded += card.CurrentUpgradeLevel() > 0;
    require(upgraded == 4, "Yummy Cookie did not upgrade four deck cards");

    auto perfectFitState = std::make_unique<GameState>();
    new_run(*perfectFitState, NewRunOptions{.seed = "WARRIORPERFECT", .character = CharacterId::IRONCLAD});
    Sim perfectFitSim(*perfectFitState);
    Player perfectFitPlayer(&perfectFitSim, 1);
    RunManager::EnterMapCoordDebug(&perfectFitSim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
    CardList shuffled = perfectFitPlayer.PlayerCombatState().DrawPile().Cards();
    require(!shuffled.empty(), "Perfect Fit test had an empty draw pile");
    CardModel combatFitted = shuffled[0];
    CardCmd::Enchant(EnchantmentId::PERFECT_FIT, combatFitted, Dec(1));
    Hook::ModifyShuffleOrder(combatFitted.Owner().Creature().CombatState(), perfectFitPlayer, shuffled, /*isInitialShuffle=*/false);
    require(!shuffled.empty() && shuffled[0] == combatFitted,
            "Perfect Fit did not move its card to the top after a shuffle");

    auto sneckoState = std::make_unique<GameState>();
    new_run(*sneckoState, NewRunOptions{.seed = "WARRIORSNECKO", .character = CharacterId::IRONCLAD});
    Sim sneckoSim(*sneckoState);
    Player sneckoPlayer(&sneckoSim, 1);
    RelicModel sneckoRelic = RelicCmd::Obtain(RelicId::SNECKO_EYE, sneckoPlayer);
    SneckoEye snecko(&sneckoSim, sneckoRelic.Index());
    require(snecko.ModifyHandDraw(sneckoPlayer, Dec(5)) == Dec(7),
            "Snecko Eye did not increase hand draw by two");
    RunManager::EnterMapCoordDebug(&sneckoSim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
    require(static_cast<bool>(sneckoPlayer.Creature().GetPower(PowerId::CONFUSED_POWER)),
            "Snecko Eye did not apply Confused at combat start");

    auto strugglesState = std::make_unique<GameState>();
    new_run(*strugglesState, NewRunOptions{.seed = "WARRIORSTRUGGLE", .character = CharacterId::IRONCLAD});
    Sim strugglesSim(*strugglesState);
    Player strugglesPlayer(&strugglesSim, 1);
    RunManager::EnterMapCoordDebug(&strugglesSim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
    RelicModel strugglesRelic = RelicCmd::Obtain(RelicId::MR_STRUGGLES, strugglesPlayer);
    Creature struggleEnemy = CombatState(&strugglesSim).Enemies()[0];
    int32_t enemyHp = struggleEnemy.CurrentHp();
    PlayerChoiceContext struggleContext = PlayerChoiceContext::Throwing();
    MrStruggles(&strugglesSim, strugglesRelic.Index()).AfterPlayerTurnStart(struggleContext, strugglesPlayer);
    require(struggleEnemy.CurrentHp() < enemyHp, "Mr. Struggles did not damage enemies at the player's turn start");

    auto sealState = std::make_unique<GameState>();
    new_run(*sealState, NewRunOptions{.seed = "WARRIORSEAL", .character = CharacterId::IRONCLAD});
    Sim sealSim(*sealState);
    Player sealPlayer(&sealSim, 1);
    PlayerCmd::GainGold(10, sealPlayer);
    RunManager::EnterMapCoordDebug(&sealSim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
    RelicModel sealRelic = RelicCmd::Obtain(RelicId::SEAL_OF_GOLD, sealPlayer);
    SealOfGold seal(&sealSim, sealRelic.Index());
    int32_t goldBefore = sealPlayer.Gold();
    int32_t energyBefore = sealPlayer.PlayerCombatState().Energy();
    seal.AfterSideTurnStart(CombatSide::Player, CombatState(&sealSim).PlayerCreatures(), CombatState(&sealSim));
    require(sealPlayer.Gold() == goldBefore - 3 && sealPlayer.PlayerCombatState().Energy() == energyBefore + 1,
            "Seal of Gold did not exchange three gold for one energy at player turn start");

    auto glassState = std::make_unique<GameState>();
    new_run(*glassState, NewRunOptions{.seed = "WARRIORGLASSEYE", .character = CharacterId::IRONCLAD});
    Sim glassSim(*glassState);
    Player glassPlayer(&glassSim, 1);
    std::array<int32_t, 1> glassChoices{5};  // Proceed from the five custom card offers.
    glassSim.set_prefix(glassChoices);
    RelicCmd::Obtain(RelicId::GLASS_EYE, glassPlayer);

    auto bellState = std::make_unique<GameState>();
    new_run(*bellState, NewRunOptions{.seed = "WARRIORCALLINGBELL", .character = CharacterId::IRONCLAD});
    Sim bellSim(*bellState);
    Player bellPlayer(&bellSim, 1);
    std::array<int32_t, 1> bellChoices{3};  // Proceed from the three rarity-tiered relic offers.
    bellSim.set_prefix(bellChoices);
    const size_t deckBeforeBell = bellPlayer.Deck().Cards().size();
    RelicCmd::Obtain(RelicId::CALLING_BELL, bellPlayer);
    bool hasBellCurse = false;
    for (CardModel card : bellPlayer.Deck().Cards()) hasBellCurse |= card.CardId_() == CardId::CURSE_OF_THE_BELL;
    require(bellPlayer.Deck().Cards().size() == deckBeforeBell + 1 && hasBellCurse,
            "Calling Bell did not add Curse of the Bell to Ironclad's deck");

    auto soupState = std::make_unique<GameState>();
    new_run(*soupState, NewRunOptions{.seed = "WARRIORSOUP", .character = CharacterId::IRONCLAD});
    Sim soupSim(*soupState);
    Player soupPlayer(&soupSim, 1);
    RelicCmd::Obtain(RelicId::NUTRITIOUS_SOUP, soupPlayer);
    int emberStrikes = 0;
    for (CardModel card : soupPlayer.Deck().Cards()) {
        if (!card.HasTag(CardTag::Strike) || card.Rarity() != CardRarity::Basic) continue;
        require(card.Enchantment() && card.Enchantment().EnchantmentId_() == EnchantmentId::TEZCATARAS_EMBER,
                "Nutritious Soup did not apply Tezcataras Ember to a basic Strike");
        require(card.Keywords().Contains(CardKeyword::Eternal) && card.EnergyCost().GetWithModifiers(CostModifiers::None) == 0,
                "Tezcataras Ember did not make the Strike free and Eternal");
        require(card.Enchantment().EnchantDamageAdditive(Dec(5), ValueProp::Move) == Dec(3),
                "Tezcataras Ember did not add three powered attack damage");
        require(card.Enchantment().EnchantDamageAdditive(Dec(5), ValueProp::Move | ValueProp::Unpowered) == Dec(0),
                "Tezcataras Ember modified unpowered damage");
        ++emberStrikes;
    }
    require(emberStrikes > 0, "Ironclad had no basic Strike cards for Nutritious Soup");

    auto braceletState = std::make_unique<GameState>();
    new_run(*braceletState, NewRunOptions{.seed = "WARRIORBRACELET", .character = CharacterId::IRONCLAD});
    Sim braceletSim(*braceletState);
    Player braceletPlayer(&braceletSim, 1);
    RelicCmd::Obtain(RelicId::BEAUTIFUL_BRACELET, braceletPlayer);
    int swiftCards = 0;
    for (CardModel card : braceletPlayer.Deck().Cards()) {
        if (!card.Enchantment() || card.Enchantment().EnchantmentId_() != EnchantmentId::SWIFT) continue;
        ++swiftCards;
        require(card.Enchantment().Amount() == 2, "Beautiful Bracelet applied the wrong Swift draw amount");
    }
    require(swiftCards == 4, "Beautiful Bracelet did not enchant four eligible cards");

    auto axeState = std::make_unique<GameState>();
    new_run(*axeState, NewRunOptions{.seed = "WARRIORAXE", .character = CharacterId::IRONCLAD});
    Sim axeSim(*axeState);
    Player axePlayer(&axeSim, 1);
    RelicModel axeRelic = RelicCmd::Obtain(RelicId::THROWING_AXE, axePlayer);
    RunManager::EnterMapCoordDebug(&axeSim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
    CardModel axeCard = axePlayer.PlayerCombatState().Hand().Cards()[0];
    ThrowingAxe axe(&axeSim, axeRelic.Index());
    require(axe.ModifyCardPlayCount(axeCard, Creature{}, 1) == 2,
            "Throwing Axe did not grant the first card an extra play");
    axe.AfterModifyingCardPlayCount(axeCard);
    require(axe.ModifyCardPlayCount(axeCard, Creature{}, 1) == 1,
            "Throwing Axe granted more than one extra play in a combat");

    auto stoneState = std::make_unique<GameState>();
    new_run(*stoneState, NewRunOptions{.seed = "WARRIORSTONE", .character = CharacterId::IRONCLAD});
    Sim stoneSim(*stoneState);
    Player stonePlayer(&stoneSim, 1);
    RelicModel stoneRelic = RelicCmd::Obtain(RelicId::PHILOSOPHERS_STONE, stonePlayer);
    RunManager::EnterMapCoordDebug(&stoneSim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
    PhilosophersStone stone(&stoneSim, stoneRelic.Index());
    require(stonePlayer.PlayerCombatState().MaxEnergy() == 4,
            "Philosopher's Stone did not grant one max energy");
    require(CombatState(&stoneSim).Enemies()[0].GetPower(PowerId::STRENGTH_POWER).Amount() == 1,
            "Philosopher's Stone did not give enemies one Strength");

    auto talonState = std::make_unique<GameState>();
    new_run(*talonState, NewRunOptions{.seed = "WARRIORTALON", .character = CharacterId::IRONCLAD});
    Sim talonSim(*talonState);
    Player talonPlayer(&talonSim, 1);
    const int32_t talonMaxHp = talonPlayer.Creature().MaxHp();
    const size_t talonDeckSize = talonPlayer.Deck().Cards().size();
    RelicCmd::Obtain(RelicId::SERE_TALON, talonPlayer);
    int32_t wishCount = 0;
    for (CardModel card : talonPlayer.Deck().Cards()) wishCount += card.CardId_() == CardId::WISH;
    require(talonPlayer.Creature().MaxHp() == talonMaxHp - 9 &&
                talonPlayer.Deck().Cards().size() == talonDeckSize + 3 && wishCount == 3,
            "Sere Talon did not trade nine max HP for three Wishes");

    auto pearlState = std::make_unique<GameState>();
    new_run(*pearlState, NewRunOptions{.seed = "WARRIORPEARL", .character = CharacterId::IRONCLAD});
    Sim pearlSim(*pearlState);
    Player pearlPlayer(&pearlSim, 1);
    RelicCmd::Obtain(RelicId::RADIANT_PEARL, pearlPlayer);
    RunManager::EnterMapCoordDebug(&pearlSim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
    bool hasLuminesce = false;
    for (CardModel card : pearlPlayer.PlayerCombatState().Hand().Cards())
        hasLuminesce |= card.CardId_() == CardId::LUMINESCE;
    require(hasLuminesce, "Radiant Pearl did not add Luminesce to the first hand");

    auto gemState = std::make_unique<GameState>();
    new_run(*gemState, NewRunOptions{.seed = "WARRIORGEM", .character = CharacterId::IRONCLAD});
    Sim gemSim(*gemState);
    Player gemPlayer(&gemSim, 1);
    RelicModel gemRelic = RelicCmd::Obtain(RelicId::PRISMATIC_GEM, gemPlayer);
    RunManager::EnterMapCoordDebug(&gemSim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
    require(gemPlayer.PlayerCombatState().MaxEnergy() == 4,
            "Prismatic Gem did not grant one max energy");
    require(PrismaticGem(&gemSim, gemRelic.Index()).ModifyCardRewardCreationOptions(
                gemPlayer, CardCreationOptions::ForRoom(gemPlayer, RoomType::Monster)).pools.size() == 1,
            "Prismatic Gem changed the single-player Ironclad reward pool");

    auto frondState = std::make_unique<GameState>();
    new_run(*frondState, NewRunOptions{.seed = "WARRIORFROND", .character = CharacterId::IRONCLAD});
    Sim frondSim(*frondState);
    Player frondPlayer(&frondSim, 1);
    RelicCmd::Obtain(RelicId::DELICATE_FROND, frondPlayer);
    RunManager::EnterMapCoordDebug(&frondSim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
    require(frondPlayer.Potions().size() == static_cast<size_t>(frondPlayer.MaxPotionCount()),
            "Delicate Frond did not fill the open potion slots before combat");

    auto shrympState = std::make_unique<GameState>();
    new_run(*shrympState, NewRunOptions{.seed = "WARRIORSHRYMP", .character = CharacterId::IRONCLAD});
    Sim shrympSim(*shrympState);
    Player shrympPlayer(&shrympSim, 1);
    {
        CardSelectCmd::ScopedDeterministicSelector selector(&shrympSim);
        RelicCmd::Obtain(RelicId::ELECTRIC_SHRYMP, shrympPlayer);
    }
    int imbuedSkills = 0;
    for (CardModel card : shrympPlayer.Deck().Cards())
        imbuedSkills += card.Type() == CardType::Skill && card.Enchantment() &&
                        card.Enchantment().EnchantmentId_() == EnchantmentId::IMBUED;
    require(imbuedSkills == 1, "Electric Shrymp did not Imbue one Ironclad skill");

    auto pickupState = std::make_unique<GameState>();
    new_run(*pickupState, NewRunOptions{.seed = "WARRIORPICKUPS", .character = CharacterId::IRONCLAD});
    Sim pickupSim(*pickupState);
    Player pickupPlayer(&pickupSim, 1);
    const int32_t goldBeforeSignet = pickupPlayer.Gold();
    RelicCmd::Obtain(RelicId::SIGNET_RING, pickupPlayer);
    require(pickupPlayer.Gold() == goldBeforeSignet + 888,
            "Signet Ring did not grant its configured pickup gold");

    auto jewelryState = std::make_unique<GameState>();
    new_run(*jewelryState, NewRunOptions{.seed = "WARRIORJEWELRY", .character = CharacterId::IRONCLAD});
    Sim jewelrySim(*jewelryState);
    Player jewelryPlayer(&jewelrySim, 1);
    RelicCmd::Obtain(RelicId::JEWELRY_BOX, jewelryPlayer);
    require(jewelryPlayer.Deck().Cards().back().CardId_() == CardId::APOTHEOSIS,
            "Jewelry Box did not add Apotheosis to the deck");

    auto whistleState = std::make_unique<GameState>();
    new_run(*whistleState, NewRunOptions{.seed = "WARRIORWHISTLE", .character = CharacterId::IRONCLAD});
    Sim whistleSim(*whistleState);
    Player whistlePlayer(&whistleSim, 1);
    RelicCmd::Obtain(RelicId::TANXS_WHISTLE, whistlePlayer);
    require(whistlePlayer.Deck().Cards().back().CardId_() == CardId::WHISTLE,
            "Tanx's Whistle did not add Whistle to the deck");

    auto cageState = std::make_unique<GameState>();
    new_run(*cageState, NewRunOptions{.seed = "WARRIORCAGE", .character = CharacterId::IRONCLAD});
    Sim cageSim(*cageState);
    Player cagePlayer(&cageSim, 1);
    const size_t deckBeforeCage = cagePlayer.Deck().Cards().size();
    {
        CardSelectCmd::ScopedDeterministicSelector selector(&cageSim);
        RelicCmd::Obtain(RelicId::EMPTY_CAGE, cagePlayer);
    }
    require(cagePlayer.Deck().Cards().size() == deckBeforeCage - 2,
            "Empty Cage did not remove two selected deck cards");

    auto clawsState = std::make_unique<GameState>();
    new_run(*clawsState, NewRunOptions{.seed = "WARRIORCLAWS", .character = CharacterId::IRONCLAD});
    Sim clawsSim(*clawsState);
    Player clawsPlayer(&clawsSim, 1);
    {
        CardSelectCmd::ScopedDeterministicSelector selector(&clawsSim);
        RelicCmd::Obtain(RelicId::CLAWS, clawsPlayer);
    }
    int mauls = 0;
    for (CardModel card : clawsPlayer.Deck().Cards()) mauls += card.CardId_() == CardId::MAUL;
    require(mauls == 6, "Claws did not transform six deck cards into Maul");

    auto seaState = std::make_unique<GameState>();
    new_run(*seaState, NewRunOptions{.seed = "WARRIORSEAGLASS", .character = CharacterId::IRONCLAD});
    Sim seaSim(*seaState);
    Player seaPlayer(&seaSim, 1);
    RelicModel seaGlass(&seaSim, alloc_relic(*seaState, RelicId::SEA_GLASS, 0));
    seaGlass.st().extra[0] = static_cast<int32_t>(CharacterId::SILENT);
    const size_t deckBeforeSeaGlass = seaPlayer.Deck().Cards().size();
    std::array<int32_t, 2> seaGlassChoices{1, 0};  // Select one card, then the first displayed candidate.
    seaSim.set_prefix(seaGlassChoices);
    RelicCmd::Obtain(seaGlass, seaPlayer);
    require(seaPlayer.Deck().Cards().size() == deckBeforeSeaGlass + 1 &&
                sts2::data(seaPlayer.Deck().Cards().back().CardId_()).pool ==
                    sts2::data(CharacterId::SILENT).card_pool,
            "Sea Glass did not add a selected card from its saved character pool");

    auto bingState = std::make_unique<GameState>();
    new_run(*bingState, NewRunOptions{.seed = "WARRIORBINGBONG", .character = CharacterId::IRONCLAD});
    Sim bingSim(*bingState);
    Player bingPlayer(&bingSim, 1);
    RelicCmd::Obtain(RelicId::BING_BONG, bingPlayer);
    CardModel addedByBingTest = bingPlayer.RunState().CreateCard(CardId::IRON_WAVE, bingPlayer);
    CardPileCmd::Add(addedByBingTest, PileType::Deck);
    int ironWaves = 0;
    for (CardModel card : bingPlayer.Deck().Cards()) ironWaves += card.CardId_() == CardId::IRON_WAVE;
    require(ironWaves == 2, "Bing Bong did not duplicate a card entering the permanent deck exactly once");

    auto toyState = std::make_unique<GameState>();
    new_run(*toyState, NewRunOptions{.seed = "WARRIORTOYBOX", .character = CharacterId::IRONCLAD});
    Sim toySim(*toyState);
    Player toyPlayer(&toySim, 1);
    {
        CardSelectCmd::ScopedDeterministicSelector selector(&toySim);
        std::array<int32_t, 6> toyChoices{0, 1, 2, 3, 4, 5};
        toySim.set_prefix(toyChoices);
        RelicCmd::Obtain(RelicId::TOY_BOX, toyPlayer);
    }
    int waxRelics = 0;
    for (RelicModel relic : toyPlayer.Relics()) waxRelics += relic.IsWax();
    require(waxRelics == 5, "Toy Box did not grant five wax relics on pickup");
}
