using System.Collections;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Text.Json;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.GameActions.Multiplayer;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Events;
using MegaCrit.Sts2.Core.Models.Powers;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Unlocks;
using MegaCrit.Sts2.Core.ValueProps;

internal static class Program {
    static void Main() { GameBootstrap.InstallResolver(GameBootstrap.DefaultGameData); Check(); }
    [MethodImpl(MethodImplOptions.NoInlining)]
    static void Check() {
        GameBootstrap.Init(testMode:false);
        NonInteractiveMode.AutoSlayerCheck = () => true;
        GameBootstrap.UseInMemorySaves(); GameBootstrap.LoadLocalization();
        Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all,1);
        RunState run = RunState.CreateForNewRun([player],ModelDb.ActsByIndex.Select(a=>a[0].ToMutable()).ToArray(),[],GameMode.Standard,0,"POWERCAPTURES");
        RunManager.Instance.SetUpNewSingleplayer(run,shouldSave:false);
        player.ResetCombatState(); var state = new CombatState(runState:run);state.AddPlayer(player);
        CardPlay Play(CardModel card) => new() { Card=card, Player=player, Target=player.Creature,
            ResultPile=PileType.Discard, Resources=new ResourceInfo {EnergySpent=0,EnergyValue=0,StarsSpent=0,StarValue=0},
            IsAutoPlay=false,PlayIndex=0,PlayCount=1 };
        IDictionary Map(PowerModel power,string name) {
            var data=typeof(PowerModel).GetField("_internalData",BindingFlags.Instance|BindingFlags.NonPublic)!.GetValue(power)!;
            return (IDictionary)data.GetType().GetField(name)!.GetValue(data)!;
        }
        var ctx = new ThrowingPlayerChoiceContext();
        var rows = new List<object>();
        var skill=state.CreateCard<MadScience>(player);skill.TinkerTimeType=CardType.Skill;skill.TinkerTimeRider=TinkerTime.RiderEffect.Energized;
        var sp=Play(skill);
        var storm=(StormPower)ModelDb.Power<StormPower>().ToMutable();storm.ApplyInternal(player.Creature,3,silent:true);
        var sub=(SubroutinePower)ModelDb.Power<SubroutinePower>().ToMutable();sub.ApplyInternal(player.Creature,5,silent:true);
        storm.BeforeCardPlayed(sp).GetAwaiter().GetResult();sub.BeforeCardPlayed(sp).GetAwaiter().GetResult();
        storm.AfterCardPlayed(ctx,sp).GetAwaiter().GetResult();sub.AfterCardPlayed(ctx,sp).GetAwaiter().GetResult();
        rows.Add(new {name="skill-preserved", type=(int)skill.TinkerTimeType,rider=(int)skill.TinkerTimeRider,
            storm_count=Map(storm,"amountsForPlayedCards").Count,subroutine_count=Map(sub,"amountsForPlayedCards").Count});
        var pc=state.CreateCard<MadScience>(player);pc.TinkerTimeType=CardType.Power;pc.TinkerTimeRider=TinkerTime.RiderEffect.Expertise;
        var pp=Play(pc);
        storm.BeforeCardPlayed(pp).GetAwaiter().GetResult();sub.BeforeCardPlayed(pp).GetAwaiter().GetResult();
        storm.SetAmount(7,silent:true);sub.SetAmount(9,silent:true);
        rows.Add(new {name="captured-before-amount", storm=(int)Map(storm,"amountsForPlayedCards")[pc]!,
            subroutine=(int)Map(sub,"amountsForPlayedCards")[pc]!,type=(int)pc.TinkerTimeType,rider=(int)pc.TinkerTimeRider});
        bool duplicate=false;try { storm.BeforeCardPlayed(pp).GetAwaiter().GetResult(); } catch(ArgumentException) { duplicate=true; }
        rows.Add(new {name="duplicate-rejected",duplicate});
        var rupture=(RupturePower)ModelDb.Power<RupturePower>().ToMutable();rupture.ApplyInternal(player.Creature,2,silent:true);
        var a=state.CreateCard<Hemokinesis>(player);var b=state.CreateCard<Breakthrough>(player);
        var ap=Play(a);var bp=Play(b);
        rupture.BeforeCardPlayed(ap).GetAwaiter().GetResult();rupture.BeforeCardPlayed(bp).GetAwaiter().GetResult();
        void Observe(string name) => rows.Add(new {name,a=(int)Map(rupture,"playedCards")[a]!,b=(int)Map(rupture,"playedCards")[b]!,
            strength=player.Creature.GetPower<StrengthPower>()?.Amount??0});
        var damage=new DamageResult(player.Creature,ValueProp.Move|ValueProp.Unpowered) {UnblockedDamage=2};
        rupture.AfterDamageReceived(ctx,player.Creature,damage,damage.Props,player.Creature,a).GetAwaiter().GetResult();Observe("first-buffered");
        rupture.SetAmount(3,silent:true);
        rupture.AfterDamageReceived(ctx,player.Creature,damage,damage.Props,player.Creature,a).GetAwaiter().GetResult();
        rupture.AfterDamageReceived(ctx,player.Creature,damage,damage.Props,player.Creature,b).GetAwaiter().GetResult();Observe("nested-distinct-and-current-amount");
        var blocked=new DamageResult(player.Creature,ValueProp.Move) {UnblockedDamage=0};
        rupture.AfterDamageReceived(ctx,player.Creature,blocked,blocked.Props,player.Creature,a).GetAwaiter().GetResult();Observe("blocked-ignored");
        state.CurrentSide=CombatSide.Enemy;
        rupture.AfterDamageReceived(ctx,player.Creature,damage,damage.Props,player.Creature,a).GetAwaiter().GetResult();Observe("enemy-side-ignored");
        state.CurrentSide=CombatSide.Player;
        rupture.AfterDamageReceived(ctx,player.Creature,damage,damage.Props,player.Creature,null).GetAwaiter().GetResult();Observe("null-source-immediate");
        rupture.AfterCardPlayed(ctx,ap).GetAwaiter().GetResult();
        rows.Add(new {name="outer-finish",a_present=Map(rupture,"playedCards").Contains(a),b=(int)Map(rupture,"playedCards")[b]!,
            strength=player.Creature.GetPower<StrengthPower>()?.Amount??0});
        rupture.AfterCardPlayed(ctx,bp).GetAwaiter().GetResult();rupture.AfterCardPlayed(ctx,bp).GetAwaiter().GetResult();
        rows.Add(new {name="inner-and-duplicate-finish",count=Map(rupture,"playedCards").Count,strength=player.Creature.GetPower<StrengthPower>()?.Amount??0});
        Console.WriteLine(JsonSerializer.Serialize(new {game_version="v0.111.0",test_mode=false,scope="native power dictionaries and delayed Rupture primitive",rows}));
        RunManager.Instance.CleanUp();
    }
}
