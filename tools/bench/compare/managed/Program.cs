using System.Diagnostics;
using System.Runtime;
using System.Text.Json;
using Sts2Sim.Core.Content;
using Sts2Sim.Core.Content.Acts;
using Sts2Sim.Core.Combat;
using Sts2Sim.Core.Combat.StateDescription;
using Sts2Sim.Core.Entities.Cards;
using Sts2Sim.Core.Entities.Players;
using Sts2Sim.Core.Entities.Rngs;
using Sts2Sim.Core.Models;
using Sts2Sim.Core.Models.Characters;
using Sts2Sim.Core.Map;
using Sts2Sim.Core.Rooms;
using Sts2Sim.Core.Runs;
using Sts2Sim.Core.Rl;

ModelDb.Init(ContentRegistry.AllTypes);
void Emit(object o) => Console.WriteLine(JsonSerializer.Serialize(o));
var cases=File.ReadAllLines(args[0]).Where(l=>!string.IsNullOrWhiteSpace(l)).Select(l=>l.Split('\t')).ToArray();
Emit(new {type="metadata",language="csharp",runtime=Environment.Version.ToString(),server_gc=GCSettings.IsServerGC,case_count=cases.Length});
(RunState,CombatState) Setup(string[] c) {
    var run=new RunState(c[1],ascensionLevel:int.Parse(c[3]));
    CharacterModel character=c[2] switch {
        "IRONCLAD"=>ModelDb.Character<Ironclad>(),"SILENT"=>ModelDb.Character<Silent>(),
        "REGENT"=>ModelDb.Character<Regent>(),"DEFECT"=>ModelDb.Character<Defect>(),
        "NECROBINDER"=>ModelDb.Character<Necrobinder>(),_=>throw new Exception("unknown character")};
    var player=Player.CreateForNewRun(character,run);run.AddPlayer(player);
    _=run.CurrentAncientEventType;
    int act=int.Parse(c[5]);for(int i=0;i<act;++i)run.AdvanceToNextAct();
    run.AddVisitedMapCoord(new MapCoord(0,0));
    var encounter=c[4] switch {
        "CULTISTS_NORMAL"=>UnderdocksEncounters.BatchA.Single(e=>e.Name=="CultistsNormal"),
        "EXOSKELETONS_NORMAL"=>HiveEncounters.BatchA.Single(e=>e.Name=="ExoskeletonsNormal"),
        "MECHA_KNIGHT_ELITE"=>GloryEncounters.BatchA.Single(e=>e.Name=="MechaKnightElite"),
        _=>throw new Exception("unknown encounter")};
    var room=new CombatRoom(()=>encounter.CreateMonsters(),RoomType.Monster,encounter.Name);
    run.PushRoom(room);room.Enter(run).GetAwaiter().GetResult();var s=room.Engine.State;
    for(int i=0;i<int.Parse(c[6]);++i)s.Engine!.EndPlayerTurnAsync().GetAwaiter().GetResult();
    if(!s.Engine!.IsInProgress)throw new Exception("root is not active combat");return(run,s);
}
ulong Digest(CombatState s){var b=new CombatStateDescriptionBuilder();CombatStateDescription.AppendExactState(ref b,s);return b.Build().First;}
object Project(CombatState s) {
    var p=s.Players[0];var pcs=p.PlayerCombatState!;
    string[] Pile(CardPile pile)=>pile.Cards.Select(c=>c.Id.Entry+(c.IsUpgraded?"+1":"")).ToArray();
    object[] Powers(Sts2Sim.Core.Entities.Creatures.Creature c)=>c.Powers.Select(p=>(object)new object[]{p.Id.Entry,p.Amount}).ToArray();
    return new {round=s.RoundNumber,turn=pcs.TurnNumber,
      player=new {hp=p.Creature.CurrentHp,max_hp=p.Creature.MaxHp,block=p.Creature.Block,energy=pcs.Energy,stars=pcs.Stars,
        powers=Powers(p.Creature),hand=Pile(pcs.Hand),draw=Pile(pcs.DrawPile),discard=Pile(pcs.DiscardPile),exhaust=Pile(pcs.ExhaustPile),play=Pile(pcs.PlayPile),
        orbs=pcs.OrbQueue.Orbs.Select(o=>o.Id.Entry).ToArray(),orb_capacity=pcs.OrbQueue.Capacity,
        pets=pcs.Pets.Select(e=>new {id=e.Monster!.Id.Entry,combat_id=e.CombatId,hp=e.CurrentHp,max_hp=e.MaxHp,block=e.Block,powers=Powers(e),next_move=e.Monster.NextMove?.Id}).ToArray()},
      enemies=s.Enemies.Select(e=>new {id=e.Monster!.Id.Entry,combat_id=e.CombatId,hp=e.CurrentHp,max_hp=e.MaxHp,block=e.Block,
        powers=Powers(e),next_move=e.Monster.NextMove?.Id,rng_counter=e.Monster.Rng!.Counter}).ToArray(),
      rng=Enum.GetValues<RunRngType>().ToDictionary(t=>t.ToString(),t=>s.RunState.Rng.GetRng(t).Counter)};
}
List<(int hand,int target)> Legal(CombatState s) {
    var result=new List<(int,int)>();var hand=s.Players[0].PlayerCombatState!.Hand.Cards;
    for(int i=0;i<hand.Count;++i){var card=hand[i];if(!card.CanPlay(out _))continue;
      if(card.TargetType==TargetType.AnyEnemy){foreach(var e in s.Enemies)if(e.IsHittable)result.Add((i,(int)(e.CombatId??throw new Exception("missing combat ID"))));}
      else result.Add((i,-1));}
    result.Add((-1,-1));return result;
}
void Measure(string id,string name,Func<ulong> op,int n,int warm,int samples){
    ulong sink=0;for(int i=0;i<warm;++i)sink+=op();
    var times=new List<double>();var allocated=new List<double>();var gc=new int[3];
    for(int s=0;s<samples;++s){long a=GC.GetAllocatedBytesForCurrentThread();int g0=GC.CollectionCount(0),g1=GC.CollectionCount(1),g2=GC.CollectionCount(2);
      long b=Stopwatch.GetTimestamp();for(int i=0;i<n;++i)sink+=op();long e=Stopwatch.GetTimestamp();
      times.Add((e-b)*1e9/Stopwatch.Frequency/n);allocated.Add((GC.GetAllocatedBytesForCurrentThread()-a)/(double)n);
      gc[0]+=GC.CollectionCount(0)-g0;gc[1]+=GC.CollectionCount(1)-g1;gc[2]+=GC.CollectionCount(2)-g2;}
    Emit(new {type="measurement",@case=id,operation=name,iterations=n,warmup=warm,samples_ns=times,allocated_bytes=allocated,gc_collections=gc,sink});
}
if(args[1]=="parallel") {
    int workers=int.Parse(args[2]),total=int.Parse(args[3]);var (_,root)=Setup(cases[0]);var hash=Digest(root);
    for(int i=0;i<512;++i){var e=root.Clone();e.Engine!.EndPlayerTurnAsync().GetAwaiter().GetResult();}
    using var ready=new Barrier(workers+1);using var start=new Barrier(workers+1);var sinks=new ulong[workers];
    var threads=Enumerable.Range(0,workers).Select(t=>new Thread(()=>{var local=root.Clone();ready.SignalAndWait();start.SignalAndWait();
      for(int i=t;i<total;i+=workers){var e=local.Clone();e.Engine!.EndPlayerTurnAsync().GetAwaiter().GetResult();sinks[t]+=(ulong)e.Players[0].PlayerCombatState!.Energy;}})).ToArray();
    foreach(var thread in threads)thread.Start();ready.SignalAndWait();long a=GC.GetTotalAllocatedBytes(true),b=Stopwatch.GetTimestamp();
    start.SignalAndWait();foreach(var thread in threads)thread.Join();double seconds=(Stopwatch.GetTimestamp()-b)/(double)Stopwatch.Frequency;
    if(Digest(root)!=hash)throw new Exception("parallel source mutated");
    Emit(new {type="parallel",@case=cases[0][0],workers,operations=total,seconds,ops_per_second=total/seconds,
        allocated_bytes=GC.GetTotalAllocatedBytes(true)-a,sink=sinks.Aggregate(0UL,(a,b)=>a+b)});return;
}
bool verify=args[1]=="verify";int n=args.Length>2?int.Parse(args[2]):64,warm=args.Length>3?int.Parse(args[3]):128,samples=args.Length>4?int.Parse(args[4]):7;
if(!verify){var (_,hot)=Setup(cases[0]);for(int i=0;i<2000;++i){var e=hot.Clone();e.Engine!.EndPlayerTurnAsync().GetAwaiter().GetResult();}}
int failures=0;
foreach(var c in cases){try{
    var (run,root)=Setup(c);var legal=Legal(root);var hash=Digest(root);var hand=root.Players[0].PlayerCombatState!.Hand.Cards;
    var card=legal.First(a=>a.hand>=0&&(hand[a.hand].Id.Entry.StartsWith("STRIKE_")||hand[a.hand].Id.Entry.StartsWith("DEFEND_")));
    Emit(new {type="action",@case=c[0],hand_index=card.hand,target=card.target});
    Emit(new {type="state",@case=c[0],boundary="root",state=Project(root)});
    var after=root.Clone();after.Engine!.EndPlayerTurnAsync().GetAwaiter().GetResult();
    Emit(new {type="state",@case=c[0],boundary="after_endturn",state=Project(after)});
    var played=root.Clone();var p=played.Players[0];var target=card.target<0?null:played.Enemies.Single(e=>e.CombatId==card.target);
    played.Engine!.PlayCardAsync(p,p.PlayerCombatState!.Hand.Cards[card.hand],target).GetAwaiter().GetResult();
    Emit(new {type="state",@case=c[0],boundary="after_card",state=Project(played)});
    if(!verify){
      Measure(c[0],"Clone",()=>{var e=root.Clone();return (ulong)e.Players[0].Creature.CurrentHp;},n*4,warm,samples);
      Measure(c[0],"Clone_EndTurn",()=>{var e=root.Clone();e.Engine!.EndPlayerTurnAsync().GetAwaiter().GetResult();return (ulong)e.Players[0].PlayerCombatState!.Energy;},n,warm,samples);
      Measure(c[0],"Clone_PlayCard",()=>{var e=root.Clone();var p=e.Players[0];var target=card.target<0?null:e.Enemies.Single(x=>x.CombatId==card.target);
          e.Engine!.PlayCardAsync(p,p.PlayerCombatState!.Hand.Cards[card.hand],target).GetAwaiter().GetResult();return (ulong)p.PlayerCombatState.Energy;},n,warm,samples);
      Measure(c[0],"LegalActions_harness",()=> (ulong)Legal(root).Count,n*32,warm,samples);
      Measure(c[0],"Observe",()=> (ulong)ObservationEncoder.EncodeCombatDecision(run,root).Candidates.Count,n*4,warm,samples);
      Measure(c[0],"ExactCombatDigest",()=>Digest(root),Math.Max(16,n/4),32,samples);
    }
    if(Digest(root)!=hash)throw new Exception("root changed during measurement");
    Emit(new {type="verified",@case=c[0],root_stable=true});
  }catch(Exception e){++failures;Emit(new {type="failure",@case=c[0],message=e.ToString()});}}
Environment.ExitCode=failures==0?0:2;
