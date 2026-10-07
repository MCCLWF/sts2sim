using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using CombatSolverEnv.Api;

namespace Sts2EnvCompare;

// Dedicated search prediction handles. Never pass these to NativeEnv's run
// API: this scope preserves terminal combat state before post-combat effects.
internal static class NativeCombatPrediction
{
    private const string Library = "sts2env_c";
    [StructLayout(LayoutKind.Sequential)]
    private struct CAction { public int Kind, Index, Target, Col, Row; }

    internal static Session FromGameSnapshot(string json) => Import(json, gameSnapshot: true);
    internal static Session Restore(string json) => Import(json, gameSnapshot: false);

    private static Session Import(string json, bool gameSnapshot)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(json);
        byte[] payload = Encoding.UTF8.GetBytes(json);
        if (payload.Length > 4 * 1024 * 1024) throw new InvalidOperationException("Combat prediction snapshot exceeds 4 MiB");
        NativeEnv.Configure();
        IntPtr diagnostic = Create();
        if (diagnostic == IntPtr.Zero) throw new InvalidOperationException("Combat prediction diagnostic allocation failed");
        IntPtr owned = IntPtr.Zero;
        try
        {
            int status = gameSnapshot
                ? FromSnapshot(diagnostic, payload, (nuint)payload.Length, out owned)
                : ImportSnapshot(diagnostic, payload, (nuint)payload.Length, out owned);
            Check(diagnostic, status);
            if (owned == IntPtr.Zero) throw new InvalidOperationException("Combat prediction import returned no successor");
            Session result = new(owned);
            owned = IntPtr.Zero;
            return result;
        }
        finally { if (owned != IntPtr.Zero) Destroy(owned); Destroy(diagnostic); }
    }

    private static void Check(IntPtr handle, int status)
    {
        if (status == 0) return;
        string reason = Marshal.PtrToStringUTF8(LastError(handle)) ?? "Combat prediction operation failed";
        if (status == 1) throw new EnvPredictionUnportedException(reason);
        throw new InvalidOperationException(reason);
    }

    internal sealed class Session(IntPtr handle) : IDisposable
    {
        private IntPtr _handle = handle;
        private readonly object _gate = new();
        private IntPtr RequireHandle() => _handle != IntPtr.Zero ? _handle : throw new ObjectDisposedException(nameof(Session));

        internal ulong Hash
        {
            get
            {
                lock (_gate)
                {
                    IntPtr source = RequireHandle();
                    Check(source, HashNative(source, out ulong result));
                    return result;
                }
            }
        }

        internal string ExportSnapshotJson()
        {
            lock (_gate)
            {
                IntPtr source = RequireHandle();
                Check(source, ExportSnapshot(source));
                return ReadResult(source);
            }
        }

        internal string InspectJson()
        {
            lock (_gate)
            {
                IntPtr source = RequireHandle();
                Check(source, InspectCurrent(source));
                return ReadResult(source);
            }
        }

        internal string ExportQueriesJson()
        {
            lock (_gate)
            {
                IntPtr source = RequireHandle();
                Check(source, ExportQueries(source));
                return ReadResult(source);
            }
        }

        internal string ExportCalculatedVarsJson()
        {
            lock (_gate)
            {
                IntPtr source = RequireHandle();
                Check(source, ExportCalculatedVars(source));
                return ReadResult(source);
            }
        }

        internal string ExportOrbsJson()
        {
            lock (_gate)
            {
                IntPtr source = RequireHandle();
                Check(source, ExportOrbs(source));
                return ReadResult(source);
            }
        }

        internal string ExportPowerQueriesJson()
        {
            lock (_gate)
            {
                IntPtr source = RequireHandle();
                Check(source, ExportPowerQueries(source));
                return ReadResult(source);
            }
        }

        internal IReadOnlyList<EnvCombatAction> LegalActions()
        {
            lock (_gate)
            {
                IntPtr source = RequireHandle();
                Check(source, LegalActionsNative(source));
                using JsonDocument document = JsonDocument.Parse(ReadResult(source));
                JsonElement root = document.RootElement;
                if (root.GetProperty("format").GetString() != "sts2env.combat_prediction_actions"
                    || root.GetProperty("version").GetInt32() != 1)
                    throw new InvalidOperationException("Combat prediction actions protocol mismatch");
                EnvCombatAction[] actions = root.GetProperty("actions").EnumerateArray().Select(item =>
                {
                    EnvCombatActionKind kind = (EnvCombatActionKind)item.GetProperty("kind").GetInt32();
                    if (!Enum.IsDefined(kind)) throw new InvalidOperationException("Combat prediction returned an unknown action");
                    return new EnvCombatAction(kind, item.GetProperty("index").GetInt32(), item.GetProperty("target").GetInt32());
                }).ToArray();
                return Array.AsReadOnly(actions);
            }
        }

        internal Session Clone()
        {
            lock (_gate)
            {
                IntPtr source = RequireHandle();
                int status = CloneNative(source, out IntPtr result);
                return Own(source, status, result);
            }
        }

        internal Session Predict(EnvCombatAction action)
        {
            lock (_gate)
            {
                IntPtr source = RequireHandle();
                CAction selected = new() { Kind = (int)action.Kind, Index = action.Index, Target = action.TargetCombatId };
                int status = PredictNative(source, selected, out IntPtr result);
                return Own(source, status, result);
            }
        }

        private static Session Own(IntPtr source, int status, IntPtr result)
        {
            try
            {
                Check(source, status);
                if (result == IntPtr.Zero) throw new InvalidOperationException("Combat prediction returned no owned successor");
                return new(result);
            }
            catch { if (result != IntPtr.Zero) Destroy(result); throw; }
        }

        private static string ReadResult(IntPtr source) => Marshal.PtrToStringUTF8(LastResult(source))
            ?? throw new InvalidOperationException("Combat prediction returned an empty document");

        public void Dispose()
        {
            lock (_gate)
            {
                IntPtr source = Interlocked.Exchange(ref _handle, IntPtr.Zero);
                if (source != IntPtr.Zero) Destroy(source);
            }
            GC.SuppressFinalize(this);
        }
        ~Session() => Dispose();
    }

    [DllImport(Library, EntryPoint = "sts2_combat_prediction_create", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr Create();
    [DllImport(Library, EntryPoint = "sts2_combat_prediction_destroy", CallingConvention = CallingConvention.Cdecl)]
    private static extern void Destroy(IntPtr handle);
    [DllImport(Library, EntryPoint = "sts2_combat_prediction_from_game_snapshot", CallingConvention = CallingConvention.Cdecl)]
    private static extern int FromSnapshot(IntPtr diagnostic, byte[] data, nuint length, out IntPtr successor);
    [DllImport(Library, EntryPoint = "sts2_combat_prediction_import_snapshot", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ImportSnapshot(IntPtr diagnostic, byte[] data, nuint length, out IntPtr successor);
    [DllImport(Library, EntryPoint = "sts2_combat_prediction_clone", CallingConvention = CallingConvention.Cdecl)]
    private static extern int CloneNative(IntPtr source, out IntPtr successor);
    [DllImport(Library, EntryPoint = "sts2_combat_prediction_predict", CallingConvention = CallingConvention.Cdecl)]
    private static extern int PredictNative(IntPtr source, CAction action, out IntPtr successor);
    [DllImport(Library, EntryPoint = "sts2_combat_prediction_export_snapshot", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ExportSnapshot(IntPtr source);
    [DllImport(Library, EntryPoint = "sts2_combat_prediction_inspect_current", CallingConvention = CallingConvention.Cdecl)]
    private static extern int InspectCurrent(IntPtr source);
    [DllImport(Library, EntryPoint = "sts2_combat_prediction_export_queries", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ExportQueries(IntPtr source);
    [DllImport(Library, EntryPoint = "sts2_combat_prediction_export_calculated_vars", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ExportCalculatedVars(IntPtr source);
    [DllImport(Library, EntryPoint = "sts2_combat_prediction_export_orbs", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ExportOrbs(IntPtr source);
    [DllImport(Library, EntryPoint = "sts2_combat_prediction_export_power_queries", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ExportPowerQueries(IntPtr source);
    [DllImport(Library, EntryPoint = "sts2_combat_prediction_legal_actions", CallingConvention = CallingConvention.Cdecl)]
    private static extern int LegalActionsNative(IntPtr source);
    [DllImport(Library, EntryPoint = "sts2_combat_prediction_hash", CallingConvention = CallingConvention.Cdecl)]
    private static extern int HashNative(IntPtr source, out ulong hash);
    [DllImport(Library, EntryPoint = "sts2_combat_prediction_last_result", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr LastResult(IntPtr source);
    [DllImport(Library, EntryPoint = "sts2_combat_prediction_last_error", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr LastError(IntPtr source);
}
