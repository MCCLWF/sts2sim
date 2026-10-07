using System.Reflection;
using System.Runtime.CompilerServices;
using System.Text.Json;
using CombatSolver;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Multiplayer;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.GameActions.Multiplayer;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Powers;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.TestSupport;
using MegaCrit.Sts2.Core.Unlocks;

internal static class Program
{
    private sealed class NoChoice : PlayerChoiceContext
    {
        public override ulong? OwnerId => null;
        public override Task SignalPlayerChoiceBegun(Player player, PlayerChoiceOptions options)
            => throw new InvalidOperationException("unexpected choice");
        public override Task SignalPlayerChoiceEnded() => Task.CompletedTask;
    }

    private static void Main()
    {
        GameBootstrap.InstallResolver(GameBootstrap.DefaultGameData);
        Check();
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static void Check()
    {
        GameBootstrap.Init(testMode: false);
        if (TestMode.IsOn) throw new InvalidOperationException("VoidForm oracle requires TestMode=false");
        Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all, 1);
        RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(acts => acts[0].ToMutable()).ToArray(),
            [], GameMode.Standard, 0, "VOIDHOOK");
        CombatState combat = new(runState: run);
        combat.AddPlayer(player);
        player.PlayerCombatState = new(player);
        MonsterModel monster = ModelDb.GetById<MonsterModel>(new("MONSTER", "CALCIFIED_CULTIST")).ToMutable();
        Creature enemy = combat.CreateCreature(monster, CombatSide.Enemy, null);
        combat.AddCreature(enemy);
        CombatManager.Instance._turnState = new(combat) { IsInProgress = true, IsStarting = false };
        NoChoice context = new();
        CardModel card = combat.CreateCard<StrikeIronclad>(player);
        player.PlayerCombatState.Hand.AddInternal(card, silent: true);
        VoidFormPower power = (VoidFormPower)ModelDb.Power<VoidFormPower>().ToMutable();
        power.ApplyInternal(player.Creature, 2m, silent: true);
        VoidFormPower enemyPower = (VoidFormPower)ModelDb.Power<VoidFormPower>().ToMutable();
        enemyPower.ApplyInternal(enemy, 2m, silent: true);
        StrengthPower unrelated = (StrengthPower)ModelDb.Power<StrengthPower>().ToMutable();
        unrelated.ApplyInternal(player.Creature, 1m, silent: true);
        FieldInfo counter = power._internalData!.GetType().GetField("cardsPlayedThisTurn")!;
        int Count() => (int)counter.GetValue(power._internalData)!;
        List<object> rows = [];
        void Record(string name, VoidFormPower? queried = null)
        {
            queried ??= power;
            bool energy = queried.TryModifyEnergyCostInCombatLate(card, 7.12500m, out decimal energyCost);
            bool stars = queried.TryModifyStarCost(card, 4.000m, out decimal starCost);
            rows.Add(new { name, counter = Count(), energy_modified = energy, star_modified = stars,
                energy_bits = decimal.GetBits(energyCost).Select(value => unchecked((uint)value)).ToArray(),
                star_bits = decimal.GetBits(starCost).Select(value => unchecked((uint)value)).ToArray() });
        }
        void Play(bool automatic, int index, int count)
        {
            power.AfterCardPlayed(context, new CardPlay { Card = card, Player = player, Target = null,
                ResultPile = PileType.Discard, Resources = new() { EnergySpent = 0, EnergyValue = 0, StarsSpent = 0, StarValue = 0 },
                IsAutoPlay = automatic, PlayIndex = index, PlayCount = count })
                .GetAwaiter().GetResult();
        }
        Record("initial");
        power.BeforeApplied(player.Creature, 2m, player.Creature, card).GetAwaiter().GetResult();
        Record("before_applied");
        power.BeforeSideTurnStart(context, CombatSide.Enemy, [enemy], combat).GetAwaiter().GetResult();
        Record("other_participant");
        power.BeforeSideTurnStart(context, CombatSide.Player, [player.Creature], combat).GetAwaiter().GetResult();
        Record("owner_participant");
        foreach (PileType pile in new[] { PileType.Play, PileType.Draw, PileType.Discard, PileType.Exhaust, PileType.Hand })
        {
            card.Pile!.RemoveInternal(card, silent: true);
            CardPile.Get(pile, player)!.AddInternal(card, silent: true);
            Record("pile_" + pile.ToString().ToLowerInvariant());
        }
        Record("foreign_owner", enemyPower);
        Play(true, 0, 1); Record("automatic");
        Play(false, 0, 2); Record("manual_nonlast");
        Play(false, 1, 2); Record("manual_last");
        Play(false, 0, 1); Record("manual_threshold");
        power.BeforePowerAmountChanged(unrelated, 1m, player.Creature, player.Creature, card).GetAwaiter().GetResult();
        Record("unrelated_amount");
        power.BeforeSideTurnStart(context, CombatSide.Player, [player.Creature], combat).GetAwaiter().GetResult();
        power.BeforePowerAmountChanged(power, 1m, player.Creature, player.Creature, card).GetAwaiter().GetResult();
        Record("own_amount");
        // The live mapper must freeze the real private counter and ignore only
        // the exact visual reference. Native mutation cannot change that copy.
        EnvLiveCreatures.Warmup(combat);
        EnvLiveCreatures frozen = EnvLiveCreatures.Capture(combat, player, combat.Creatures.ToArray());
        int id = EnvModelCatalog.Content.GetProperty("powers").GetProperty("VOID_FORM_POWER").GetProperty("id").GetInt32();
        int CapturedCount(EnvLiveCreatures capture) => capture.Component.GetProperty("powers").GetProperty("slots").EnumerateArray()
            .Select(slot => slot.GetProperty("value")).First(value => value.GetProperty("id").GetInt32() == id)
            .GetProperty("extra")[0].GetInt32();
        int mapped = CapturedCount(frozen);
        counter.SetValue(power._internalData, 27);
        if (mapped != 999999999 || CapturedCount(frozen) != mapped
            || CapturedCount(EnvLiveCreatures.Capture(combat, player, combat.Creatures.ToArray())) != 27)
            throw new InvalidOperationException("live VoidForm private counter ownership lost");
        object internalData = power._internalData!;
        power._internalData = null;
        try
        {
            EnvLiveCreatures.Capture(combat, player, combat.Creatures.ToArray());
            throw new InvalidOperationException("missing VoidForm private counter accepted");
        }
        catch (InvalidOperationException error) when (error.Message == "live_power_internal_data_missing:VOID_FORM_POWER") { }
        finally { power._internalData = internalData; }
        Console.WriteLine(JsonSerializer.Serialize(new { rows, mapped_counter = mapped, test_mode = TestMode.IsOn }));
        CombatManager.Instance._turnState.Cancel();
    }
}
