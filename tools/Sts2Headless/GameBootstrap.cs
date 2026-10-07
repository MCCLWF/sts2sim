using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.Loader;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Localization;
using MegaCrit.Sts2.Core.Modding;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Multiplayer.Serialization;
using MegaCrit.Sts2.Core.Saves;
using MegaCrit.Sts2.Core.Saves.Test;

// Shared by the headless tools: brings sts2.dll's model database up in a plain
// .NET process, mirroring OneTimeInitialization.ExecuteEssential minus
// localization and atlases.
internal static class GameBootstrap
{
    public const string ModelsNs = "MegaCrit.Sts2.Core.Models";

    // The game's data directory: $STS2_GAME_DIR if set, else the path the
    // tool was built against (tools/Directory.Build.props).
    public static string DefaultGameData =>
        Environment.GetEnvironmentVariable("STS2_GAME_DIR") is { Length: > 0 } dir
            ? Path.Combine(dir, "data_sts2_linuxbsd_x86_64")
            : BuildSetting("Sts2GameData");

    // The decompiled src/Core tree: $STS2_SRC, else the build-time path.
    public static string DefaultGameSrc =>
        Environment.GetEnvironmentVariable("STS2_SRC") is { Length: > 0 } src ? src : BuildSetting("Sts2GameSrc");

    private static string BuildSetting(string key) =>
        typeof(GameBootstrap).Assembly.GetCustomAttributes<AssemblyMetadataAttribute>()
            .FirstOrDefault(a => a.Key == key)?.Value
        ?? throw new InvalidOperationException("tool built without " + key + " (tools/Directory.Build.props)");

    // Must run before any game type is touched (i.e. from Main, before calling
    // a method that references sts2 types).
    public static void InstallResolver(string gameData)
    {
        AssemblyLoadContext.Default.Resolving += (ctx, name) =>
        {
            string path = Path.Combine(gameData, name.Name + ".dll");
            return File.Exists(path) ? ctx.LoadFromAssemblyPath(path) : null;
        };
    }

    public static Type[] ModelTypes { get; private set; } = Array.Empty<Type>();

    public static Dictionary<ModelId, AbstractModel> ContentById { get; private set; } = null!;

    public static void Init(bool testMode)
    {
        Headless.Install();
        Assembly game = typeof(AbstractModel).Assembly;
        var all = (IEnumerable<Type>)game.GetType(ModelsNs + ".AbstractModelSubtypes")!
            .GetProperty("All", BindingFlags.Public | BindingFlags.Static)!
            .GetValue(null)!;
        ModelTypes = all.ToArray();
        // ModelDb.AllAbstractModelSubtypes would also scan mods, which needs ModManager.
        typeof(ModelDb).GetField("_allAbstractModelSubtypes", BindingFlags.NonPublic | BindingFlags.Static)!
            .SetValue(null, ModelTypes);
        AssemblyInfo.Init();
        ModelDb.Init();
        ModelIdSerializationCache.Init();
        ModelDb.InitIds();
        ContentById = (Dictionary<ModelId, AbstractModel>)typeof(ModelDb)
            .GetField("_contentById", BindingFlags.NonPublic | BindingFlags.Static)!
            .GetValue(null)!;
        if (testMode)
        {
            Headless.EnableTestMode();
            // Also skips the action executor's pause loop, which polls with
            // real-time delays under TestMode.
            NonInteractiveMode.AutoSlayerCheck = () => true;
        }
    }

    // English localization tables, read from the decompiled project's
    // localization/eng directory instead of res:// (Godot file access is
    // stubbed). Gameplay code checks whether keys exist (event options, power
    // descriptions), so the tables must be the real ones, not placeholders.
    public static void LoadLocalization(string? dir = null)
    {
        dir ??= Path.Combine(DefaultGameSrc, "..", "..", "localization", "eng");
        if (!Directory.Exists(dir)) throw new DirectoryNotFoundException("localization tables not found: " + dir);
        var tables = new Dictionary<string, LocTable>();
        foreach (string file in Directory.GetFiles(dir, "*.json").OrderBy(f => f, StringComparer.Ordinal))
        {
            string name = Path.GetFileNameWithoutExtension(file);
            var data = System.Text.Json.JsonSerializer.Deserialize<Dictionary<string, string>>(File.ReadAllText(file))!;
            tables[name] = new LocTable(name, data);
        }
        const BindingFlags Inst = BindingFlags.Instance | BindingFlags.NonPublic;
        var loc = (LocManager)RuntimeHelpers.GetUninitializedObject(typeof(LocManager));
        typeof(LocManager).GetField("_localeChangeCallbacks", Inst)!.SetValue(loc, new List<LocManager.LocaleChangeCallback>());
        typeof(LocManager).GetField("_languageKeyCount", Inst)!.SetValue(loc, new Dictionary<string, int>());
        typeof(LocManager).GetMethod("SetLanguageInternal", Inst)!
            .Invoke(loc, new object[] { "eng", tables, false, new List<LocValidationError>() });
        typeof(LocManager).GetMethod("LoadLocFormatters", Inst)!.Invoke(loc, null);
        typeof(LocManager).GetProperty(nameof(LocManager.Instance))!.SetValue(null, loc);
    }

    // In-memory save system (the game's own test store): the profile starts
    // empty and nothing touches disk or the platform layer.
    public static void UseInMemorySaves()
    {
        var saves = new SaveManager(new MockGodotFileIo("user://sts2sim"), forceSynchronous: true);
        saves.InitProfileId(1);
        // The game normally loads these two stores during the Godot bootstrap.
        // Turn-end combat code reads PrefsSave.FastMode even in TestMode, so
        // initialize the same in-memory defaults before any run starts.
        saves.InitSettingsDataForTest();
        saves.InitPrefsDataForTest();
        SaveManager.MockInstanceForTesting(saves);
        // A veteran profile has seen every tutorial (they would open UI popups).
        new HarmonyLib.Harmony("sts2sim.headless.profile").Patch(
            HarmonyLib.AccessTools.Method(typeof(SaveManager), nameof(SaveManager.SeenFtue)),
            prefix: new HarmonyLib.HarmonyMethod(typeof(GameBootstrap), nameof(SeenAllFtues)));
    }

    private static bool SeenAllFtues(ref bool __result)
    {
        __result = true;
        return false;
    }
}
