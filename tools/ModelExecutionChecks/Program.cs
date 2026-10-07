using System.Reflection;
using System.Runtime.CompilerServices;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Collections;
using HarmonyLib;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Hooks;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Models.Events;
using MegaCrit.Sts2.Core.Models.Monsters;
using MegaCrit.Sts2.Core.Models.Powers;
using MegaCrit.Sts2.Core.Models.Relics;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Unlocks;
using MegaCrit.Sts2.Core.ValueProps;

internal static class Program {
    static readonly List<object> calls=[];
    static readonly Dictionary<object,int> fixtureRefs=new(ReferenceEqualityComparer.Instance);
    sealed record Argument(string name,string declared_type,bool is_out,bool is_by_ref,bool is_in,bool value_complete,object value);
    sealed class Frame {
        public int parent {get;init;}
        public string kind {get;init;}="";
        public string operation {get;init;}="";
        public string source_model_id {get;init;}="";
        public bool source_is_override {get;init;}
        public bool source_is_canonical {get;init;}
        public bool source_is_canonical_known {get;init;}
        public ulong sequence {get;init;}
        public ulong finished_sequence {get;set;}
        public bool finished {get;set;}
        public bool method_values_observed {get;set;}
        public bool method_returned {get;set;}
        public string declared_return_type {get;set;}="";
        public Argument[] parameters_before {get;set;}=[];
        public Argument[] parameters_after {get;set;}=[];
        public bool return_value_complete {get;set;}
        public object? return_value {get;set;}
        [JsonIgnore] public ParameterInfo[] signature {get;set;}=[];
        [JsonIgnore] public object[] raw_arguments {get;set;}=[];
    }
    static readonly List<Frame> frames=[];
    static int current=-1;
    static ulong sequence;
    static bool observing;
    static Frame Begin(string kind,string operation,string model="",bool isOverride=false,bool isCanonical=false) {
        var frame=new Frame {parent=current,kind=kind,operation=operation,source_model_id=model,
            source_is_override=isOverride,source_is_canonical=isCanonical,
            source_is_canonical_known=kind=="model_method",sequence=sequence++};
        current=frames.Count;frames.Add(frame);return frame;
    }
    static void Prefix(AbstractModel __instance,MethodBase __originalMethod,object[] __args,out Frame? __state) {
        __state=null;
        if(observing) {
            var method=(MethodInfo)__originalMethod;
            string operation=method.GetBaseDefinition().DeclaringType!.Name+"."+
                (method.Name.StartsWith("get_",StringComparison.Ordinal)?method.Name[4..]:method.Name);
            string model=__instance.Id.ToString();
            calls.Add(new {operation,source_model_id=model});
            __state=Begin("model_method",operation,model,method.DeclaringType!=method.GetBaseDefinition().DeclaringType,__instance.IsCanonical);
            ObserveBefore(__state,method.GetBaseDefinition(),__args);
        }
    }
    static void HookPrefix(MethodBase __originalMethod,object[] __args,out Frame? __state) {
        __state=observing?Begin("hook","Hook."+__originalMethod.Name):null;
        if(__state is not null)ObserveBefore(__state,(MethodInfo)__originalMethod,__args);
    }
    static void Finish(Frame frame,object[] arguments,Type resultType,object result) {
        frame.parameters_after=Arguments(frame.signature,arguments,false);
        frame.return_value=Freeze(resultType,result);frame.return_value_complete=true;frame.method_returned=true;
        frame.finished_sequence=sequence++;frame.finished=true;current=frame.parent;
    }
    static void Postfix(Frame? __state,decimal __result) {
        if(__state is null) return;
        Finish(__state,__state.raw_arguments,typeof(decimal),__result);
    }
    static void CardTypePostfix(Frame? __state,CardType __result) {
        if(__state is not null)Finish(__state,__state.raw_arguments,typeof(CardType),__result);
    }
    static void BoolOutPostfix(Frame? __state,bool __result,decimal modifiedCost) {
        if(__state is null)return;
        // Read the real named out value; __args in a postfix would write back
        // Harmony's stale prefix slot and change the method under observation.
        var completed=(object[])__state.raw_arguments.Clone();
        int slot=Array.FindIndex(__state.signature,p=>p.IsOut && p.Name=="modifiedCost");
        if(slot<0)throw new InvalidOperationException("Unqualified model out signature");
        completed[slot]=modifiedCost;Finish(__state,completed,typeof(bool),__result);
    }
    static void HookPostfix(Frame? __state,decimal __result,IEnumerable<AbstractModel> modifiers) {
        if(__state is null)return;
        // Do not inject __args here: Harmony copies its stale out slot back
        // after postfixes. Read the actual named out argument directly.
        var completed=(object[])__state.raw_arguments.Clone();
        int slot=Array.FindIndex(__state.signature,p=>p.IsOut && p.Name=="modifiers");
        if(slot<0)throw new InvalidOperationException("Unqualified Hook out signature");
        completed[slot]=modifiers;Finish(__state,completed,typeof(decimal),__result);
    }
    static void ObserveBefore(Frame frame,MethodInfo method,object[] args) {
        frame.signature=method.GetParameters();frame.declared_return_type=TypeName(method.ReturnType);
        frame.raw_arguments=(object[])args.Clone();
        frame.parameters_before=Arguments(frame.signature,args,true);frame.method_values_observed=true;
    }
    static Argument[] Arguments(ParameterInfo[] signature,object[] args,bool before) => signature.Select((p,i)=>new Argument(
        p.Name!,TypeName(p.ParameterType),p.IsOut,p.ParameterType.IsByRef,p.IsIn,true,
        before && p.IsOut ? new {kind="undefined"} : Freeze(p.ParameterType.IsByRef?p.ParameterType.GetElementType()!:p.ParameterType,args[i]))).ToArray();
    static object Freeze(Type type,object? value) {
        if(typeof(AbstractModel).IsAssignableFrom(type)) {
            if(value is null)return new {kind="model_ref",@null=true};
            var model=(AbstractModel)value;
            int kind=model switch {CardModel=>0,RelicModel=>1,PowerModel=>2,PotionModel=>3,MonsterModel=>4,
                EncounterModel=>5,EventModel=>6,AfflictionModel=>8,OrbModel=>9,_=>throw new InvalidOperationException("Unqualified native model kind")};
            return new {kind="model_ref",@ref=model.IsCanonical?0:fixtureRefs[model],model_kind=kind,
                is_canonical=model.IsCanonical,model_id=model.Id.ToString()};
        }
        if(type==typeof(Creature))return value is Creature creature ? new {kind="creature_ref",@ref=fixtureRefs[creature],combat_id=(long?)creature.CombatId??-1}
            : new {kind="creature_ref",@null=true};
        if(type==typeof(Player))return value is Player player ? new {kind="player_ref",@ref=1,net_id=player.NetId}
            : new {kind="player_ref",@null=true};
        if(typeof(IRunState).IsAssignableFrom(type))return new {kind="run_ref",bound=value is not null};
        if(typeof(ICombatState).IsAssignableFrom(type))return new {kind="combat_ref",bound=value is not null,exists=value is not null};
        if(type==typeof(CardPlay) && value is null)return new {kind="card_play",@null=true};
        if(type==typeof(decimal))return new {kind="decimal",bits=decimal.GetBits((decimal)value!).Select(v=>unchecked((uint)v)).ToArray()};
        if(type.IsEnum)return new {kind="enum",value=Convert.ToInt64(value)};
        if(type==typeof(bool))return new {kind="bool",value=(bool)value!};
        if(type==typeof(float))return new {kind="f32",bits=BitConverter.SingleToUInt32Bits((float)value!)};
        if(type==typeof(double))return new {kind="f64",bits=BitConverter.DoubleToUInt64Bits((double)value!)};
        if(type.IsPrimitive)return new {kind="integer",value=Convert.ToInt64(value)};
        // This component probe only observes materialized List/array outputs;
        // never enumerate an arbitrary lazy game IEnumerable for diagnostics.
        if(value is IList sequence) {
            var items=new List<object>();foreach(var item in sequence)items.Add(Freeze(item.GetType(),item));
            return new {kind="sequence",items};
        }
        throw new InvalidOperationException("Unqualified native value: "+TypeName(type));
    }
    static void Main(string[] args) {var output=Console.Out;Console.SetOut(TextWriter.Null);
        GameBootstrap.InstallResolver(GameBootstrap.DefaultGameData);
        if(args is ["--signatures"]) Signatures(output);
        else if(args.Length==0) Check(output);
        else throw new ArgumentException("usage: ModelExecutionChecks [--signatures]");}
    static string TypeName(Type type) {
        if(type.IsByRef)return TypeName(type.GetElementType()!)+"&";
        if(type.IsGenericType)return type.Name.Split('`')[0]+"<"+string.Join(",",type.GetGenericArguments().Select(TypeName))+">";
        return type.Name;
    }
    [MethodImpl(MethodImplOptions.NoInlining)]
    static void Signatures(TextWriter output) {
        var bases=new[]{typeof(AbstractModel),typeof(CardModel),typeof(PowerModel),typeof(MonsterModel),typeof(RelicModel),
            typeof(PotionModel),typeof(EncounterModel),typeof(EventModel),typeof(AfflictionModel),typeof(OrbModel)};
        var methods=bases.SelectMany(t=>t.GetMethods(BindingFlags.Instance|BindingFlags.Public|BindingFlags.NonPublic|BindingFlags.DeclaredOnly)
                .Where(m=>m.IsVirtual && !m.IsFinal && m.GetBaseDefinition().DeclaringType==t))
            .Concat(typeof(Hook).GetMethods(BindingFlags.Static|BindingFlags.Public|BindingFlags.NonPublic|BindingFlags.DeclaredOnly))
            .OrderBy(m=>m.DeclaringType!.Name,StringComparer.Ordinal).ThenBy(m=>m.MetadataToken).ToArray();
        output.WriteLine(JsonSerializer.Serialize(new {format="sts2oracle.execution_signatures",version=1,game_version="v0.111.0",
            game_dll_sha256=Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(File.ReadAllBytes(typeof(Hook).Assembly.Location))).ToLowerInvariant(),
            methods=methods.Select(m=>new {owner=m.DeclaringType!.Name,name=m.Name,returns=TypeName(m.ReturnType),
                parameters=m.GetParameters().Select(p=>new {name=p.Name,type=TypeName(p.ParameterType),is_out=p.IsOut,
                    is_in=p.IsIn,is_by_ref=p.ParameterType.IsByRef})})},new JsonSerializerOptions{WriteIndented=true}));
    }
    [MethodImpl(MethodImplOptions.NoInlining)]
    static void Check(TextWriter output) {
        GameBootstrap.Init(testMode:false);NonInteractiveMode.AutoSlayerCheck=()=>true;
        GameBootstrap.UseInMemorySaves();GameBootstrap.LoadLocalization();
        var harmony=new Harmony("sts2sim.model-execution-checks");
        var names=new HashSet<string>{"ModifyDamageAdditive","ModifyDamageMultiplicative","ModifyDamageCap","TryModifyEnergyCostInCombat","get_Type"};
        var methods=GameBootstrap.ModelTypes.Append(typeof(AbstractModel)).Append(typeof(CardModel))
            .SelectMany(t=>t.GetMethods(BindingFlags.Instance|BindingFlags.Public))
            .Where(m=>names.Contains(m.Name) && !m.IsAbstract && (m.Name!="get_Type" || m.ReturnType==typeof(CardType)))
            .Select(m=>m.DeclaringType!.GetMethod(m.Name,BindingFlags.Instance|BindingFlags.Public|BindingFlags.DeclaredOnly,
                null,m.GetParameters().Select(p=>p.ParameterType).ToArray(),null)!)
            .Distinct().ToArray();
        var player=Player.CreateForNewRun<Ironclad>(UnlockState.all,1);
        var run=RunState.CreateForNewRun([player],ModelDb.ActsByIndex.Select(a=>a[0].ToMutable()).ToArray(),[],GameMode.Standard,0,"MODELTRACE");
        RunManager.Instance.SetUpNewSingleplayer(run,shouldSave:false);player.ResetCombatState();
        var combat=new CombatState(runState:run);combat.AddPlayer(player);
        var enemy=combat.CreateCreature(ModelDb.Monster<Mawler>().ToMutable(),CombatSide.Enemy,null);
        combat.AddCreature(enemy);
        var strength=(StrengthPower)ModelDb.Power<StrengthPower>().ToMutable();strength.ApplyInternal(player.Creature,3,silent:true);
        var vulnerable=(VulnerablePower)ModelDb.Power<VulnerablePower>().ToMutable();vulnerable.ApplyInternal(enemy,2,silent:true);
        var intangible=(IntangiblePower)ModelDb.Power<IntangiblePower>().ToMutable();intangible.ApplyInternal(enemy,1,silent:true);
        // Explicit identity correspondence of this fixture, never inferred
        // from model names or native object addresses in the comparator.
        fixtureRefs[player.Creature]=1;fixtureRefs[enemy]=2;
        fixtureRefs[strength]=1;fixtureRefs[vulnerable]=2;fixtureRefs[intangible]=3;
        var canonicalCard=ModelDb.Card<StrikeIronclad>();
        var mutableCard=player.Deck.Cards[0];fixtureRefs[mutableCard]=1;
        var canonicalRelic=ModelDb.Relic<BurningBlood>();
        var mutableRelic=player.Relics[0];fixtureRefs[mutableRelic]=1;
        // Detached, owned models for direct calls, outside the combat listener
        // inventory. These do not change the existing damage fixture.
        var powerCard=ModelDb.Card<Inflame>().ToMutable();powerCard.Owner=player;fixtureRefs[powerCard]=21;
        var spiked=ModelDb.Relic<SpikedGauntlets>().ToMutable();spiked.Owner=player;fixtureRefs[spiked]=2;
        var otherCard=canonicalCard.ToMutable();fixtureRefs[otherCard]=22;
        var otherRelic=canonicalRelic.ToMutable();fixtureRefs[otherRelic]=3;
        var canonicalEvent=ModelDb.Event<SunkenStatue>();
        var mutableEvent=canonicalEvent.ToMutable();fixtureRefs[mutableEvent]=1;
        var cardModes=new[]{("null",(CardModel?)null),("canonical",canonicalCard),("mutable",mutableCard)};
        var baselines=new Dictionary<(string,int,int),(int[],string[])>();
        foreach(var (cardMode,card) in cardModes) foreach(int flags in new[]{0,2,4,6,8,10,12,14}) foreach(int damage in new[]{0,1,6,13}) {
            var baseline=Hook.ModifyDamage(run,combat,enemy,player.Creature,damage,ValueProp.Move,card,null,
                (ModifyDamageHookType)flags,CardPreviewMode.None,out var modifiers);
            baselines[(cardMode,flags,damage)]=(decimal.GetBits(baseline),modifiers.Select(m=>m.Id.ToString()).ToArray());
        }
        var costInputs=new List<(string id,string sourceMode,string cardMode,AbstractModel source,CardModel? card,decimal cost,bool result,int[] modified)>();
        var costValues=new[]{-1m,0m,new decimal(0,0,0,true,2),1.25m,4m,-2.50m,new decimal(1,0,0,false,28)};
        void Cost(string sourceMode,AbstractModel source,string cardMode,CardModel? card,decimal cost,int costIndex) {
            bool result=source.TryModifyEnergyCostInCombat(card!,cost,out decimal modified);
            costInputs.Add(($"cost/{sourceMode}/{cardMode}/{costIndex}",sourceMode,cardMode,source,card,cost,result,decimal.GetBits(modified)));
        }
        foreach(var (sourceMode,source) in new[]{("canonical",(AbstractModel)canonicalRelic),("mutable",mutableRelic),
            ("event_canonical",canonicalEvent),("event_mutable",mutableEvent)})
            foreach(var (cardMode,card) in cardModes) for(int i=0;i<costValues.Length;i++) Cost(sourceMode,source,cardMode,card,costValues[i],i);
        foreach(var (cardMode,card) in new[]{("mutable",mutableCard),("power",powerCard)})
            for(int i=0;i<costValues.Length;i++) Cost("spiked",spiked,cardMode,card,costValues[i],i);
        var identities=new[]{
            ("card/canonical_same",(AbstractModel)canonicalCard,(AbstractModel)ModelDb.Card<StrikeIronclad>()),
            ("card/canonical_different",(AbstractModel)canonicalCard,(AbstractModel)ModelDb.Card<DefendIronclad>()),
            ("card/canonical_mutable",(AbstractModel)canonicalCard,(AbstractModel)mutableCard),
            ("card/mutable_same",(AbstractModel)mutableCard,(AbstractModel)mutableCard),
            ("card/mutable_different",(AbstractModel)mutableCard,(AbstractModel)otherCard),
            ("relic/canonical_same",(AbstractModel)canonicalRelic,(AbstractModel)ModelDb.Relic<BurningBlood>()),
            ("relic/canonical_mutable",(AbstractModel)canonicalRelic,(AbstractModel)mutableRelic),
            ("relic/mutable_different",(AbstractModel)mutableRelic,(AbstractModel)otherRelic),
            ("event/canonical_same",(AbstractModel)canonicalEvent,(AbstractModel)ModelDb.Event<SunkenStatue>()),
            ("event/canonical_different",(AbstractModel)canonicalEvent,(AbstractModel)ModelDb.Event<SunkenTreasury>()),
            ("event/canonical_mutable",(AbstractModel)canonicalEvent,(AbstractModel)mutableEvent),
            ("event/mutable_same",(AbstractModel)mutableEvent,(AbstractModel)mutableEvent)};
        var planned=baselines.Keys.Select(k=>$"damage/{k.Item1}/{k.Item2}/{k.Item3}")
            .Concat(costInputs.Select(c=>c.id)).Concat(identities.Select(c=>"identity/"+c.Item1)).ToArray();
        output.WriteLine(JsonSerializer.Serialize(new {format="sts2oracle.model_execution_plan",version=2,
            oracle_test_mode=false,game_version="v0.111.0",
            game_dll_sha256=Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(File.ReadAllBytes(typeof(Hook).Assembly.Location))).ToLowerInvariant(),
            model_ref_scheme="explicit_fixture_correspondence; canonical_definition",case_ids=planned}));
        // Establish unpatched DLL results first, then verify that observation
        // preserved them (including real out values) for every recorded row.
        foreach(var method in methods) harmony.Patch(method,new HarmonyMethod(typeof(Program),nameof(Prefix)),
            new HarmonyMethod(typeof(Program),method.ReturnType==typeof(bool)?nameof(BoolOutPostfix)
                :method.ReturnType==typeof(CardType)?nameof(CardTypePostfix):nameof(Postfix)));
        foreach(var method in typeof(Hook).GetMethods(BindingFlags.Static|BindingFlags.Public|BindingFlags.NonPublic)
            .Where(m=>m.Name is "ModifyDamage" or "ModifyDamageInternal")) {
            if(method.ReturnType!=typeof(decimal)) throw new InvalidOperationException("probe only observes synchronous decimal Hook calls");
            harmony.Patch(method,new HarmonyMethod(typeof(Program),nameof(HookPrefix)),new HarmonyMethod(typeof(Program),nameof(HookPostfix)));
        }
        void Observe(Action body) {
            calls.Clear();frames.Clear();current=-1;sequence=0;observing=true;
            try {body();} finally {observing=false;}
            if(current!=-1 || frames.Any(f=>!f.finished))throw new InvalidOperationException("Native frame did not close");
        }
        foreach(var (cardMode,card) in cardModes) foreach(int flags in new[]{0,2,4,6,8,10,12,14}) foreach(int damage in new[]{0,1,6,13}) {
            decimal result=0;
            IEnumerable<AbstractModel> modifiers=[];
            Observe(()=>result=Hook.ModifyDamage(run,combat,enemy,player.Creature,damage,ValueProp.Move,card,null,
                (ModifyDamageHookType)flags,CardPreviewMode.None,out modifiers));
            var baseline=baselines[(cardMode,flags,damage)];
            if(!decimal.GetBits(result).SequenceEqual(baseline.Item1)
                || !modifiers.Select(m=>m.Id.ToString()).SequenceEqual(baseline.Item2))
                throw new InvalidOperationException("Native diagnostic observer changed unpatched DLL result/out values");
            output.WriteLine(JsonSerializer.Serialize(new {format="sts2oracle.model_execution",version=2,case_kind="damage",
                case_id=$"damage/{cardMode}/{flags}/{damage}",card_mode=cardMode,
                oracle_test_mode=false,unpatched_baseline_equal=true,flags,damage,result_bits=decimal.GetBits(result).Select(v=>unchecked((uint)v)),
                calls,frames,modifiers=modifiers.Select(m=>m.Id.ToString())}));
        }
        foreach(var c in costInputs) {
            bool result=false;decimal modified=0;
            Observe(()=>result=c.source.TryModifyEnergyCostInCombat(c.card!,c.cost,out modified));
            if(result!=c.result || !decimal.GetBits(modified).SequenceEqual(c.modified))
                throw new InvalidOperationException("Native cost observer changed unpatched bool/out values: "+c.id);
            output.WriteLine(JsonSerializer.Serialize(new {format="sts2oracle.model_execution",version=2,case_kind="energy_cost_out",
                case_id=c.id,source_mode=c.sourceMode,card_mode=c.cardMode,
                oracle_test_mode=false,unpatched_baseline_equal=true,cost_bits=decimal.GetBits(c.cost).Select(v=>unchecked((uint)v)),
                result_bool=result,modified_cost_bits=decimal.GetBits(modified).Select(v=>unchecked((uint)v)),calls,frames}));
        }
        foreach(var (name,left,right) in identities) {
            output.WriteLine(JsonSerializer.Serialize(new {format="sts2oracle.model_execution",version=2,case_kind="identity",
                case_id="identity/"+name,identity_case=name,oracle_test_mode=false,unpatched_baseline_equal=true,
                left=Freeze(typeof(AbstractModel),left),right=Freeze(typeof(AbstractModel),right),same=ReferenceEquals(left,right)}));
        }
        harmony.UnpatchAll(harmony.Id);RunManager.Instance.CleanUp();
        output.WriteLine(JsonSerializer.Serialize(new {format="sts2oracle.model_execution_end",version=2,complete=true,rows=planned.Length}));
    }
}
