using System.Reflection;
using System.Text.Json;
using System.Threading.Tasks;
using HarmonyLib;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Entities.Multiplayer;
using MegaCrit.Sts2.Core.GameActions.Multiplayer;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Powers;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Unlocks;
using MegaCrit.Sts2.Core.ValueProps;

// Executes shipped gameplay hooks with TestMode=false. It does not create a
// complete live-root search fixture or certify turn/room transitions.
internal static class HpDisplayChecks
{
    private static int _autoplayCalls;
    private static bool SkipAutoPlay(ref Task __result)
    {
        _autoplayCalls++;
        __result = Task.CompletedTask;
        return false;
    }
    private sealed class NoChoice : PlayerChoiceContext
    {
        public override ulong? OwnerId => null;
        public override Task SignalPlayerChoiceBegun(Player player, PlayerChoiceOptions options)
            => throw new InvalidOperationException("unexpected choice");
        public override Task SignalPlayerChoiceEnded() => Task.CompletedTask;
    }

    internal static void Run()
    {
        Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all, 1);
        RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(acts => acts[0].ToMutable()).ToArray(),
            [], GameMode.Standard, 0, "HPDISPLAY");
        CombatState combat = new(runState: run);
        combat.AddPlayer(player);
        player.PlayerCombatState = new(player) { Phase = PlayerTurnPhase.Play, Energy = 3 };
        MonsterModel monster = ModelDb.GetById<MonsterModel>(new("MONSTER", "CALCIFIED_CULTIST")).ToMutable();
        Creature enemy = combat.CreateCreature(monster, CombatSide.Enemy, null);
        combat.AddCreature(enemy);
        enemy.SetMaxHpInternal(1000m);
        enemy.SetCurrentHpInternal(1000m);
        CombatManager manager = CombatManager.Instance;
        manager._turnState = new(combat) { IsInProgress = true, IsStarting = false };
        NoChoice context = new();
        HardenedShellPower shell = (HardenedShellPower)ModelDb.Power<HardenedShellPower>().ToMutable();
        shell.ApplyInternal(enemy, 20m, silent: true);
        FieldInfo counterField = shell._internalData!.GetType().GetField("damageReceivedThisTurn")!;
        decimal Counter() => (decimal)counterField.GetValue(shell._internalData)!;
        uint[] Bits(decimal value) => decimal.GetBits(value).Select(v => unchecked((uint)v)).ToArray();
        List<object> shellRows = [];
        foreach (decimal counter in new[] { 7.000m, 7.125m, new decimal(0, 0, 0, true, 3), 21m })
        {
            counterField.SetValue(shell._internalData, counter);
            decimal result = shell.ModifyHpLostBeforeOstyLate(enemy, 13m, ValueProp.Unpowered, null, null);
            shellRows.Add(new { counter = Bits(Counter()), result = Bits(result) });
        }
        counterField.SetValue(shell._internalData, 19.000m);
        shell.AfterDamageReceived(context, enemy, new(enemy, ValueProp.Unpowered) { UnblockedDamage = 1 },
            ValueProp.Unpowered, null, null).GetAwaiter().GetResult();
        var afterDamage = new { counter = Bits(Counter()), hp_display = (int)enemy.HpDisplay };
        shell.BeforeSideTurnStart(context, CombatSide.Player, [], combat).GetAwaiter().GetResult();
        var afterTurn = new { counter = Bits(Counter()), hp_display = (int)enemy.HpDisplay };
        Require(Counter() == 0m && enemy.HpDisplay == HpDisplay.Normal && afterDamage.hp_display == (int)HpDisplay.InfiniteWithNumbers,
            "HardenedShell decimal threshold or HpDisplay reset differs from native");
        shell.RemoveInternal();

        HellraiserPower hellraiser = (HellraiserPower)ModelDb.Power<HellraiserPower>().ToMutable();
        hellraiser.ApplyInternal(player.Creature, 1m, silent: true);
        Harmony harmony = new("sts2sim.hpdisplay-checks");
        harmony.Patch(AccessTools.Method(typeof(CardCmd), nameof(CardCmd.AutoPlay))!,
            prefix: new HarmonyMethod(typeof(HpDisplayChecks), nameof(SkipAutoPlay)));
        FieldInfo infiniteField = hellraiser._internalData!.GetType().GetField("infiniteAutoPlaysThisTurn")!;
        FieldInfo messageField = hellraiser._internalData!.GetType().GetField("showedCapReachedMessage")!;
        List<object> hellraiserRows = [];
        foreach (HpDisplay display in new[] { HpDisplay.Normal, HpDisplay.InfiniteWithNumbers, HpDisplay.InfiniteWithoutNumbers })
        {
            enemy.HpDisplay = display;
            enemy.SetCurrentHpInternal(1000m);
            infiniteField.SetValue(hellraiser._internalData, 9);
            // Only suppress ThinkCmd's VFX; the actual cap test still runs.
            messageField.SetValue(hellraiser._internalData, true);
            CardModel strike = combat.CreateCard<StrikeIronclad>(player);
            player.PlayerCombatState.Hand.AddInternal(strike, silent: true);
            hellraiser.AfterCardDrawnEarly(context, strike, true).GetAwaiter().GetResult();
            hellraiserRows.Add(new { hp_display = (int)display, counter = (int)infiniteField.GetValue(hellraiser._internalData)!,
                autoplay_calls = _autoplayCalls });
        }
        Require(((int)infiniteField.GetValue(hellraiser._internalData)!) == 10 && _autoplayCalls == 1,
            "Hellraiser finite/infinite cap or counter reset differs from native");
        harmony.UnpatchAll("sts2sim.hpdisplay-checks");
        manager._turnState.Cancel();
        Console.WriteLine(JsonSerializer.Serialize(new { hp_display_primitives = new
        {
            shell = shellRows, after_damage = afterDamage, after_turn = afterTurn, hellraiser = hellraiserRows,
        }, full_root = false }));
    }

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }
}
