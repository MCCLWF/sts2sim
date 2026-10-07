using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.CompilerServices;
using System.Text.Encodings.Web;
using System.Text.Json;
using System.Text.Json.Nodes;
using MegaCrit.Sts2.Core.Entities.Ascension;
using MegaCrit.Sts2.Core.Localization.DynamicVars;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Multiplayer.Serialization;
using MegaCrit.Sts2.Core.Runs;

internal static class Exporter
{
    private const string ModelsNs = "MegaCrit.Sts2.Core.Models";
    private const BindingFlags DeclaredInstance =
        BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly;

    // Cosmetic or engine-backed members: they either need Godot resources or
    // carry no simulation-relevant data.
    private static readonly string[] DeniedNameParts =
    {
        "Path", "Icon", "Portrait", "Sfx", "Vfx", "Scene", "Texture", "Shader", "Spine", "Material", "Anim",
    };

    private static readonly HashSet<string> VarBaseMembers = new(StringComparer.Ordinal)
    {
        "Name", "BaseValue", "EnchantedValue", "PreviewValue", "IntValue", "WasJustUpgraded",
    };

    // "DeclaringType.Property" entries whose getters call into the Godot engine
    // (native, uncatchable crash without it). Maintained by find_native_getters.sh.
    private static readonly HashSet<string> NativeGetters = LoadNativeGetters();

    private static HashSet<string> LoadNativeGetters()
    {
        string path = Path.Combine(AppContext.BaseDirectory, "native_getters.txt");
        var set = new HashSet<string>(StringComparer.Ordinal);
        if (File.Exists(path))
        {
            foreach (string line in File.ReadAllLines(path))
            {
                string t = line.Trim();
                if (t.Length > 0 && !t.StartsWith('#')) set.Add(t);
            }
        }
        return set;
    }

    private static int _getterErrors;
    private static readonly HashSet<Type> _enums = new();
    // STS2EXP_TRACE=1 logs every getter before it runs, to pinpoint native (Godot) crashes.
    private static readonly bool Trace = Environment.GetEnvironmentVariable("STS2EXP_TRACE") == "1";
    private static Dictionary<ModelId, AbstractModel> _contentById = null!;

    private static void Step(string what)
    {
        if (Trace) Console.Error.WriteLine("step " + what);
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    public static int Run(Options o)
    {
        Type[] types = Bootstrap();

        var root = new JsonObject
        {
            ["meta"] = Meta(o),
        };
        Step("virtuals");
        root["virtuals"] = Virtuals();
        Step("models");

        var models = new JsonArray();
        int ascensionDiffs = 0;
        for (int i = 0; i < types.Length; i++)
        {
            Step("model " + types[i].Name);
            JsonObject entry = ModelEntry(i, types[i], o);
            if (entry["ascension"] is JsonObject asc && asc.Count > 0) ascensionDiffs++;
            models.Add(entry);
        }
        root["models"] = models;
        root["modeldb"] = ModelDbLists();
        root["enums"] = Enums();

        using (FileStream fs = File.Create(o.Out))
        using (var w = new Utf8JsonWriter(fs, new JsonWriterOptions
               {
                   Indented = true,
                   Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
               }))
        {
            root.WriteTo(w);
        }

        Console.Error.WriteLine(
            $"exported {types.Length} models to {o.Out}; {ascensionDiffs} with ascension-dependent values; " +
            $"{_getterErrors} property getters threw (expected for run-state-dependent members)");
        return 0;
    }

    private static Type[] Bootstrap()
    {
        Step("bootstrap");
        GameBootstrap.Init(testMode: false);
        _contentById = GameBootstrap.ContentById;
        return GameBootstrap.ModelTypes;
    }

    private static JsonObject Meta(Options o)
    {
        var meta = new JsonObject();
        string releaseInfo = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(o.GameData))!, "release_info.json");
        if (File.Exists(releaseInfo)) meta["release_info"] = JsonNode.Parse(File.ReadAllText(releaseInfo));
        meta["model_id_hash"] = ModelIdSerializationCache.Hash;
        meta["model_count"] = _contentById.Count;
        return meta;
    }

    // ---------------------------------------------------------------- virtuals

    private static IEnumerable<Type> FrameworkTypes() =>
        typeof(AbstractModel).Assembly.GetTypes()
            .Where(t => t.Namespace == ModelsNs && typeof(AbstractModel).IsAssignableFrom(t))
            .OrderBy(t => t == typeof(AbstractModel) ? 0 : 1)
            .ThenBy(t => t.Name, StringComparer.Ordinal);

    // Every overridable member introduced by the framework base classes, in
    // declaration order. AbstractModel's list is the hook universe.
    private static JsonObject Virtuals()
    {
        var result = new JsonObject();
        foreach (Type t in FrameworkTypes())
        {
            var members = t.GetMethods(DeclaredInstance)
                .Where(m => m.IsVirtual && !m.IsFinal && m.GetBaseDefinition().DeclaringType == t)
                .OrderBy(m => m.MetadataToken)
                .ToList();
            if (members.Count == 0) continue;
            var arr = new JsonArray();
            foreach (MethodInfo m in members)
            {
                arr.Add(new JsonObject
                {
                    ["name"] = m.Name,
                    ["returns"] = TypeName(m.ReturnType),
                    ["params"] = new JsonArray(m.GetParameters()
                        .Select(p => (JsonNode)JsonValue.Create(TypeName(p.ParameterType) + " " + p.Name)!)
                        .ToArray()),
                    ["abstract"] = m.IsAbstract,
                });
            }
            result[t.Name] = arr;
        }
        return result;
    }

    // ModelDb's static content lists (shared events, act order, pools, ...), in
    // the order the game iterates them.
    private static JsonObject ModelDbLists()
    {
        var result = new JsonObject();
        foreach (PropertyInfo p in typeof(ModelDb).GetProperties(BindingFlags.Static | BindingFlags.Public)
                     .OrderBy(p => p.Name, StringComparer.Ordinal))
        {
            if (p.Name == "All" || p.Name.StartsWith("Debug", StringComparison.Ordinal)) continue;
            Type? e = ElementType(p.PropertyType);
            if (e == null) continue;
            bool models = IsModel(e) || (ElementType(e) is Type inner && IsModel(inner));
            if (!models) continue;
            try
            {
                result[p.Name] = ToNode(p.GetValue(null), depth: 1);
            }
            catch (Exception ex)
            {
                result[p.Name] = "error: " + ex.GetType().Name;
            }
        }
        return result;
    }

    // Engine enums the simulator needs even though no model property exposes them.
    private static readonly string[] ExtraEnums =
    {
        "AscensionLevel", "MapPointType", "PileType", "CombatSide", "PlayerTurnPhase", "IntentType",
        "MoveRepeatType", "CardRarityOddsType", "RewardType",
        // Used by the combat engine port (game/).
        "LocalCostType", "LocalCostModifierExpiration", "CostModifiers", "UnplayableReason",
        "CardPilePosition", "AutoPlayType", "KeywordSources", "CardScope", "HpLossHookPhase",
        "ModifyDamageHookType", "GoldLossType", "PurchaseStatus", "CardCreationSource",
        "CardCreationFlags", "GameActionType", "PlayerChoiceType", "CombatRoomMode", "MapPointState",
        "PotionProcureFailureReason", "GameMode", "CrystalSphereItemType", "RelicPickingResultType",
        "CardPreviewMode",
    };

    // Full definitions of every game enum that appeared in exported values.
    private static JsonObject Enums()
    {
        foreach (Type t in typeof(AbstractModel).Assembly.GetTypes())
        {
            if (t.IsEnum && Array.IndexOf(ExtraEnums, t.Name) >= 0 &&
                t.Namespace?.StartsWith("MegaCrit.Sts2.Core", StringComparison.Ordinal) == true)
                _enums.Add(t);
        }
        var result = new JsonObject();
        var names = _enums.GroupBy(t => t.Name).ToDictionary(g => g.Key, g => g.Count());
        foreach (Type t in _enums.Where(t => t.Namespace?.StartsWith("MegaCrit", StringComparison.Ordinal) == true)
                     .OrderBy(t => t.FullName, StringComparer.Ordinal))
        {
            string key = names[t.Name] > 1 ? t.FullName!.Replace("MegaCrit.Sts2.Core.", "") : t.Name;
            var values = new JsonArray();
            foreach (object v in Enum.GetValues(t))
            {
                values.Add(new JsonObject
                {
                    ["name"] = Enum.GetName(t, v),
                    ["value"] = Convert.ToInt64(v, CultureInfo.InvariantCulture),
                });
            }
            result[key] = new JsonObject
            {
                ["namespace"] = t.Namespace,
                ["underlying"] = Enum.GetUnderlyingType(t).Name,
                ["flags"] = t.IsDefined(typeof(FlagsAttribute), false),
                ["values"] = values,
            };
        }
        return result;
    }

    // Overridden framework members, i.e. the hooks/behaviours a content class
    // implements and the C++ port must implement too.
    private static JsonArray Overrides(Type type)
    {
        var names = new SortedSet<string>(StringComparer.Ordinal);
        for (Type? t = type; t != null && t.Namespace != ModelsNs; t = t.BaseType)
        {
            foreach (MethodInfo m in t.GetMethods(DeclaredInstance))
            {
                MethodInfo b = m.GetBaseDefinition();
                if (b.DeclaringType == m.DeclaringType) continue;
                if (b.DeclaringType?.Namespace?.StartsWith("MegaCrit.Sts2", StringComparison.Ordinal) != true) continue;
                string name = b.DeclaringType.Name + "." + m.Name;
                if (t != type) name += " (via " + t.Name + ")";
                names.Add(name);
            }
        }
        return new JsonArray(names.Select(n => (JsonNode)JsonValue.Create(n)!).ToArray());
    }

    // ---------------------------------------------------------------- models

    private static JsonObject ModelEntry(int index, Type type, Options o)
    {
        ModelId id = ModelDb.GetId(type);
        AbstractModel model = _contentById[id];

        var entry = new JsonObject
        {
            ["index"] = index,
            ["id"] = id.ToString(),
            ["type"] = type.FullName!.StartsWith(ModelsNs + ".", StringComparison.Ordinal)
                ? type.FullName.Substring(ModelsNs.Length + 1)
                : type.FullName,
            ["kind"] = FrameworkBase(type)?.Name,
        };
        var contentBases = new List<string>();
        for (Type? t = type.BaseType; t != null && t.Namespace != ModelsNs; t = t.BaseType) contentBases.Add(t.Name);
        if (contentBases.Count > 0) entry["content_bases"] = new JsonArray(contentBases.Select(n => (JsonNode)JsonValue.Create(n)!).ToArray());
        if (type.Namespace?.Contains(".Mock", StringComparison.Ordinal) == true) entry["mock"] = true;
        if (o.Src != null)
        {
            string rel = SourcePath(type);
            if (File.Exists(Path.Combine(o.Src, rel))) entry["source"] = rel;
        }
        entry["overrides"] = Overrides(type);

        JsonObject snap = Snapshot(model);
        // CrystalSphere.DynamicVars currently fails in the headless exporter
        // because its CanonicalVars also constructs a Godot-backed StringVar
        // title. Preserve the three numeric vars from the v0.111.0 source so
        // regenerating content.json does not silently erase event semantics.
        if (type.FullName == ModelsNs + ".Events.CrystalSphere" && snap["vars"] == null)
        {
            snap["vars"] = new JsonArray
            {
                new JsonObject { ["name"] = "UncoverFutureCost", ["type"] = "DynamicVar", ["base"] = "50" },
                new JsonObject { ["name"] = "UncoverFutureProphesizeCount", ["type"] = "IntVar", ["base"] = "3" },
                new JsonObject { ["name"] = "PaymentPlanCount", ["type"] = "IntVar", ["base"] = "6" },
            };
        }
        foreach (var kv in snap.ToList())
        {
            snap.Remove(kv.Key);
            entry[kv.Key] = kv.Value;
        }

        if (model is CardModel card) entry["upgrades"] = Upgrades(card);
        if (o.Ascension)
        {
            JsonObject asc = AscensionDiffs(type, id);
            if (asc.Count > 0) entry["ascension"] = asc;
        }
        return entry;
    }

    private static Type? FrameworkBase(Type type)
    {
        for (Type? t = type.BaseType; t != null; t = t.BaseType)
        {
            if (t.Namespace == ModelsNs) return t;
        }
        return null;
    }

    // Namespace MegaCrit.Sts2.Core.Models.Cards + type Bash -> Models/Cards/Bash.cs
    private static string SourcePath(Type type)
    {
        string ns = type.Namespace ?? "";
        const string prefix = "MegaCrit.Sts2.Core.";
        if (ns.StartsWith(prefix, StringComparison.Ordinal)) ns = ns.Substring(prefix.Length);
        string name = type.IsNested ? type.DeclaringType!.Name : type.Name;
        return Path.Combine(ns.Replace('.', '/'), name + ".cs");
    }

    private static JsonObject Snapshot(object model)
    {
        var snap = new JsonObject
        {
            ["props"] = Props(model, depth: 0, skip: null),
            ["fields"] = Fields(model),
        };
        JsonArray? vars = Vars(model);
        if (vars != null) snap["vars"] = vars;
        return snap;
    }

    private static JsonArray Upgrades(CardModel canonical)
    {
        var result = new JsonArray();
        int max = 1;
        try
        {
            max = Math.Min(canonical.MaxUpgradeLevel, 3);
        }
        catch
        {
            _getterErrors++;
        }
        try
        {
            CardModel card = canonical.ToMutable();
            JsonObject prev = Snapshot(card);
            for (int level = 1; level <= max; level++)
            {
                card.UpgradeInternal();
                card.FinalizeUpgradeInternal();
                JsonObject cur = Snapshot(card);
                result.Add(Diff(prev, cur));
                prev = cur;
            }
        }
        catch (Exception e)
        {
            result.Add(new JsonObject { ["error"] = e.GetType().Name + ": " + e.Message });
        }
        return result;
    }

    // Instantiates a fresh model at each cumulative ascension level (the game's
    // AscensionManager.HasLevel is `_level >= level`) and records what changes
    // relative to the previous level.
    private static JsonObject AscensionDiffs(Type type, ModelId id)
    {
        var result = new JsonObject();
        PropertyInfo stateProp = typeof(RunManager).GetProperty("State", BindingFlags.Instance | BindingFlags.NonPublic)!;
        PropertyInfo ascProp = typeof(RunManager).GetProperty(nameof(RunManager.AscensionManager))!;
        RunManager rm = RunManager.Instance;
        object? oldState = stateProp.GetValue(rm);
        object? oldAsc = ascProp.GetValue(rm);
        AbstractModel canonical = _contentById[id];
        try
        {
            JsonObject? prev = null;
            foreach (AscensionLevel level in Enum.GetValues<AscensionLevel>())
            {
                if (level == AscensionLevel.None)
                {
                    stateProp.SetValue(rm, null);
                }
                else
                {
                    stateProp.SetValue(rm, RuntimeHelpers.GetUninitializedObject(typeof(RunState)));
                    ascProp.SetValue(rm, new AscensionManager(level));
                }

                // The AbstractModel constructor refuses to create a model whose
                // id is registered, so unregister it briefly. Dictionary reuses
                // the freed slot, which keeps ModelDb's enumeration order intact.
                _contentById.Remove(id);
                AbstractModel fresh;
                try
                {
                    fresh = (AbstractModel)Activator.CreateInstance(type)!;
                }
                finally
                {
                    _contentById[id] = canonical;
                }

                JsonObject cur = Snapshot(fresh);
                if (prev != null)
                {
                    JsonObject diff = Diff(prev, cur);
                    if (diff.Count > 0) result[level.ToString()] = diff;
                }
                prev = cur;
            }
        }
        catch (Exception e)
        {
            result["error"] = e.GetType().Name + ": " + e.Message;
        }
        finally
        {
            stateProp.SetValue(rm, oldState);
            ascProp.SetValue(rm, oldAsc);
        }
        return result;
    }

    // props: changed keys with their new values; vars: changed/added vars by name.
    private static JsonObject Diff(JsonObject before, JsonObject after)
    {
        var diff = new JsonObject();
        var bp = before["props"] as JsonObject ?? new JsonObject();
        var ap = after["props"] as JsonObject ?? new JsonObject();
        var props = new JsonObject();
        foreach (var kv in ap)
        {
            if (!bp.TryGetPropertyValue(kv.Key, out JsonNode? old) || !JsonNode.DeepEquals(old, kv.Value))
                props[kv.Key] = kv.Value?.DeepClone();
        }
        if (props.Count > 0) diff["props"] = props;

        var bf = before["fields"] as JsonObject ?? new JsonObject();
        var af = after["fields"] as JsonObject ?? new JsonObject();
        var fields = new JsonObject();
        foreach (var kv in af)
        {
            if (!bf.TryGetPropertyValue(kv.Key, out JsonNode? old) || !JsonNode.DeepEquals(old, kv.Value))
                fields[kv.Key] = kv.Value?.DeepClone();
        }
        if (fields.Count > 0) diff["fields"] = fields;

        var bv = (before["vars"] as JsonArray ?? new JsonArray()).OfType<JsonObject>()
            .ToDictionary(v => v["name"]!.GetValue<string>());
        var vars = new JsonArray();
        foreach (JsonObject v in (after["vars"] as JsonArray ?? new JsonArray()).OfType<JsonObject>())
        {
            string name = v["name"]!.GetValue<string>();
            if (!bv.TryGetValue(name, out JsonObject? old) || !JsonNode.DeepEquals(old, v)) vars.Add(v.DeepClone());
        }
        if (vars.Count > 0) diff["vars"] = vars;
        return diff;
    }

    // ---------------------------------------------------------------- values

    private static JsonArray? Vars(object model)
    {
        PropertyInfo? p = model.GetType().GetProperty("DynamicVars", BindingFlags.Instance | BindingFlags.Public);
        if (p == null) return null;
        object? value;
        try
        {
            value = p.GetValue(model);
        }
        catch
        {
            _getterErrors++;
            return null;
        }
        if (value is not DynamicVarSet set) return null;
        var arr = new JsonArray();
        foreach (DynamicVar v in set.Values)
        {
            var o = new JsonObject
            {
                ["name"] = v.Name,
                ["type"] = TypeName(v.GetType()),
                ["base"] = v.BaseValue.ToString(CultureInfo.InvariantCulture),
            };
            JsonObject props = Props(v, depth: 1, skip: VarBaseMembers);
            if (props.Count > 0) o["props"] = props;
            arr.Add(o);
        }
        return arr;
    }

    // Private leaf-typed instance fields across the class hierarchy (upgrade
    // state such as CardEnergyCost._base is only visible here). Compiler
    // backing fields are skipped; the matching properties are in "props".
    private static JsonObject Fields(object obj)
    {
        var result = new JsonObject();
        for (Type? t = obj.GetType(); t != null && t != typeof(object); t = t.BaseType)
        {
            foreach (FieldInfo f in t.GetFields(DeclaredInstance).OrderBy(f => f.MetadataToken))
            {
                if (f.IsPublic || f.Name.Contains('<') || !IsLeafCore(f.FieldType)) continue;
                string key = t == obj.GetType() ? f.Name : t.Name + "." + f.Name;
                if (result.ContainsKey(key)) continue;
                if (Trace) Console.Error.WriteLine("  field " + t.Name + "." + f.Name + " : " + f.FieldType.Name);
                try
                {
                    result[key] = ToNode(FieldReader(f)(obj), depth: 1);
                }
                catch
                {
                    _getterErrors++;
                }
            }
        }
        return result;
    }

    private static readonly Dictionary<FieldInfo, Func<object, object?>> _fieldReaders = new();

    // FieldInfo.GetValue runs the declaring class's static constructor, and
    // several model base classes allocate Godot objects there (native crash).
    // A raw ldfld of an instance field does not trigger class initialization.
    private static Func<object, object?> FieldReader(FieldInfo f)
    {
        if (_fieldReaders.TryGetValue(f, out var reader)) return reader;
        var dm = new DynamicMethod("read_" + f.Name, typeof(object), new[] { typeof(object) }, typeof(Exporter).Module, skipVisibility: true);
        ILGenerator il = dm.GetILGenerator();
        il.Emit(OpCodes.Ldarg_0);
        il.Emit(f.DeclaringType!.IsValueType ? OpCodes.Unbox : OpCodes.Castclass, f.DeclaringType);
        il.Emit(OpCodes.Ldfld, f);
        if (f.FieldType.IsValueType) il.Emit(OpCodes.Box, f.FieldType);
        il.Emit(OpCodes.Ret);
        reader = (Func<object, object?>)dm.CreateDelegate(typeof(Func<object, object?>));
        _fieldReaders[f] = reader;
        return reader;
    }

    private static JsonObject Props(object obj, int depth, HashSet<string>? skip)
    {
        var result = new JsonObject();
        foreach (PropertyInfo p in obj.GetType()
                     .GetProperties(BindingFlags.Instance | BindingFlags.Public)
                     .OrderBy(p => p.Name, StringComparer.Ordinal))
        {
            if (p.GetIndexParameters().Length > 0 || p.GetMethod == null) continue;
            if (skip != null && skip.Contains(p.Name)) continue;
            if (p.Name == "DynamicVars") continue;
            if (DeniedNameParts.Any(d => p.Name.Contains(d, StringComparison.Ordinal))) continue;
            if (!IsExportable(p.PropertyType, depth)) continue;
            if (result.ContainsKey(p.Name)) continue;  // hidden (new) members
            string key = p.DeclaringType!.Name + "." + p.Name;
            if (NativeGetters.Contains(key)) continue;
            object? value;
            if (Trace) Console.Error.WriteLine("  get " + key + " on " + obj.GetType().Name);
            try
            {
                value = p.GetValue(obj);
            }
            catch
            {
                _getterErrors++;
                continue;
            }
            JsonNode? node;
            try
            {
                node = ToNode(value, depth);
            }
            catch
            {
                _getterErrors++;
                continue;
            }
            result[p.Name] = node;
        }
        return result;
    }

    private static bool IsLeaf(Type t)
    {
        Type u = Nullable.GetUnderlyingType(t) ?? t;
        if (u.IsEnum) _enums.Add(u);
        return IsLeafCore(t);
    }

    private static bool IsLeafCore(Type t) =>
        t.IsPrimitive || t.IsEnum || t == typeof(decimal) || t == typeof(string) || t == typeof(ModelId) ||
        (Nullable.GetUnderlyingType(t) is Type u && IsLeafCore(u));

    private static bool IsModel(Type t) => typeof(AbstractModel).IsAssignableFrom(t);

    // Plain data classes from the game (e.g. CardEnergyCost) are dumped one level deep.
    private static bool IsNestable(Type t) =>
        t.IsClass && !IsModel(t) && !typeof(Delegate).IsAssignableFrom(t) &&
        t.Namespace?.StartsWith("MegaCrit.Sts2.Core.Entities", StringComparison.Ordinal) == true &&
        !t.FullName!.Contains("Godot", StringComparison.Ordinal) &&
        !typeof(Godot.GodotObject).IsAssignableFrom(t);

    private static Type? ElementType(Type t)
    {
        if (t == typeof(string)) return null;
        if (t.IsArray) return t.GetElementType();
        Type? ie = t.IsGenericType && t.GetGenericTypeDefinition() == typeof(IEnumerable<>)
            ? t
            : t.GetInterfaces().FirstOrDefault(i => i.IsGenericType && i.GetGenericTypeDefinition() == typeof(IEnumerable<>));
        return ie?.GetGenericArguments()[0];
    }

    private static bool IsExportable(Type t, int depth)
    {
        if (IsLeaf(t) || IsModel(t)) return true;
        if (ElementType(t) is Type e) return IsLeaf(e) || IsModel(e);
        return depth == 0 && IsNestable(t);
    }

    private static JsonNode? ToNode(object? v, int depth)
    {
        switch (v)
        {
            case null: return null;
            case bool b: return JsonValue.Create(b);
            case string s: return JsonValue.Create(s);
            case decimal d: return JsonValue.Create(d.ToString(CultureInfo.InvariantCulture));
            case float f: return float.IsFinite(f) ? JsonValue.Create(f) : JsonValue.Create(f.ToString(CultureInfo.InvariantCulture));
            case double d: return double.IsFinite(d) ? JsonValue.Create(d) : JsonValue.Create(d.ToString(CultureInfo.InvariantCulture));
            case Enum e: return JsonValue.Create(e.ToString());
            case ModelId id: return JsonValue.Create(id.ToString());
            case AbstractModel m: return JsonValue.Create(ModelRef(m));
            case sbyte or byte or short or ushort or int or uint or long:
                return JsonValue.Create(Convert.ToInt64(v, CultureInfo.InvariantCulture));
            case ulong ul: return JsonValue.Create(ul);
            case char c: return JsonValue.Create(c.ToString());
            case IEnumerable seq:
            {
                var arr = new JsonArray();
                int n = 0;
                foreach (object? item in seq)
                {
                    if (++n > 5000) break;
                    arr.Add(ToNode(item, depth + 1));
                }
                return arr;
            }
            default:
                if (depth == 0 && IsNestable(v.GetType()))
                {
                    JsonObject nested = Props(v, depth + 1, skip: null);
                    foreach (var kv in Fields(v)) nested[kv.Key] = kv.Value?.DeepClone();
                    return nested;
                }
                return JsonValue.Create(v.ToString());
        }
    }

    private static string ModelRef(AbstractModel m)
    {
        try
        {
            return m.Id.ToString();
        }
        catch
        {
            return m.GetType().Name;
        }
    }

    private static string TypeName(Type t)
    {
        if (!t.IsGenericType) return t.Name;
        string name = t.Name.Split('`')[0];
        return name + "<" + string.Join(",", t.GetGenericArguments().Select(TypeName)) + ">";
    }
}
