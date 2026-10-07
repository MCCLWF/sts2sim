#include <span>

#include "engine/run_setup.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Combat/CombatManager.h"
#include "game/Combat/History/CombatHistory.h"
#include "game/Hooks/Hook.h"
#include "game/Runs/RunManager.h"

using namespace sts2;

struct Fixture {
    GameState state;
    Sim sim;
    Player player;
    Creature enemy;
    Fixture() : sim(state), player(&sim, 1) {
        new_run(state, NewRunOptions{.seed = "COLORLESSBATCH", .character = CharacterId::IRONCLAD, .ascension = 0});
        RunManager::SetActInternal(&sim, 0);
        RunManager::EnterMapCoordDebug(&sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        enemy = CombatState(&sim).Enemies().front();
    }
};

static CardModel add(Fixture& f, CardId id) {
    CardModel card = CombatState(&f.sim).CreateCard(id, f.player);
    CardPileCmd::Add(card, PileType::Hand);
    return card;
}

static void play(CardModel card, Creature target = {}) {
    PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
    CardCmd::AutoPlay(ctx, card, target);
}

int main() {
    {
        Fixture f;
        CardModel card = add(f, CardId::DARK_SHACKLES);
        play(card, f.enemy);
        if (f.enemy.GetPowerAmount(PowerId::STRENGTH_POWER) != -9) return 1;
        PlayerChoiceContext ctx;
        f.enemy.GetPower(PowerId::DARK_SHACKLES_POWER).AfterSideTurnEnd(ctx, CombatSide::Enemy, CreatureList{f.enemy});
        if (f.enemy.GetPowerAmount(PowerId::STRENGTH_POWER) != 0) return 2;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::MASTER_OF_STRATEGY);
        play(card, f.player.Creature());
        for (CardModel item : f.player.PlayerCombatState().Hand().Cards()) if (item == card) return 3;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::ENLIGHTENMENT);
        CardModel strike = f.player.PlayerCombatState().Hand().Cards().front();
        play(card, f.player.Creature());
        if (strike.EnergyCost().GetWithModifiers(CostModifiers::All) > 1) return 4;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::DISCOVERY);
        const int32_t answer[] = {0};
        f.sim.set_prefix(std::span<const int32_t>(answer));
        play(card, f.player.Creature());
        for (CardModel item : f.player.PlayerCombatState().Hand().Cards()) if (item == card) return 5;
    }
    {
        Fixture f;
        f.enemy.SetCurrentHpInternal(10);
        CardModel card = add(f, CardId::HAND_OF_GREED);
        int gold = f.player.Gold();
        play(card, f.enemy);
        if (!f.enemy.IsDead() || f.player.Gold() != gold + 20) return 6;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::PANIC_BUTTON);
        play(card, f.player.Creature());
        if (f.player.Creature().Block() < 30 ||
            f.player.Creature().GetPowerAmount(PowerId::NO_BLOCK_POWER) != 2) return 7;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::FASTEN);
        play(card, f.player.Creature());
        PowerModel power = f.player.Creature().GetPower(PowerId::FASTEN_POWER);
        CardModel defend = f.player.PlayerCombatState().Hand().Cards().front();
        if (power.ModifyBlockAdditive(f.player.Creature(), Dec(0), ValueProp::Move, defend, nullptr) != Dec(4)) return 8;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::EQUILIBRIUM);
        play(card, f.player.Creature());
        if (f.player.Creature().Block() < 13 ||
            f.player.Creature().GetPowerAmount(PowerId::RETAIN_HAND_POWER) != 1) return 9;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::PURITY);
        const int32_t answers[] = {1, 0};
        f.sim.set_prefix(std::span<const int32_t>(answers));
        play(card, f.player.Creature());
        if (!f.sim.prefix_consumed() || !f.player.PlayerCombatState().ExhaustPile().Cards().size()) return 10;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::SCRAWL);
        CardCmd::Upgrade(card);
        if (!card.Keywords().Contains(CardKeyword::Retain)) return 11;
        play(card, f.player.Creature());
        if (f.player.PlayerCombatState().Hand().Count() != CardPile::MaxCardsInHand) return 13;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::SALVO);
        int before = f.enemy.CurrentHp();
        play(card, f.enemy);
        if (f.enemy.CurrentHp() >= before || f.player.Creature().GetPowerAmount(PowerId::RETAIN_HAND_POWER) != 1) return 12;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::ENTROPY);
        play(card, f.player.Creature());
        if (f.player.Creature().GetPowerAmount(PowerId::ENTROPY_POWER) != 1) return 14;
        const int32_t answers[] = {1, 0};
        f.sim.set_prefix(std::span<const int32_t>(answers));
        PlayerChoiceContext ctx;
        f.player.Creature().GetPower(PowerId::ENTROPY_POWER).AfterPlayerTurnStart(ctx, f.player);
        if (!f.sim.prefix_consumed()) return 15;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::RESTLESSNESS);
        PlayerChoiceContext setup = PlayerChoiceContext::Throwing();
        CardList hand = f.player.PlayerCombatState().Hand().Cards();
        for (CardModel item : hand) {
            if (item != card) CardCmd::Exhaust(setup, item);
        }
        int before = f.player.PlayerCombatState().Energy();
        play(card, f.player.Creature());
        if (f.player.PlayerCombatState().Energy() != before + 2) return 16;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::ANOINTED);
        CardCmd::Upgrade(card);
        if (!card.Keywords().Contains(CardKeyword::Retain)) return 17;
        play(card, f.player.Creature());
        if (f.player.PlayerCombatState().Hand().Count() > CardPile::MaxCardsInHand) return 18;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::CATASTROPHE);
        play(card, f.player.Creature());
        if (f.player.PlayerCombatState().Hand().Count() > CardPile::MaxCardsInHand) return 19;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::THE_BOMB);
        play(card, f.player.Creature());
        PowerModel bomb = f.player.Creature().GetPower(PowerId::THE_BOMB_POWER);
        if (!bomb || bomb.Amount() != 3 || bomb.DynamicVars().Damage().BaseValue() != Dec(40)) return 20;
        PlayerChoiceContext ctx;
        bomb.BeforeSideTurnEnd(ctx, CombatSide::Player, CreatureList{f.player.Creature()});
        if (bomb.Amount() != 2) return 21;
    }
    {
        Fixture f;
        play(add(f, CardId::PREP_TIME), f.player.Creature());
        PowerModel power = f.player.Creature().GetPower(PowerId::PREP_TIME_POWER);
        if (!power || power.Amount() != 4) return 22;
        power.AfterSideTurnStart(CombatSide::Player, CreatureList{f.player.Creature()}, CombatState(&f.sim));
        if (f.player.Creature().GetPowerAmount(PowerId::VIGOR_POWER) != 4) return 23;
    }
    {
        Fixture f;
        play(add(f, CardId::ROLLING_BOULDER), f.player.Creature());
        PowerModel power = f.player.Creature().GetPower(PowerId::ROLLING_BOULDER_POWER);
        if (!power || power.Amount() != 5) return 24;
        int before = f.enemy.CurrentHp();
        PlayerChoiceContext ctx;
        power.AfterPlayerTurnStart(ctx, f.player);
        if (f.enemy.CurrentHp() >= before || power.Amount() != 10) return 25;
    }
    {
        Fixture f;
        play(add(f, CardId::PANACHE), f.player.Creature());
        PowerModel power = f.player.Creature().GetPower(PowerId::PANACHE_POWER);
        if (!power || power.DynamicVars()["CardsLeft"].IntValue() != 5) return 26;
        CardModel card = add(f, CardId::DEFEND_IRONCLAD);
        CardPlay cardPlay{.Card = card, .Player = f.player};
        PlayerChoiceContext ctx;
        power.AfterCardPlayed(ctx, &cardPlay);
        if (power.DynamicVars()["CardsLeft"].IntValue() != 4) return 27;
    }
    {
        Fixture f;
        play(add(f, CardId::THE_GAMBIT), f.player.Creature());
        PowerModel power = f.player.Creature().GetPower(PowerId::THE_GAMBIT_POWER);
        if (!power || f.player.Creature().Block() < 50) return 28;
        DamageResult result(f.player.Creature(), ValueProp::Move);
        result.UnblockedDamage = 1;
        PlayerChoiceContext ctx;
        power.AfterDamageReceived(ctx, f.player.Creature(), result, ValueProp::Move, f.enemy, CardModel{});
        if (!f.player.Creature().IsDead()) return 29;
    }
    {
        Fixture f;
        EventModel event = EventModel::ToMutable(&f.sim, EventId::BUGSLAYER);
        event.BeginEvent(f.player, false);
        if (event.CurrentOptions().size() != 2 || event.IsFinished()) return 35;
        event.ChooseOption(0);
        bool found = false;
        for (CardModel card : f.player.Deck().Cards()) found |= card.CardId_() == CardId::EXTERMINATE;
        if (!found || !event.IsFinished()) return 36;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::EXTERMINATE);
        int before = f.enemy.CurrentHp();
        play(card, f.player.Creature());
        if (f.enemy.CurrentHp() != before - 12) return 37;
        CardCmd::Upgrade(card);
        if (card.DynamicVars().Damage().BaseValue() != Dec(4)) return 38;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::SQUASH);
        int before = f.enemy.CurrentHp();
        play(card, f.enemy);
        if (f.enemy.CurrentHp() != before - 10 || f.enemy.GetPowerAmount(PowerId::VULNERABLE_POWER) != 2) return 39;
        CardCmd::Upgrade(card);
        if (card.DynamicVars().Damage().BaseValue() != Dec(12) ||
            card.DynamicVars().Vulnerable().BaseValue() != Dec(3)) return 40;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::BAD_LUCK);
        int before = f.player.Creature().CurrentHp();
        PlayerChoiceContext ctx;
        card.OnTurnEndInHand(ctx);
        if (f.player.Creature().CurrentHp() != before - 13 || !card.Keywords().Contains(CardKeyword::Unplayable) ||
            !card.Keywords().Contains(CardKeyword::Eternal)) return 41;
    }
    {
        Fixture f;
        play(add(f, CardId::AUTOMATION), f.player.Creature());
        PowerModel power = f.player.Creature().GetPower(PowerId::AUTOMATION_POWER);
        if (!power || power.Amount() != 1) return 30;
        CardModel drawn = f.player.PlayerCombatState().Hand().Cards().front();
        int energyBefore = f.player.PlayerCombatState().Energy();
        PlayerChoiceContext ctx;
        for (int i = 0; i < 10; ++i) power.AfterCardDrawn(ctx, drawn, true);
        if (f.player.PlayerCombatState().Energy() != energyBefore + 1) return 31;
    }
    {
        Fixture f;
        int before = f.player.Potions().size();
        play(add(f, CardId::ALCHEMIZE), f.player.Creature());
        if (f.player.Potions().size() != static_cast<size_t>(before + 1)) return 32;
    }
    {
        Fixture f;
        CardList before = f.player.PlayerCombatState().Hand().Cards();
        CardModel jack = add(f, CardId::JACK_OF_ALL_TRADES);
        play(jack, f.player.Creature());
        int generated = 0;
        for (CardModel card : f.player.PlayerCombatState().Hand().Cards()) {
            if (before.contains(card)) continue;
            if (card.CardId_() == CardId::JACK_OF_ALL_TRADES || card.Rarity() == CardRarity::Basic ||
                card.Rarity() == CardRarity::Ancient || card.Rarity() == CardRarity::Event) return 33;
            ++generated;
        }
        if (generated != 1) return 34;
    }
    {
        Fixture f;
        CardList draw = f.player.PlayerCombatState().DrawPile().Cards();
        CardPileCmd::Add(draw, PileType::Discard);
        CardModel strike = CombatState(&f.sim).CreateCard(CardId::STRIKE_IRONCLAD, f.player);
        CardPileCmd::Add(strike, PileType::Draw);
        play(add(f, CardId::HIDDEN_GEM), f.player.Creature());
        if (strike.BaseReplayCount() != 2) return 35;
    }
    {
        Fixture f;
        play(add(f, CardId::STRIKE_IRONCLAD), f.enemy);
        CardModel card = add(f, CardId::GOLD_AXE);
        int hp = f.enemy.CurrentHp();
        play(card, f.enemy);
        if (hp - f.enemy.CurrentHp() != 1 || CombatState(&f.sim).st().cards_played_this_combat != 2) return 36;
        CardCmd::Upgrade(card);
        if (!card.Keywords().Contains(CardKeyword::Retain)) return 37;
    }
    {
        Fixture f;
        PlayerChoiceContext ctx;
        PowerCmd::Apply(PowerId::DARK_SHACKLES_POWER, ctx, f.enemy, Dec(2), f.player.Creature(), CardModel{});
        CardModel card = add(f, CardId::REND);
        CardCmd::Upgrade(card);
        int hp = f.enemy.CurrentHp();
        play(card, f.enemy);
        int actualDamage = hp - f.enemy.CurrentHp();
        if (actualDamage != 20) return 38;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::BOLAS);
        int hp = f.enemy.CurrentHp();
        play(card, f.enemy);
        if (hp - f.enemy.CurrentHp() != 3) return 44;
        if (card.st().last_played_turn != 1) return 45;
        PlayerChoiceContext ctx;
        Hook::BeforeHandDraw(CombatState(&f.sim), f.player, ctx);
        if (f.player.PlayerCombatState().Hand().Cards().contains(card)) return 46;
        f.player.PlayerCombatState().IncrementTurnNumber();
        Hook::BeforeHandDraw(CombatState(&f.sim), f.player, ctx);
        if (!f.player.PlayerCombatState().Hand().Cards().contains(card)) return 47;
        int handCount = f.player.PlayerCombatState().Hand().Count();
        Hook::BeforeHandDraw(CombatState(&f.sim), f.player, ctx);
        if (f.player.PlayerCombatState().Hand().Count() != handCount) return 48;
        CardCmd::Upgrade(card);
        if (card.DynamicVars().Damage().BaseValue() != Dec(4)) return 49;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::THRUMMING_HATCHET);
        int hp = f.enemy.CurrentHp();
        play(card, f.enemy);
        if (hp - f.enemy.CurrentHp() != 11) return 50;
        f.player.PlayerCombatState().IncrementTurnNumber();
        PlayerChoiceContext ctx;
        Hook::BeforeHandDraw(CombatState(&f.sim), f.player, ctx);
        if (!f.player.PlayerCombatState().Hand().Cards().contains(card)) return 51;
        CardCmd::Upgrade(card);
        if (card.DynamicVars().Damage().BaseValue() != Dec(14)) return 52;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::BOLAS);
        play(card, f.enemy);
        f.player.PlayerCombatState().IncrementTurnNumber();
        f.player.PlayerCombatState().IncrementTurnNumber();
        PlayerChoiceContext ctx;
        Hook::BeforeHandDraw(CombatState(&f.sim), f.player, ctx);
        if (f.player.PlayerCombatState().Hand().Cards().contains(card)) return 53;
    }
    {
        Fixture f;
        CardList startingHand = f.player.PlayerCombatState().Hand().Cards();
        CardModel jackpot = add(f, CardId::JACKPOT);
        CardCmd::Upgrade(jackpot);
        int hp = f.enemy.CurrentHp();
        play(jackpot, f.enemy);
        if (hp - f.enemy.CurrentHp() != 30) return 54;
        int generated = 0;
        for (CardModel card : f.player.PlayerCombatState().Hand().Cards()) {
            if (startingHand.contains(card) || card == jackpot) continue;
            if (card.EnergyCost().Canonical() != 0 || card.EnergyCost().CostsX()) return 55;
            if (card.IsUpgradable() && card.CurrentUpgradeLevel() == 0) return 56;
            ++generated;
        }
        if (generated != 3) return 57;
    }
    {
        Fixture f;
        CardModel mayhem = add(f, CardId::MAYHEM);
        play(mayhem, f.player.Creature());
        PowerModel power = f.player.Creature().GetPower(PowerId::MAYHEM_POWER);
        if (!power || power.Amount() != 1) return 58;
        CardModel strike = CombatState(&f.sim).CreateCard(CardId::STRIKE_IRONCLAD, f.player);
        CardPileCmd::Add(strike, PileType::Draw, CardPilePosition::Top);
        int hp = f.enemy.CurrentHp();
        PlayerChoiceContext ctx;
        Hook::AfterAutoPrePlayPhaseEntered(ctx, CombatState(&f.sim), f.player);
        if (hp - f.enemy.CurrentHp() != 6) return 59;
        if (!f.player.PlayerCombatState().DiscardPile().Cards().contains(strike)) return 60;
        if (strike.ExhaustOnNextPlay()) return 61;
    }
    {
        Fixture f;
        CardModel nostalgia = add(f, CardId::NOSTALGIA);
        CardCmd::Upgrade(nostalgia);
        if (nostalgia.EnergyCost().GetWithModifiers(CostModifiers::All) != 0) return 62;
        play(nostalgia, f.player.Creature());
        if (f.player.Creature().GetPowerAmount(PowerId::NOSTALGIA_POWER) != 1) return 63;
        CardModel first = add(f, CardId::STRIKE_IRONCLAD);
        play(first, f.enemy);
        if (!f.player.PlayerCombatState().DrawPile().Cards().contains(first) || first.Pile().Type() != PileType::Draw)
            return 64;
        if (f.player.PlayerCombatState().DrawPile().Cards().front() != first) return 65;
        if (CombatState(&f.sim).st().player_attack_skill_cards_started_this_turn != 1) return 66;
        CardModel second = add(f, CardId::DEFEND_IRONCLAD);
        play(second, f.player.Creature());
        if (!f.player.PlayerCombatState().DiscardPile().Cards().contains(second)) return 67;
        if (CombatState(&f.sim).st().player_attack_skill_cards_started_this_turn != 2) return 68;
    }
    {
        // Native NostalgiaPower counts CardPlayStarted entries only when
        // CombatHistoryEntry.HappenedThisTurn matches the current side,
        // round and player TurnNumber. SwitchSides increments TurnNumber
        // for Ambergris's extra turn even though side and round stay put.
        // Hellraiser auto-plays a drawn Strike; Nostalgia puts its first play
        // back on top, so the next draw plays it again for 6 + 6 damage.
        Fixture f;
        CombatState combat(&f.sim);
        for (Creature enemy : combat.Enemies()) {
            enemy.SetMaxHpInternal(Dec(200));
            enemy.SetCurrentHpInternal(Dec(200));
        }
        play(add(f, CardId::NOSTALGIA), f.player.Creature());
        play(add(f, CardId::STRIKE_IRONCLAD), f.enemy);
        play(add(f, CardId::DEFEND_IRONCLAD), f.player.Creature());
        if (combat.st().player_attack_skill_cards_started_this_turn != 2) return 81;
        PlayerChoiceContext ctx;
        PowerCmd::Apply(PowerId::HELLRAISER_POWER, ctx, f.player.Creature(), Dec(1),
                        f.player.Creature(), CardModel{});
        PowerCmd::Apply(PowerId::AMBERGRIS_POWER, ctx, f.player.Creature(), Dec(1),
                        f.player.Creature(), CardModel{});
        auto prepareDraw = [&] {
            CardList oldDraw = f.player.PlayerCombatState().DrawPile().Cards();
            CardPileCmd::Add(oldDraw, PileType::Exhaust);
            CardModel strike = combat.CreateCard(CardId::STRIKE_IRONCLAD, f.player);
            CardPileCmd::Add(strike, PileType::Draw);
            for (int i = 0; i < 4; ++i) {
                CardModel defend = combat.CreateCard(CardId::DEFEND_IRONCLAD, f.player);
                CardPileCmd::Add(defend, PileType::Draw);
            }
            return strike;
        };
        auto enemyHp = [&] {
            int hp = 0;
            for (Creature enemy : combat.Enemies()) hp += enemy.CurrentHp();
            return hp;
        };
        int round = combat.RoundNumber();
        int turn = f.player.PlayerCombatState().TurnNumber();
        CardModel extraStrike = prepareDraw();
        int hp = enemyHp();
        CombatManager(&f.sim).EndPlayerTurn();
        if (combat.RoundNumber() != round || f.player.PlayerCombatState().TurnNumber() != turn + 1 ||
            !combat.st().player_taking_extra_turn) return 82;
        if (hp - enemyHp() != 12 || combat.st().player_attack_skill_cards_started_this_turn != 2 ||
            extraStrike.Pile().Type() != PileType::Discard) return 83;
        CardModel skill = add(f, CardId::DEFEND_IRONCLAD);
        play(skill, f.player.Creature());
        if (combat.st().player_attack_skill_cards_started_this_turn != 3 ||
            skill.Pile().Type() != PileType::Discard) return 84;
        CardModel normalStrike = prepareDraw();
        hp = enemyHp();
        CombatManager(&f.sim).EndPlayerTurn();
        if (combat.RoundNumber() != round + 1 || f.player.PlayerCombatState().TurnNumber() != turn + 2 ||
            combat.st().player_taking_extra_turn) return 85;
        if (hp - enemyHp() != 12 || combat.st().player_attack_skill_cards_started_this_turn != 2 ||
            normalStrike.Pile().Type() != PileType::Discard) return 86;
    }
    {
        // Juggling.AfterApplied initializes from the native STARTED Attack
        // history, which is distinct from Stomp's FINISHED Attack history.
        // Cover natural removal/reapplication as well as entries whose play
        // has started but has not finished, so using the finished count fails.
        for (bool startedOnly : {false, true}) {
            Fixture f;
            CombatState combat(&f.sim);
            for (Creature enemy : combat.Enemies()) {
                enemy.SetMaxHpInternal(Dec(200));
                enemy.SetCurrentHpInternal(Dec(200));
            }
            if (!startedOnly) play(add(f, CardId::JUGGLING), f.player.Creature());
            for (int i = 0; i < 2; ++i) {
                CardModel attack = add(f, CardId::STRIKE_IRONCLAD);
                if (startedOnly) {
                    CardPlay entry{.Card = attack, .Player = f.player};
                    CombatManager(&f.sim).History().CardPlayStarted(combat, entry);
                } else {
                    play(attack, f.enemy);
                }
            }
            if (!startedOnly) PowerCmd::Remove(PowerId::JUGGLING_POWER, f.player.Creature());
            if (combat.st().player_attack_cards_started_this_turn != 2 ||
                combat.st().player_attack_cards_this_turn != (startedOnly ? 0 : 2)) return 87;
            play(add(f, CardId::JUGGLING), f.player.Creature());
            PowerModel power = f.player.Creature().GetPower(PowerId::JUGGLING_POWER);
            if (!power || power.st().extra[0] != 2) return 88;
            CardModel third = add(f, CardId::STRIKE_IRONCLAD);
            CardList before = f.player.PlayerCombatState().Hand().Cards();
            play(third, f.enemy);
            int clones = 0;
            for (CardModel card : f.player.PlayerCombatState().Hand().Cards()) {
                if (before.contains(card)) continue;
                if (card.CardId_() != CardId::STRIKE_IRONCLAD || card.st().clone_of != third.Index()) return 89;
                ++clones;
            }
            if (clones != 1 || power.st().extra[0] != 3 ||
                combat.st().player_attack_cards_started_this_turn != 3 ||
                combat.st().player_attack_cards_this_turn != (startedOnly ? 1 : 3)) return 90;
            CardModel skill = add(f, CardId::DEFEND_IRONCLAD);
            play(skill, f.player.Creature());
            if (power.st().extra[0] != 3 || combat.st().player_attack_cards_started_this_turn != 3) return 91;
            PlayerChoiceContext ctx;
            if (startedOnly)
                PowerCmd::Apply(PowerId::AMBERGRIS_POWER, ctx, f.player.Creature(), Dec(1),
                                f.player.Creature(), CardModel{});
            CombatManager(&f.sim).EndPlayerTurn();
            if (combat.st().player_attack_cards_started_this_turn != 0 || power.st().extra[0] != 0 ||
                combat.st().player_taking_extra_turn != startedOnly) return 92;
        }
    }
    {
        Fixture f;
        CardModel beatDown = add(f, CardId::BEAT_DOWN);
        CardCmd::Upgrade(beatDown);
        for (int i = 0; i < 4; ++i) {
            CardModel strike = CombatState(&f.sim).CreateCard(CardId::STRIKE_IRONCLAD, f.player);
            CardPileCmd::Add(strike, PileType::Discard);
        }
        CardModel unplayable = CombatState(&f.sim).CreateCard(CardId::STRIKE_IRONCLAD, f.player);
        CardCmd::ApplyKeyword(unplayable, {CardKeyword::Unplayable});
        CardPileCmd::Add(unplayable, PileType::Discard);
        int32_t hpBefore = 0;
        for (Creature enemy : CombatState(&f.sim).HittableEnemies()) hpBefore += enemy.CurrentHp();
        play(beatDown, f.enemy);
        int32_t hpAfter = 0;
        for (Creature enemy : CombatState(&f.sim).HittableEnemies()) hpAfter += enemy.CurrentHp();
        if (hpBefore - hpAfter != 24) return 69;
        if (CombatState(&f.sim).st().player_attack_cards_this_turn != 4) return 70;
        if (unplayable.Pile().Type() != PileType::Discard) return 71;
    }
    {
        Fixture f;
        CardModel stratagem = add(f, CardId::STRATAGEM);
        CardCmd::Upgrade(stratagem);
        if (stratagem.EnergyCost().GetWithModifiers(CostModifiers::All) != 0) return 72;
        play(stratagem, f.player.Creature());
        if (f.player.Creature().GetPowerAmount(PowerId::STRATAGEM_POWER) != 1) return 73;
        CardList before = f.player.PlayerCombatState().Hand().Cards();
        CardList candidates = f.player.PlayerCombatState().DrawPile().Cards();
        for (int i = 0; i < 3; ++i) {
            CardModel card = CombatState(&f.sim).CreateCard(i % 2 == 0 ? CardId::STRIKE_IRONCLAD : CardId::DEFEND_IRONCLAD,
                                                            f.player);
            CardPileCmd::Add(card, PileType::Discard);
            candidates.push_back(card);
        }
        const int32_t answers[] = {1, 0};
        f.sim.set_prefix(std::span<const int32_t>(answers));
        PlayerChoiceContext ctx;
        CardPileCmd::Shuffle(ctx, f.player);
        if (!f.sim.prefix_consumed()) return 74;
        CardModel selected;
        for (CardModel card : f.player.PlayerCombatState().Hand().Cards()) {
            if (!before.contains(card)) selected = card;
        }
        if (!selected || !candidates.contains(selected) || selected.Pile().Type() != PileType::Hand) return 75;
        if (f.player.PlayerCombatState().Hand().Count() != static_cast<int32_t>(before.size() + 1)) return 76;
    }
    {
        Fixture f;
        CardModel calamity = add(f, CardId::CALAMITY);
        CardCmd::Upgrade(calamity);
        if (calamity.EnergyCost().GetWithModifiers(CostModifiers::All) != 2) return 77;
        play(calamity, f.player.Creature());
        if (f.player.Creature().GetPowerAmount(PowerId::CALAMITY_POWER) != 1) return 78;
        CardModel strike = add(f, CardId::STRIKE_IRONCLAD);
        CardList before = f.player.PlayerCombatState().Hand().Cards();
        play(strike, f.enemy);
        CardModel generated;
        for (CardModel card : f.player.PlayerCombatState().Hand().Cards()) {
            if (!before.contains(card)) generated = card;
        }
        if (!generated || generated.Type() != CardType::Attack || strike.st().calamity_pending_amount != -1) return 79;
        int32_t handBeforeSkill = f.player.PlayerCombatState().Hand().Count();
        CardModel defend = add(f, CardId::DEFEND_IRONCLAD);
        play(defend, f.player.Creature());
        if (f.player.PlayerCombatState().Hand().Count() != handBeforeSkill) return 80;
    }
    {
        Fixture f;
        CardModel card = add(f, CardId::SEEKER_STRIKE);
        CardList draw = f.player.PlayerCombatState().DrawPile().Cards();
        int handBefore = f.player.PlayerCombatState().Hand().Count();
        const int32_t answers[] = {1, 0};
        f.sim.set_prefix(std::span<const int32_t>(answers));
        int hp = f.enemy.CurrentHp();
        play(card, f.enemy);
        if (hp - f.enemy.CurrentHp() != 9 || !f.sim.prefix_consumed()) return 39;
        if (f.player.PlayerCombatState().Hand().Count() != handBefore) return 40;
        CardModel selected;
        for (CardModel candidate : f.player.PlayerCombatState().Hand().Cards())
            if (draw.contains(candidate)) selected = candidate;
        if (!selected || selected == card) return 41;
    }
    {
        Fixture f;
        RelicCmd::Obtain(RelicId::CHEMICAL_X, f.player);
        f.player.PlayerCombatState().SetEnergy(2);
        CardModel volley = add(f, CardId::VOLLEY);
        CardModel whirlwind = add(f, CardId::WHIRLWIND);
        whirlwind.EnergyCost().SetCapturedXValue(2);
        if (whirlwind.ResolveEnergyXValue() != 4) return 42;
        int32_t hpBefore = 0;
        for (Creature enemy : CombatState(&f.sim).Enemies()) hpBefore += enemy.CurrentHp();
        play(volley);
        int32_t hpAfter = 0;
        for (Creature enemy : CombatState(&f.sim).Enemies()) hpAfter += enemy.CurrentHp();
        if (hpBefore - hpAfter != 40) return 43;
    }
    return 0;
}
