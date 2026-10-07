using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Linq;
using System.Reflection;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;
using HarmonyLib;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Entities.Rngs;
using MegaCrit.Sts2.Core.Random;
using MegaCrit.Sts2.Core.Runs;

// Optional instrumentation for the combat oracle.  The patch is deliberately
// installed only for `combat --rng-ledger`; the normal oracle has no extra
// calls, stack walks, or output.
internal static class OracleRngLedger
{
    private static readonly FieldInfo CounterField = AccessTools.Field(typeof(Rng), "_counter")
        ?? throw new MissingFieldException(typeof(Rng).FullName, "_counter");
    private static readonly FieldInfo RunRngsField = AccessTools.Field(typeof(RunRngSet), "_rngs")
        ?? throw new MissingFieldException(typeof(RunRngSet).FullName, "_rngs");
    private static readonly FieldInfo PlayerRngsField = AccessTools.Field(typeof(PlayerRngSet), "_rngs")
        ?? throw new MissingFieldException(typeof(PlayerRngSet).FullName, "_rngs");
    private static readonly Dictionary<Rng, string> Streams = new(ReferenceEqualityComparer.Instance);
    private static readonly Regex GeneratedType = new("^<(?<name>[^>]+)>", RegexOptions.Compiled);
    private static readonly HashSet<string> DrawNames = new(StringComparer.Ordinal)
    {
        nameof(Rng.NextBool), nameof(Rng.NextInt), nameof(Rng.NextUnsignedInt),
        nameof(Rng.NextUnsignedLong), nameof(Rng.NextFloat), nameof(Rng.NextDouble),
        nameof(Rng.NextGaussianFloat), nameof(Rng.NextGaussianDouble), nameof(Rng.NextGaussianInt),
    };

    [ThreadStatic]
    private static int _depth;
    private static bool _installed;

    public static void Install()
    {
        if (_installed) return;
        _installed = true;
        Harmony harmony = new("sts2sim.oracle.rng-ledger");

        foreach (ConstructorInfo constructor in typeof(RunRngSet).GetConstructors(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic))
            harmony.Patch(constructor, postfix: new HarmonyMethod(typeof(OracleRngLedger), nameof(RegisterRunStreams)));
        foreach (ConstructorInfo constructor in typeof(PlayerRngSet).GetConstructors(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic))
            harmony.Patch(constructor, postfix: new HarmonyMethod(typeof(OracleRngLedger), nameof(RegisterPlayerStreams)));

        foreach (MethodInfo method in typeof(Rng).GetMethods(BindingFlags.Instance | BindingFlags.Public | BindingFlags.DeclaredOnly)
                     .Where(m => !m.IsGenericMethodDefinition && DrawNames.Contains(m.Name)))
        {
            harmony.Patch(method,
                prefix: new HarmonyMethod(typeof(OracleRngLedger), nameof(BeforeDraw)),
                postfix: new HarmonyMethod(typeof(OracleRngLedger), nameof(AfterDraw)),
                finalizer: new HarmonyMethod(typeof(OracleRngLedger), nameof(DrawFinalizer)));
        }
    }

    public static void BeginRun()
    {
        Streams.Clear();
    }

    public static void RegisterMonster(Rng rng, int combatId)
    {
        Streams[rng] = "monster." + combatId.ToString(CultureInfo.InvariantCulture);
    }

    private static void RegisterRunStreams(object __instance)
    {
        if (RunRngsField.GetValue(__instance) is not IDictionary rngs)
            throw new InvalidOperationException("RunRngSet._rngs is not a dictionary");
        foreach (DictionaryEntry entry in rngs)
            Streams[(Rng)entry.Value!] = ((RunRngType)entry.Key!).ToString();
    }

    private static void RegisterPlayerStreams(object __instance)
    {
        if (PlayerRngsField.GetValue(__instance) is not IDictionary rngs)
            throw new InvalidOperationException("PlayerRngSet._rngs is not a dictionary");
        foreach (DictionaryEntry entry in rngs)
            Streams[(Rng)entry.Value!] = "player." + ((PlayerRngType)entry.Key!).ToString();
    }

    private static void BeforeDraw(ref int __state)
    {
        __state = _depth++;
    }

    private static void AfterDraw(Rng __instance, MethodBase __originalMethod, object[] __args, object __result, int __state)
    {
        _depth = __state;
        if (__state != 0) return; // NextGaussian* calls NextDouble internally.
        EmitDraw(__instance, __originalMethod, __args, __result);
    }

    private static Exception? DrawFinalizer(Exception? __exception, int __state)
    {
        _depth = __state;
        return __exception;
    }

    private static void EmitDraw(Rng rng, MethodBase method, object[] args, object result)
    {
        // Rng.Chaotic is explicitly for presentation, save identifiers, and
        // other non-gameplay work.  Its seed is wall-clock based, so emitting
        // its values would make an otherwise deterministic combat trace vary
        // from process to process.
        if (ReferenceEquals(rng, Rng.Chaotic)) return;
        JsonArray normalizedArgs = NormalizeArgs(method.Name, args);
        string stream = Streams.TryGetValue(rng, out string? name) ? name : "other";
        int counter = (int)CounterField.GetValue(rng)!;
        var line = new JsonObject
        {
            ["event"] = "rng",
            ["stream"] = stream,
            ["method"] = method.Name,
            ["args"] = normalizedArgs,
            ["result"] = JsonSerializer.SerializeToNode(result),
            ["counter"] = counter,
            ["caller"] = CallerName(),
        };
        Program.Out.WriteLine(line.ToJsonString());
    }

    private static JsonArray NormalizeArgs(string method, object[] args)
    {
        static JsonNode Number(object value) => JsonSerializer.SerializeToNode(value)!;
        if (method is nameof(Rng.NextBool) or nameof(Rng.NextDouble) && args.Length == 0)
            return new JsonArray();
        if (method is nameof(Rng.NextInt) or nameof(Rng.NextUnsignedInt) or nameof(Rng.NextFloat))
        {
            return args.Length switch
            {
                1 => new JsonArray(Number(Convert.ChangeType(0, args[0].GetType(), CultureInfo.InvariantCulture)), Number(args[0])),
                2 => new JsonArray(Number(args[0]), Number(args[1])),
                _ => new JsonArray(args.Select(Number).ToArray()),
            };
        }
        if (method == nameof(Rng.NextUnsignedLong))
        {
            return args.Length switch
            {
                0 => new JsonArray(),
                1 => new JsonArray(0, Number(args[0])),
                2 => new JsonArray(Number(args[0]), Number(args[1])),
                _ => new JsonArray(args.Select(Number).ToArray()),
            };
        }
        if (method is nameof(Rng.NextGaussianFloat) or nameof(Rng.NextGaussianDouble) or nameof(Rng.NextGaussianInt))
            return new JsonArray(args.Select(Number).ToArray());
        return new JsonArray(args.Select(Number).ToArray());
    }

    private static string CallerName()
    {
        foreach (StackFrame frame in new StackTrace(false).GetFrames() ?? Array.Empty<StackFrame>())
        {
            MethodBase? method = frame.GetMethod();
            Type? type = method?.DeclaringType;
            if (method == null || type == null || type == typeof(Rng) || type == typeof(OracleRngLedger)) continue;
            string ns = type.Namespace ?? "";
            if (ns.StartsWith("MegaCrit.Sts2.Core.Random", StringComparison.Ordinal) ||
                ns.StartsWith("HarmonyLib", StringComparison.Ordinal) ||
                ns.StartsWith("System", StringComparison.Ordinal) ||
                ns.StartsWith("Microsoft", StringComparison.Ordinal)) continue;

            Match generated = GeneratedType.Match(type.Name);
            string methodName = method.Name;
            Type userType = type;
            if (generated.Success && methodName == "MoveNext") methodName = generated.Groups["name"].Value;
            else if (GeneratedType.Match(methodName) is Match methodMatch && methodMatch.Success)
                methodName = methodMatch.Groups["name"].Value;
            while (userType.DeclaringType != null && userType.Name.StartsWith("<", StringComparison.Ordinal))
                userType = userType.DeclaringType;
            int tick = userType.Name.IndexOf('`');
            string typeName = tick < 0 ? userType.Name : userType.Name[..tick];
            return typeName + "." + methodName;
        }
        return "<unknown>";
    }
}
