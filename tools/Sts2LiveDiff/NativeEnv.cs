using System.Runtime.InteropServices;

namespace Sts2LiveDiff;

internal sealed class NativeEnv : IDisposable
{
    private const string Library = "libsts2env_c.so";
    private IntPtr _handle;

    [StructLayout(LayoutKind.Sequential)]
    internal struct Action
    {
        public int Kind;
        public int Index;
        public int Target;
        public int Col;
        public int Row;
    }

    internal NativeEnv()
    {
        _handle = Create();
        if (_handle == IntPtr.Zero) throw new InvalidOperationException("sts2_env_create returned null");
    }

    private NativeEnv(IntPtr handle) => _handle = handle;

    internal string ResetCombat(string seed, string character, int ascension, string encounter, int act)
    {
        Check(InspectCombatNamed(_handle, seed, character, ascension, encounter, act));
        return Result();
    }

    internal void ResetRun(string seed, string character, int ascension) =>
        Check(ResetNamed(_handle, seed, character, ascension));

    internal string Inspect()
    {
        Check(InspectCurrent(_handle));
        return Result();
    }

    internal string ExportOracleCombatState()
    {
        Check(ExportOracleCombatStateNative(_handle));
        return Result();
    }

    internal string ExportOracleRunState()
    {
        Check(ExportOracleRunStateNative(_handle));
        return Result();
    }

    internal string ExportOracleRewards()
    {
        Check(ExportOracleRewardsNative(_handle));
        return Result();
    }

    internal string ExportRunQueries()
    {
        Check(ExportRunQueriesNative(_handle, 0));
        return Result();
    }

    internal int EventRngCounter() => EventRngCounterNative(_handle);

    internal NativeEnv Predict(Action action)
    {
        int status = TryTransition(_handle, action, out IntPtr successor);
        if (status != 0)
        {
            if (successor != IntPtr.Zero) Destroy(successor);
            Check(status);
        }
        if (successor == IntPtr.Zero) throw new InvalidOperationException("Env transition returned no successor");
        return new NativeEnv(successor);
    }

    private string Result() => Marshal.PtrToStringUTF8(LastResult(_handle))
        ?? throw new InvalidOperationException("Env returned no JSON");

    private void Check(int status)
    {
        if (status == 0) return;
        string reason = Marshal.PtrToStringUTF8(LastError(_handle)) ?? "unknown";
        throw new InvalidOperationException($"Env status={status}: {reason}");
    }

    public void Dispose()
    {
        IntPtr handle = Interlocked.Exchange(ref _handle, IntPtr.Zero);
        if (handle != IntPtr.Zero) Destroy(handle);
        GC.SuppressFinalize(this);
    }

    ~NativeEnv() => Dispose();

    [DllImport(Library, EntryPoint = "sts2_env_create", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr Create();
    [DllImport(Library, EntryPoint = "sts2_env_destroy", CallingConvention = CallingConvention.Cdecl)]
    private static extern void Destroy(IntPtr handle);
    [DllImport(Library, EntryPoint = "sts2_env_inspect_combat_named", CallingConvention = CallingConvention.Cdecl)]
    private static extern int InspectCombatNamed(IntPtr handle, [MarshalAs(UnmanagedType.LPUTF8Str)] string seed,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string character, int ascension,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string encounter, int act);
    [DllImport(Library, EntryPoint = "sts2_env_reset_named", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ResetNamed(IntPtr handle, [MarshalAs(UnmanagedType.LPUTF8Str)] string seed,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string character, int ascension);
    [DllImport(Library, EntryPoint = "sts2_env_inspect_current", CallingConvention = CallingConvention.Cdecl)]
    private static extern int InspectCurrent(IntPtr handle);
    [DllImport(Library, EntryPoint = "sts2_env_export_oracle_combat_state", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ExportOracleCombatStateNative(IntPtr handle);
    [DllImport(Library, EntryPoint = "sts2_env_export_oracle_run_state", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ExportOracleRunStateNative(IntPtr handle);
    [DllImport(Library, EntryPoint = "sts2_env_export_oracle_rewards", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ExportOracleRewardsNative(IntPtr handle);
    [DllImport(Library, EntryPoint = "sts2_env_export_run_queries", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ExportRunQueriesNative(IntPtr handle, int flags);
    [DllImport(Library, EntryPoint = "sts2_env_event_rng_counter", CallingConvention = CallingConvention.Cdecl)]
    private static extern int EventRngCounterNative(IntPtr handle);
    [DllImport(Library, EntryPoint = "sts2_env_try_transition", CallingConvention = CallingConvention.Cdecl)]
    private static extern int TryTransition(IntPtr handle, Action action, out IntPtr successor);
    [DllImport(Library, EntryPoint = "sts2_env_last_result", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr LastResult(IntPtr handle);
    [DllImport(Library, EntryPoint = "sts2_env_last_error", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr LastError(IntPtr handle);
}
