using System.Text.Json;
using Godot;

namespace CombatSolver;

internal enum EnvBackendMode
{
    Native,
    Env,
    Both,
}

// Settings are separate from the upstream SolverSettings file so the backend
// selector survives upstream settings migrations.
internal static class EnvBackendSettings
{
    private const string Uri = "user://combatsolver_env_backend.json";
    internal const string EnvExecutionUnavailableReason =
        "live_root_and_same_policy_adapter_incomplete";
    private static readonly object Sync = new();
    private static bool _loaded;
    private static EnvBackendMode _mode = EnvBackendMode.Both;
    private static bool _compare = true;

    internal static void Initialize()
    {
        EnsureLoaded();
        Sts2EnvCompare.Entry.BackendModeProvider = CurrentCompareMode;
        // The reference Beam/structural seed root cannot execute as the final
        // Env backend. Keep Native active until the exact live root and the
        // upstream policy adapter have both passed their acceptance gates.
        Sts2EnvCompare.Entry.EnvExecutionGate = (_, _) => EnvExecutionUnavailableReason;
        Sts2EnvCompare.Entry.EnvLiveSnapshotProvider = EnvRootCapture.CaptureLiveSnapshot;
    }

    internal static EnvBackendMode Mode { get { EnsureLoaded(); lock (Sync) return _mode; } }
    internal static bool Compare { get { EnsureLoaded(); lock (Sync) return _compare; } }

    internal static void Update(EnvBackendMode mode, bool compare)
    {
        EnsureLoaded();
        lock (Sync)
        {
            _mode = mode;
            _compare = compare;
            bool persisted = SaveLocked();
            Entry.Logger.Info($"[CombatSolverEnv] BACKEND_SETTINGS mode={_mode} compare={_compare} persisted={persisted.ToString().ToLowerInvariant()}");
        }
    }

    private static void EnsureLoaded()
    {
        lock (Sync)
        {
            if (_loaded) return;
            _loaded = true;
            string path = ProjectSettings.GlobalizePath(Uri);
            if (!File.Exists(path))
            {
                // Keep headless fixtures and first launch deterministic while
                // still letting the UI persist the choice afterward.
                string startup = System.Environment.GetEnvironmentVariable("STS2ENV_COMPARE_MODE")?.ToLowerInvariant() ?? "";
                _mode = startup switch
                {
                    "native" => EnvBackendMode.Native,
                    "env" => EnvBackendMode.Env,
                    _ => EnvBackendMode.Both,
                };
                return;
            }
            try
            {
                Persisted? value = JsonSerializer.Deserialize<Persisted>(File.ReadAllText(path));
                if (value is not null && Enum.IsDefined(value.Mode))
                {
                    _mode = value.Mode;
                    _compare = value.Compare;
                }
            }
            catch (Exception error)
            {
                Entry.Logger.Warn($"[CombatSolverEnv] BACKEND_SETTINGS_LOAD_FAILED reason={error.GetType().Name}");
            }
        }
    }

    private static bool SaveLocked()
    {
        string path = ProjectSettings.GlobalizePath(Uri);
        string temporary = path + ".tmp";
        try
        {
            File.WriteAllText(temporary, JsonSerializer.Serialize(new Persisted(_mode, _compare), new JsonSerializerOptions { WriteIndented = true }));
            File.Move(temporary, path, true);
            return true;
        }
        catch (Exception error)
        {
            try { if (File.Exists(temporary)) File.Delete(temporary); } catch { }
            Entry.Logger.Warn($"[CombatSolverEnv] BACKEND_SETTINGS_SAVE_FAILED reason={error.GetType().Name}");
            return false;
        }
    }

    private static string CurrentCompareMode()
    {
        EnvBackendMode mode = Mode;
        return mode switch
        {
            EnvBackendMode.Native => "native",
            EnvBackendMode.Env => "env",
            EnvBackendMode.Both when Compare => "both",
            _ => "native",
        };
    }

    private sealed record Persisted(EnvBackendMode Mode, bool Compare);
}
