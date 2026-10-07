using System.Runtime.CompilerServices;
using System.Text.Json;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Powers;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Unlocks;

internal static class Program
{
    private static void Main()
    {
        GameBootstrap.InstallResolver(GameBootstrap.DefaultGameData);
        Check();
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static void Check()
    {
        GameBootstrap.Init(testMode: false);
        Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all, 1);
        RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(acts => acts[0].ToMutable()).ToArray(),
            [], GameMode.Standard, 0, "EXTRATURNHISTORY");
        CombatState combat = new(runState: run);
        combat.AddPlayer(player);
        player.PlayerCombatState = new(player);
        CombatManager manager = CombatManager.Instance;
        manager._turnState = new(combat) { IsInProgress = true, IsStarting = false };
        PowerModel power = ModelDb.Power<NostalgiaPower>().ToMutable();
        power.ApplyInternal(player.Creature, 1, silent: true);
        CardModel attack = combat.CreateCard<StrikeIronclad>(player);
        CardModel skill = combat.CreateCard<DefendIronclad>(player);
        var rows = new List<object>();
        ResourceInfo resources = new() { EnergySpent = 0, EnergyValue = 1, StarsSpent = 0, StarValue = 0 };
        void Started(CardModel card) => manager.History.CardPlayStarted(combat, new()
        {
            Card = card, Player = player, Target = null, ResultPile = PileType.Discard,
            Resources = resources, IsAutoPlay = true, PlayIndex = 0, PlayCount = 1,
        });
        void Observe(string phase, int expectedStarted, PileType expectedPile)
        {
            int count = manager.History.CardPlaysStarted.Count(e => e.HappenedThisTurn(combat) &&
                e.CardPlay.Player == player && e.CardPlay.Card.Type is CardType.Attack or CardType.Skill);
            CardLocation result = power.ModifyCardPlayResultLocation(attack, true, resources,
                new(player, PileType.Discard, CardPilePosition.Bottom));
            Require(count == expectedStarted && result.pileType == expectedPile, "native history/result mismatch:" + phase);
            if (result.pileType == PileType.Draw) Require(result.position == CardPilePosition.Top, "native return position lost");
            rows.Add(new { phase, side = combat.CurrentSide.ToString(), round = combat.RoundNumber,
                turn = player.PlayerCombatState.TurnNumber, started = count,
                nostalgia_pile = result.pileType.ToString(), nostalgia_position = result.position.ToString() });
        }
        Started(attack);
        Started(skill);
        Observe("ordinary_play", 2, PileType.Discard);
        int round = combat.RoundNumber, turn = player.PlayerCombatState.TurnNumber;
        manager._turnState.PlayersTakingExtraTurn.Add(player);
        manager.SwitchSides(manager._turnState);
        Require(combat.CurrentSide == CombatSide.Player && combat.RoundNumber == round &&
            player.PlayerCombatState.TurnNumber == turn + 1, "native extra-turn boundary differs");
        Observe("extra_turn_start", 0, PileType.Draw);
        Started(attack);
        Observe("extra_turn_first_attack_started", 1, PileType.Discard);
        Started(skill);
        Observe("extra_turn_second_started", 2, PileType.Discard);
        manager._turnState.PlayersTakingExtraTurn.Clear();
        manager.SwitchSides(manager._turnState);
        Require(combat.CurrentSide == CombatSide.Enemy, "native enemy boundary differs");
        Observe("enemy_side_start", 0, PileType.Draw);
        manager.SwitchSides(manager._turnState);
        Require(combat.CurrentSide == CombatSide.Player && combat.RoundNumber == round + 1 &&
            player.PlayerCombatState.TurnNumber == turn + 2, "native normal-turn boundary differs");
        Observe("normal_turn_start", 0, PileType.Draw);
        // Juggling's application reads Started Attack history, not Finished
        // Attack history. Observe the shipped private Data after applying and
        // reapplying it; this remains a primitive check, not a full turn run.
        Started(attack);
        Started(attack);
        Started(skill);
        int jugglingStarted = manager.History.CardPlaysStarted.Count(e => e.HappenedThisTurn(combat) &&
            e.CardPlay.Player == player && e.CardPlay.Card.Type == CardType.Attack);
        int jugglingFinished = manager.History.CardPlaysFinished.Count(e => e.HappenedThisTurn(combat) &&
            e.CardPlay.Player == player && e.CardPlay.Card.Type == CardType.Attack);
        PowerModel juggling = ModelDb.Power<JugglingPower>().ToMutable();
        juggling.ApplyInternal(player.Creature, 1, silent: true);
        juggling.AfterApplied(player.Creature, null).GetAwaiter().GetResult();
        int PrivateCount(PowerModel model) => (int)model._internalData!.GetType()
            .GetField("attacksPlayedThisTurn")!.GetValue(model._internalData)!;
        Require(jugglingStarted == 2 && jugglingFinished == 0 && PrivateCount(juggling) == 2,
            "native Juggling application did not initialize from Started Attack history");
        juggling.RemoveInternal();
        PowerModel reapplied = ModelDb.Power<JugglingPower>().ToMutable();
        reapplied.ApplyInternal(player.Creature, 1, silent: true);
        reapplied.AfterApplied(player.Creature, null).GetAwaiter().GetResult();
        Require(PrivateCount(reapplied) == 2, "native Juggling reapplication forgot earlier started attacks");
        var jugglingRow = new { started_attacks = jugglingStarted, finished_attacks = jugglingFinished,
            applied_private_count = PrivateCount(juggling), reapplied_private_count = PrivateCount(reapplied) };
        manager._turnState.Cancel();
        Console.WriteLine(JsonSerializer.Serialize(new
        {
            game_version = "v0.111.0", test_mode = false,
            verified = "shipped SwitchSides, HappenedThisTurn, Nostalgia result-location and Juggling AfterApplied primitives",
            full_turn_and_damage_effects_executed = false, rows, juggling = jugglingRow,
        }));
    }

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }
}
