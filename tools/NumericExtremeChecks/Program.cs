using System.Runtime.CompilerServices;
using System.Collections;
using System.Reflection;
using System.Text.Json;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.GameActions.Multiplayer;
using MegaCrit.Sts2.Core.Localization.DynamicVars;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Afflictions;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Enchantments;
using MegaCrit.Sts2.Core.Models.Potions;
using MegaCrit.Sts2.Core.Models.Powers;
using MegaCrit.Sts2.Core.Models.Monsters;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Unlocks;

internal static class Program {
    static TextWriter Output = Console.Out;
    static void Main() { Output=Console.Out; Console.SetOut(Console.Error); GameBootstrap.InstallResolver(GameBootstrap.DefaultGameData); Check(); }
    [MethodImpl(MethodImplOptions.NoInlining)]
    static void Check() {
        GameBootstrap.Init(testMode:false);
        NonInteractiveMode.AutoSlayerCheck = () => true;
        GameBootstrap.UseInMemorySaves(); GameBootstrap.LoadLocalization();
        Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all,1);
        RunState run = RunState.CreateForNewRun([player],ModelDb.ActsByIndex.Select(a=>a[0].ToMutable()).ToArray(),[],GameMode.Standard,0,"NUMERICEXTREMES");
        RunManager.Instance.SetUpNewSingleplayer(run,shouldSave:false);
        player.ResetCombatState(); var state = new CombatState(runState:run); state.AddPlayer(player);
        var power = (StrengthPower)ModelDb.Power<StrengthPower>().ToMutable(); power.ApplyInternal(player.Creature,1,silent:true);
        int[] values = [int.MinValue,int.MinValue+1,-32769,-32768,-1,0,1,32767,32768,65535,65536,999999999,int.MaxValue-1,int.MaxValue];
        var rows = new List<object>();
        CardModel Card() => ModelDb.Card<StrikeIronclad>().ToMutable();
        foreach(int v in values) foreach(int count in new[]{1,2,3}) foreach(bool overkill in new[]{false,true}) {
            var results=Enumerable.Range(0,count).Select(_=>new DamageResult(player.Creature,default) {UnblockedDamage=v,OverkillDamage=v}).ToArray();
            bool threw=false; int total=0;
            try {total=results.Sum(r=>overkill?r.TotalDamage+r.OverkillDamage:r.TotalDamage);} catch(OverflowException) {threw=true;}
            rows.Add(new {group="damage_sum",v,count,overkill,threw,total});
        }
        {
            // Read the original private lists, beyond both former capacities;
            // cloning and expiry must retain the original append order.
            var card=Card();
            for(int i=0;i<64;i++) {card.EnergyCost.SetThisTurn(i);card.EnergyCost.AddThisCombat(1);card.SetStarCostThisTurn(1);}
            var copy=(CardModel)card.ClonePreservingMutability();
            copy.EnergyCost.EndOfTurnCleanup(); copy.EndOfTurnCleanup();
            int Count(object target,string field)=>(int)target.GetType().GetField(field,BindingFlags.NonPublic|BindingFlags.Instance)!.GetValue(target)!.GetType().GetProperty("Count")!.GetValue(target.GetType().GetField(field,BindingFlags.NonPublic|BindingFlags.Instance)!.GetValue(target))!;
            rows.Add(new {group="modifier_lists",energy_count=Count(card.EnergyCost,"_localModifiers"),star_count=((IList)typeof(CardModel).GetField("_temporaryStarCosts",BindingFlags.NonPublic|BindingFlags.Instance)!.GetValue(card)!).Count,
                cost=card.EnergyCost.GetWithModifiers(CostModifiers.Local),copy_count=Count(copy.EnergyCost,"_localModifiers"),copy_cost=copy.EnergyCost.GetWithModifiers(CostModifiers.Local),copy_star=copy.CurrentStarCost});
        }
        {
            var opponents=Enumerable.Range(0,3).Select(_=>new Creature(ModelDb.Monster<DampCultist>().ToMutable(),CombatSide.Enemy,null) {CombatState=state}).ToArray();
            var accelerants=new AccelerantPower[3];
            for(int i=0;i<3;i++) {opponents[i].SetCurrentHpInternal(1);state.AddCreature(opponents[i]);accelerants[i]=(AccelerantPower)ModelDb.Power<AccelerantPower>().ToMutable();accelerants[i].ApplyInternal(opponents[i],1,silent:true);}
            var poison=(PoisonPower)ModelDb.Power<PoisonPower>().ToMutable();poison.ApplyInternal(player.Creature,999999999,silent:true);
            var getter=typeof(PoisonPower).GetProperty("TriggerCount",BindingFlags.NonPublic|BindingFlags.Instance)!;
            foreach(int v in values) foreach(int count in new[]{1,2,3}) {
                for(int i=0;i<3;i++) {opponents[i].SetCurrentHpInternal(i<count?1:0);accelerants[i].SetAmount(v,silent:true);}
                bool threw=false;int total=0;
                try {total=(int)getter.GetValue(poison)!;} catch(TargetInvocationException e) when(e.InnerException is OverflowException) {threw=true;}
                rows.Add(new {group="poison_sum",v,count,threw,total});
            }
            foreach(var opponent in opponents)state.RemoveCreature(opponent);
            poison.RemoveInternal();
        }
        foreach(int v in values) {
            var x=ModelDb.Card<Whirlwind>().ToMutable(); x.EnergyCost.CapturedXValue=v;
            var card=Card(); card.LastStarsSpent=v; card.BaseReplayCount=v; card.SetStarCostThisCombat(v);
            var enchant=ModelDb.Enchantment<Goopy>().ToMutable(); enchant.Amount=v;
            var affliction=ModelDb.Affliction<Bound>().ToMutable(); affliction.Amount=v;
            power.SetAmount(v,silent:true);
            var upgraded=Card(); upgraded.EnergyCost.UpgradeBy(v);
            rows.Add(new {group="fields",v,x=x.EnergyCost.CapturedXValue,stars=card.LastStarsSpent,
                replay=card.BaseReplayCount,star_cost=card.CurrentStarCost,enchantment=enchant.Amount,
                affliction=affliction.Amount,power=power.Amount,upgrade_cost=upgraded.EnergyCost.GetWithModifiers(CostModifiers.Local)});
        }
        foreach(int a in values) foreach(int b in values) foreach(bool reduce in new[]{false,true}) {
            var card=Card(); card.EnergyCost.SetThisCombat(a); card.EnergyCost.AddThisCombat(b,reduce);
            int cost=card.EnergyCost.GetWithModifiers(CostModifiers.Local);
            rows.Add(new {group="cost",a,b,reduce,cost});
            var mixed=Card(); mixed.EnergyCost.AddThisTurn(a); mixed.EnergyCost.AddThisCombat(b,reduce);
            int before=mixed.EnergyCost.GetWithModifiers(CostModifiers.Local);
            mixed.EnergyCost.EndOfTurnCleanup();
            rows.Add(new {group="expiry",a,b,reduce,before,after=mixed.EnergyCost.GetWithModifiers(CostModifiers.Local)});
            var relative=Card(); relative.EnergyCost.AddThisCombat(a); relative.EnergyCost.AddThisCombat(b,reduce);
            rows.Add(new {group="relative",a,b,reduce,cost=relative.EnergyCost.GetWithModifiers(CostModifiers.Local)});
        }
        foreach(bool stars in new[]{false,true}) foreach(int count in new[]{1,2,3}) {
            CombatManager.Instance.History.Clear();
            CardModel card=stars?state.CreateCard<Radiate>(player):state.CreateCard<HelixDrill>(player);
            player.PlayerCombatState!.DrawPile.AddInternal(card);
            for(int i=0;i<count;i++) {
                if(stars) CombatManager.Instance.History.StarsModified(state,999999999,player);
                else CombatManager.Instance.History.EnergySpent(state,999999999,player);
            }
            // Invoke the exact DLL multiplier; this component fixture does not
            // start CombatManager's presentation-dependent combat lifecycle.
            var variable=(CalculatedVar)card.DynamicVars["CalculatedHits"];
            var callback=(Func<CardModel,Creature?,decimal>)typeof(CalculatedVar).GetField("_multiplierCalc",BindingFlags.NonPublic|BindingFlags.Instance)!.GetValue(variable)!;
            bool threw=false; int result=0;
            try {result=(int)callback(card,null);} catch(OverflowException) {threw=true;}
            rows.Add(new {group="history_sum",stars,count,threw,result});
            card.RemoveFromCurrentPile();
        }
        {
            var original=state.CreateCard<StrikeIronclad>(player); original.UpgradeInternal();
            player.PlayerCombatState!.DrawPile.AddInternal(original);
            var dampen=(DampenPower)ModelDb.Power<DampenPower>().ToMutable(); dampen.ApplyInternal(player.Creature,1,silent:true);
            dampen.AfterApplied(null,null).GetAwaiter().GetResult();
            var clone=state.CloneCard(original); original.RemoveFromCurrentPile();
            var casters=Enumerable.Range(0,40).Select(_=>new Creature(ModelDb.Monster<DampCultist>().ToMutable(),CombatSide.Enemy,null)).ToArray();
            foreach(var caster in casters) {dampen.AddCaster(caster); dampen.AddCaster(caster);}
            for(int i=0;i<39;i++) dampen.AfterDeath(new ThrowingPlayerChoiceContext(),casters[i],false,0).GetAwaiter().GetResult();
            dampen.AfterRemoved(player.Creature).GetAwaiter().GetResult();
            var data=typeof(PowerModel).GetField("_internalData",BindingFlags.NonPublic|BindingFlags.Instance)!.GetValue(dampen)!;
            int count=(int)data.GetType().GetField("casters")!.GetValue(data)!.GetType().GetProperty("Count")!.GetValue(data.GetType().GetField("casters")!.GetValue(data))!;
            rows.Add(new {group="dampen",casters=count,original=original.CurrentUpgradeLevel,clone=clone.CurrentUpgradeLevel,
                remembered=((IDictionary)data.GetType().GetField("downgradedCardsToOldUpgradeLevels")!.GetValue(data)!).Count});
        }
        foreach(var item in new[]{(true,37,false),(true,-2,false),(false,-2,false),(false,2,false),
            (true,int.MaxValue,false),(false,int.MinValue,false),(false,2,true),(false,4,false)}) {
            var p=Player.CreateForNewRun<Ironclad>(UnlockState.all,1);
            if(item.Item3) for(int i=0;i<p.MaxPotionCount;i++) p.AddPotionInternal(ModelDb.Potion<BlockPotion>().ToMutable(),i,silent:true);
            bool threw=false;
            try { if(item.Item1) p.AddToMaxPotionCount(item.Item2); else p.SubtractFromMaxPotionCount(item.Item2); }
            catch(ArgumentOutOfRangeException) { threw=true; }
            rows.Add(new {group="potions",add=item.Item1,amount=item.Item2,full=item.Item3,threw,
                count=p.MaxPotionCount,occupied=p.PotionSlots.Select(v=>v!=null).ToArray()});
        }
        Output.WriteLine(JsonSerializer.Serialize(new {game_version="v0.111.0",test_mode=false,rows}));
        RunManager.Instance.CleanUp();
    }
}
