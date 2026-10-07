using System.Reflection;
using System.Runtime.CompilerServices;
using System.Text.Json;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Commands.Builders;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.GameActions.Multiplayer;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Powers;
using MegaCrit.Sts2.Core.Models.Monsters;
using MegaCrit.Sts2.Core.Models.Enchantments;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Unlocks;
using MegaCrit.Sts2.Core.ValueProps;

internal static class Program
{
    static void Main() { GameBootstrap.InstallResolver(GameBootstrap.DefaultGameData); Check(); }
    [MethodImpl(MethodImplOptions.NoInlining)]
    static void Check()
    {
        GameBootstrap.Init(testMode: false);
        NonInteractiveMode.AutoSlayerCheck = () => true;
        GameBootstrap.UseInMemorySaves();
        GameBootstrap.LoadLocalization();
        Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all, 1);
        RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(a => a[0].ToMutable()).ToArray(),
            [], GameMode.Standard, 0, "UNMOVABLEHISTORY");
        RunManager.Instance.SetUpNewSingleplayer(run, shouldSave: false);
        player.ResetCombatState();
        var state = new CombatState(runState: run);
        state.AddPlayer(player);
        var card = state.CreateCard<DefendIronclad>(player);
        CardPlay MakePlay() => new() { Card = card, Player = player, Target = player.Creature,
            ResultPile = PileType.Discard, Resources = new ResourceInfo { EnergySpent = 0, EnergyValue = 0, StarsSpent = 0, StarValue = 0 },
            IsAutoPlay = false, PlayIndex = 0, PlayCount = 1 };
        var a = MakePlay(); var b = MakePlay();
        var power = (UnmovablePower)ModelDb.Power<UnmovablePower>().ToMutable();
        power.ApplyInternal(player.Creature, 1, silent: true);
        var history = CombatManager.Instance.History;
        history.Clear();
        var rows = new List<object>();
        void Query(string name, CardPlay? play, ValueProp props = ValueProp.Move) => rows.Add(new {
            name, multiplier = power.ModifyBlockMultiplicative(player.Creature, 5m, props, card, play) });
        Query("empty-first", a);
        history.BlockGained(state, player.Creature, 5, ValueProp.Move, a);
        Query("same-play-one", a); Query("different-play-one", b); Query("null-play-one", null);
        history.BlockGained(state, player.Creature, 5, ValueProp.Move, a);
        Query("same-play-two", a);
        power.SetAmount(2, silent: true); Query("different-play-two-entries-limit-two", b);
        history.BlockGained(state, player.Creature, 5, ValueProp.Move, b);
        power.SetAmount(1, silent: true); Query("outer-after-inner", a); Query("non-move", b, ValueProp.Unpowered);
        state.CurrentSide = CombatSide.Enemy; Query("new-side", a);
        history.BlockGained(state, player.Creature, 5, ValueProp.Move, a);
        Query("same-play-new-side", a); Query("different-play-new-side", b);
        state.CurrentSide = CombatSide.Player; state.RoundNumber++;
        Query("new-round", b);
        history.BlockGained(state, player.Creature, 5, ValueProp.Move | ValueProp.Unpowered, a);
        Query("move-even-if-unpowered-counts", b);
        typeof(PlayerCombatState).GetProperty(nameof(PlayerCombatState.TurnNumber))!.SetValue(player.PlayerCombatState, 2);
        Query("extra-player-turn", b);
        history.Clear();
        history.BlockGained(state, player.Creature, 5, ValueProp.Unpowered, a);
        history.BlockGained(state, player.Creature, 5, ValueProp.Move, null);
        Query("non-move-and-null-entries-ignored", b);
        history.BlockGained(state, player.Creature, 5, ValueProp.Move, a);
        history.Clear();
        history.BlockGained(state, player.Creature, 5, ValueProp.Move, b);
        Query("clear-excludes-old-own-entries", a);
        Console.WriteLine(JsonSerializer.Serialize(new { game_version = "v0.111.0", test_mode = false,
            scope = "native UnmovablePower and CombatHistory primitive", rows }));

        var attackCard = state.CreateCard<StrikeIronclad>(player);
        var otherCard = state.CreateCard<Bash>(player);
        var osty = state.CreateCreature(ModelDb.Monster<Osty>().ToMutable(), CombatSide.Player, null);
        var gigant = (GigantificationPower)ModelDb.Power<GigantificationPower>().ToMutable();
        gigant.ApplyInternal(player.Creature, 2, silent: true);
        var outer = new AttackCommand(1m).FromOsty(osty, attackCard, null);
        var innerSame = new AttackCommand(1m).FromOsty(osty, attackCard, null);
        var innerOther = new AttackCommand(1m).FromCard(otherCard, null);
        var giantRows = new List<object>();
        void GiantQuery(string name, CardModel? source, ValueProp props = ValueProp.Move) => giantRows.Add(new {
            name, multiplier = gigant.ModifyDamageMultiplicative(null, 1m, props, osty, source, null), amount = gigant.Amount });
        GiantQuery("no-active-attack", attackCard);
        gigant.BeforeAttack(new AttackCommand(1m)).GetAwaiter().GetResult();
        GiantQuery("null-model-before-attack", attackCard);
        gigant.BeforeAttack(outer).GetAwaiter().GetResult();
        GiantQuery("pet-active-same-card", attackCard);
        GiantQuery("pet-active-other-card", otherCard);
        GiantQuery("unpowered", attackCard, ValueProp.Move | ValueProp.Unpowered);
        GiantQuery("null-card", null);
        gigant.BeforeAttack(innerSame).GetAwaiter().GetResult();
        gigant.AfterAttack(new ThrowingPlayerChoiceContext(), innerSame).GetAwaiter().GetResult();
        GiantQuery("nested-same-did-not-consume", attackCard);
        gigant.BeforeAttack(innerOther).GetAwaiter().GetResult();
        gigant.AfterAttack(new ThrowingPlayerChoiceContext(), innerOther).GetAwaiter().GetResult();
        GiantQuery("nested-other-did-not-consume", attackCard);
        gigant.AfterAttack(new ThrowingPlayerChoiceContext(), outer).GetAwaiter().GetResult();
        GiantQuery("outer-consumed-once", otherCard);
        gigant.AfterAttack(new ThrowingPlayerChoiceContext(), outer).GetAwaiter().GetResult();
        GiantQuery("duplicate-after-did-not-consume", otherCard);
        Console.WriteLine(JsonSerializer.Serialize(new { game_version = "v0.111.0", test_mode = false,
            scope = "native GigantificationPower command identity primitive", rows = giantRows }));
        var shiv = state.CreateCard<Shiv>(player);
        CardCmd.Enchant<Inky>(shiv, 1m);
        Console.WriteLine(JsonSerializer.Serialize(new { scope = "native Inky canonical vars",
            weak = shiv.Enchantment!.DynamicVars.Weak.BaseValue }));
        RunManager.Instance.CleanUp();
    }
}
