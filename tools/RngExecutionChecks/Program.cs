using System.Reflection;
using System.Runtime.CompilerServices;
using System.Security.Cryptography;
using System.Text.Json;
using HarmonyLib;
using MegaCrit.Sts2.Core.Random;

// These patches only observe the shipped methods; they never skip or replace
// an RNG method. Generic adapters invoke the DLL methods on indexed items, so
// nullable default(T) corresponds to the simulator's no-item index (-1).
internal static class Program {
    static readonly List<Call> calls = [];
    static readonly List<object> shuffles = [];
    static int parent = -1;
    static ulong sequence;
    public sealed record Value(string type, ulong bits);
    public sealed record State(int counter, ulong[] state);
    public sealed class Call {
        public required string operation {get;init;}
        public required Value[] parameters {get;init;}
        public required uint[] weight_bits {get;init;}
        public required State before {get;init;}
        public required int parent_call {get;init;}
        public required ulong sequence {get;init;}
        public State? after {get;set;}
        public Value? result {get;set;}
        public ulong finished_sequence {get;set;}
        public bool finished {get;set;}
    }
    static Value Bits(object? value) => value switch {
        null => new("void",0), bool b => new("bool",b?1UL:0UL),
        int v => new("int32",unchecked((uint)v)), uint v => new("uint32",v), ulong v => new("uint64",v),
        float v => new("float32",BitConverter.SingleToUInt32Bits(v)),
        double v => new("float64",BitConverter.DoubleToUInt64Bits(v)),
        _ => throw new InvalidOperationException($"Unsupported RNG result {value.GetType()}")
    };
    static State Capture(Rng rng) {
        var s=rng.ToSerializable(); return new(s.counter,[s.state0,s.state1,s.state2,s.state3]);
    }
    static int Begin(Rng rng,string operation,object[] args,uint[]? weights=null) {
        int index=calls.Count;
        calls.Add(new(){operation=operation,parameters=args.Select(Bits).ToArray(),weight_bits=weights??[],
            before=Capture(rng),parent_call=parent,sequence=sequence++});
        parent=index;return index;
    }
    static void Finish(Rng rng,int index,object? value) {
        var call=calls[index];call.after=Capture(rng);call.result=Bits(value);
        call.finished_sequence=sequence++;call.finished=true;parent=call.parent_call;
    }
    static string Name(MethodBase method) => method.Name switch {
        "NextInt" or "NextUnsignedInt" or "NextFloat" => method.Name+(method.GetParameters().Length==1?"(max)":"(min,max)"),
        "NextUnsignedLong" => method.Name+(method.GetParameters().Length switch {0=>"",1=>"(max)",_=>"(min,max)"}),
        "NextDouble" => method.Name+(method.GetParameters().Length==0?"":"(min,max)"),
        _ => method.Name
    };
    static void Prefix(Rng __instance,MethodBase __originalMethod,object[] __args,out int __state) =>
        __state=Begin(__instance,Name(__originalMethod),__args);
    static void Postfix(Rng __instance,object __result,int __state) => Finish(__instance,__state,__result);
    static void Finalizer(int __state,Exception? __exception) {
        if(__exception!=null) parent=calls[__state].parent_call;
    }
    static void Item(Rng rng,int count) {
        int index=Begin(rng,"NextItem",[count]);
        var items=Enumerable.Range(0,count).Select(v=>(int?)v).ToArray();
        Finish(rng,index,rng.NextItem(items)??-1);
    }
    static void Weighted(Rng rng,float[] weights) {
        int index=Begin(rng,"WeightedNextItem",[],weights.Select(BitConverter.SingleToUInt32Bits).ToArray());
        var items=Enumerable.Range(0,weights.Length).Select(v=>(int?)v).ToArray();
        Finish(rng,index,rng.WeightedNextItem(items,v=>weights[v!.Value])??-1);
    }
    static void Shuffle(Rng rng,int count) {
        int index=Begin(rng,"Shuffle",[count]);var items=Enumerable.Range(0,count).ToList();
        rng.Shuffle(items);Finish(rng,index,null);shuffles.Add(new {call=index,items});
    }
    static void Main() {
        var output=Console.Out;Console.SetOut(TextWriter.Null);
        GameBootstrap.InstallResolver(GameBootstrap.DefaultGameData);Check(output);
    }
    [MethodImpl(MethodImplOptions.NoInlining)]
    static void Check(TextWriter output) {
        GameBootstrap.Init(testMode:false);
        var harmony=new Harmony("sts2sim.rng-execution-checks");
        foreach(var method in typeof(Rng).GetMethods(BindingFlags.Public|BindingFlags.Instance)
            .Where(m=>!m.IsGenericMethod && m.Name.StartsWith("Next",StringComparison.Ordinal)))
            harmony.Patch(method,new HarmonyMethod(typeof(Program),nameof(Prefix)),
                new HarmonyMethod(typeof(Program),nameof(Postfix)),finalizer:new HarmonyMethod(typeof(Program),nameof(Finalizer)));
        string dll=Path.Combine(GameBootstrap.DefaultGameData,"sts2.dll");
        string dllSha=Convert.ToHexStringLower(SHA256.HashData(File.ReadAllBytes(dll)));
        ulong[] seeds=[0,1,2,ulong.MaxValue,0x0123456789abcdefUL,0x8000000000000000UL,123456789,987654321];
        foreach(ulong seed in seeds) {
            calls.Clear();shuffles.Clear();parent=-1;sequence=0;var rng=new Rng(seed);
            for(int repeat=0;repeat<12;repeat++) {
                rng.NextBool();rng.NextInt();rng.NextInt(1);rng.NextInt(-200,300);
                rng.NextInt(int.MinValue,int.MaxValue);
                rng.NextUnsignedInt();rng.NextUnsignedInt(4000000000,4294967295);
                rng.NextUnsignedLong();rng.NextUnsignedLong(ulong.MaxValue);
                rng.NextUnsignedLong(12345);rng.NextUnsignedLong(123456,ulong.MaxValue-1);
                rng.NextFloat();rng.NextFloat(-12.5f,123.125f);rng.NextFloat(0.25f,0.25f);
                rng.NextDouble();rng.NextDouble(-100.25,400.5);rng.NextDouble(0.25,0.25);
                rng.NextGaussianFloat(0.5f,4f,-100f,100f);
                rng.NextGaussianDouble(0.5,4,-100.25,100.25);
                rng.NextGaussianInt(0,12,-1,1);
                foreach(int count in new[]{0,1,17}) Item(rng,count);
                foreach(float[] weights in new float[][]{[],[0,0,0],[0.125f,0.25f,10f],[1e-12f,1e10f,3f]}) Weighted(rng,weights);
                foreach(int count in new[]{0,1,17}) Shuffle(rng,count);
            }
            if(parent!=-1 || calls.Any(c=>!c.finished)) throw new Exception("Unbalanced native RNG trace");
            output.WriteLine(JsonSerializer.Serialize(new {format="sts2oracle.rng_execution",version=1,
                seed,oracle_test_mode=false,game_dll_sha256=dllSha,calls,shuffles}));
        }
        harmony.UnpatchAll(harmony.Id);
    }
}
