using System.Runtime.InteropServices;
using System.Text.Json;
using Microsoft.Win32.SafeHandles;

namespace Sts2Env;

/// <summary>Loads and owns one versioned sts2sim native ABI module.</summary>
public sealed class Sts2EnvLibrary : IDisposable
{
    public const uint RequiredAbiVersion = 1;
    public const uint RequiredProtocolVersion = 2;
    // Minimum padding lengths; actual buffers are discovered per decision.
    public const int ActionMaskSize = 512;
    public const int ObservationSize = 2048;
    private const int StatusOk = 0;
    private const int StatusUnported = 1;
    private const int StatusTraceLimit = 2;
    private const int StatusUnsupportedOperation = 3;
    private const int StatusOperationCapacity = 4;
    private const int StatusInvalidOperationRequest = 5;
    private const int StatusError = -1;

    private readonly object _lifetimeGate = new();
    private IntPtr _module;
    private int _ownedHandleCount;
    private bool _disposeRequested;

    private readonly CreateCall _create;
    private readonly CloneCall _clone;
    private readonly DestroyCall _destroy;
    private readonly ResetCall _reset;
    private readonly ResetNamedCall _resetNamed;
    private readonly ResetCombatCall _resetCombat;
    private readonly InspectCombatNamedCall _inspectCombatNamed;
    private readonly SimpleStatusCall _inspectCurrent;
    private readonly SimpleStatusCall _exportOracleCombat;
    private readonly SimpleStatusCall _exportOracleRun;
    private readonly SimpleStatusCall _exportOracleRewards;
    private readonly EventRngCounterCall _eventRngCounter;
    private readonly SimpleStatusCall _exportSnapshot;
    private readonly ImportSnapshotCall _importSnapshot;
    private readonly SimpleStatusCall _exportPortableSnapshot;
    private readonly SimpleStatusCall _validateCombatRoot;
    private readonly SuccessorCall _tryTransition;
    private readonly PreviewCall _preview;
    private readonly ExportRunQueriesCall _exportRunQueries;
    private readonly SimpleStatusCall _exportCombatQueries;
    private readonly PreviewWithResultsCall _previewWithResults;
    private readonly PredictOperationCall _predictOperation;
    private readonly SimpleStatusCall _exportCalculatedVars;
    private readonly SimpleStatusCall _exportOrbs;
    private readonly SimpleStatusCall _exportPowers;
    private readonly EnvStringCall _lastResult;
    private readonly DecisionCall _decision;
    private readonly HashCall _hash;
    private readonly DeterminizeCall _determinize;
    private readonly StepCall _step;
    private readonly BufferCountCall _legalActions;
    private readonly BufferCountCall _actionMask;
    private readonly BufferCountCall _observe;
    private readonly EnvStringCall _lastError;

    private Sts2EnvLibrary(string nativeLibraryPath)
    {
        NativeLibraryPath = Path.GetFullPath(nativeLibraryPath);
        _module = NativeLibrary.Load(NativeLibraryPath);
        try
        {
            // The two discovery exports are the only symbols read before the
            // ABI and descriptor protocol have both been validated.
            var abiVersion = Bind<AbiVersionCall>("sts2_env_abi_version");
            var apiDescriptor = Bind<LastStringCall>("sts2_env_api_descriptor");
            AbiVersion = abiVersion();
            if (AbiVersion != RequiredAbiVersion)
                throw new AbiVersionMismatchException(RequiredAbiVersion, AbiVersion);

            IntPtr descriptorPointer = apiDescriptor();
            if (descriptorPointer == IntPtr.Zero)
                throw new ApiDescriptorException("Native API descriptor was null.");
            ApiDescriptorJson = Marshal.PtrToStringUTF8(descriptorPointer)
                ?? throw new ApiDescriptorException("Native API descriptor was not valid UTF-8.");
            try
            {
                using JsonDocument document = JsonDocument.Parse(ApiDescriptorJson);
                if (document.RootElement.ValueKind != JsonValueKind.Object ||
                    !document.RootElement.TryGetProperty("protocol_version", out JsonElement protocol) ||
                    !protocol.TryGetUInt32(out uint protocolVersion))
                    throw new ApiDescriptorException("Native API descriptor is missing an integer protocol_version.");
                ProtocolVersion = protocolVersion;
                if (ProtocolVersion != RequiredProtocolVersion)
                    throw new ApiDescriptorException($"Unsupported API descriptor protocol {ProtocolVersion}; this SDK requires {RequiredProtocolVersion}.");
                ApiDescriptor = document.RootElement.Clone();
                if (!DescriptorAdvertises(ApiDescriptor, "sts2_env_predict_operation"))
                    throw new ApiDescriptorException("Native API descriptor does not advertise sts2_env_predict_operation.");
            }
            catch (JsonException error)
            {
                throw new ApiDescriptorException("Native API descriptor is not valid JSON.", error);
            }

            _create = Bind<CreateCall>("sts2_env_create");
            _clone = Bind<CloneCall>("sts2_env_clone");
            _destroy = Bind<DestroyCall>("sts2_env_destroy");
            _reset = Bind<ResetCall>("sts2_env_reset");
            _resetNamed = Bind<ResetNamedCall>("sts2_env_reset_named");
            _resetCombat = Bind<ResetCombatCall>("sts2_env_reset_combat");
            _inspectCombatNamed = Bind<InspectCombatNamedCall>("sts2_env_inspect_combat_named");
            _inspectCurrent = Bind<SimpleStatusCall>("sts2_env_inspect_current");
            _exportOracleCombat = Bind<SimpleStatusCall>("sts2_env_export_oracle_combat_state");
            _exportOracleRun = Bind<SimpleStatusCall>("sts2_env_export_oracle_run_state");
            _exportOracleRewards = Bind<SimpleStatusCall>("sts2_env_export_oracle_rewards");
            _eventRngCounter = Bind<EventRngCounterCall>("sts2_env_event_rng_counter");
            _exportSnapshot = Bind<SimpleStatusCall>("sts2_env_export_snapshot");
            _exportPortableSnapshot = Bind<SimpleStatusCall>("sts2_env_export_portable_snapshot");
            _importSnapshot = Bind<ImportSnapshotCall>("sts2_env_import_snapshot");
            _validateCombatRoot = Bind<SimpleStatusCall>("sts2_env_validate_combat_root");
            _tryTransition = Bind<SuccessorCall>("sts2_env_try_transition");
            _preview = Bind<PreviewCall>("sts2_env_preview");
            _exportRunQueries = Bind<ExportRunQueriesCall>("sts2_env_export_run_queries");
            _exportCombatQueries = Bind<SimpleStatusCall>("sts2_env_export_combat_queries");
            _previewWithResults = Bind<PreviewWithResultsCall>("sts2_env_preview_with_results");
            _predictOperation = Bind<PredictOperationCall>("sts2_env_predict_operation");
            _exportCalculatedVars = Bind<SimpleStatusCall>("sts2_env_export_calculated_vars");
            _exportOrbs = Bind<SimpleStatusCall>("sts2_env_export_orbs");
            _exportPowers = Bind<SimpleStatusCall>("sts2_env_export_powers");
            _lastResult = Bind<EnvStringCall>("sts2_env_last_result");
            _decision = Bind<DecisionCall>("sts2_env_decision");
            _hash = Bind<HashCall>("sts2_env_hash");
            _determinize = Bind<DeterminizeCall>("sts2_env_determinize");
            _step = Bind<StepCall>("sts2_env_step");
            _legalActions = Bind<BufferCountCall>("sts2_env_legal_actions");
            _actionMask = Bind<BufferCountCall>("sts2_env_action_mask");
            _observe = Bind<BufferCountCall>("sts2_env_observe");
            _lastError = Bind<EnvStringCall>("sts2_env_last_error");
        }
        catch
        {
            NativeLibrary.Free(_module);
            _module = IntPtr.Zero;
            throw;
        }
    }

    public string NativeLibraryPath { get; }
    public uint AbiVersion { get; }
    public uint ProtocolVersion { get; }
    public string ApiDescriptorJson { get; }
    public JsonElement ApiDescriptor { get; }

    public static Sts2EnvLibrary Load(string nativeLibraryPath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(nativeLibraryPath);
        return new Sts2EnvLibrary(nativeLibraryPath);
    }

    public Sts2Environment CreateEnvironment()
    {
        lock (_lifetimeGate)
        {
            ObjectDisposedException.ThrowIf(_disposeRequested, this);
            IntPtr raw = _create();
            return OwnHandle(raw, "sts2_env_create");
        }
    }

    public Sts2Environment RestoreSnapshot(Sts2EnvSnapshot snapshot)
    {
        ArgumentNullException.ThrowIfNull(snapshot);
        if (!ReferenceEquals(snapshot.Owner, this)) throw new SnapshotSessionMismatchException();
        using Sts2Environment diagnostic = CreateEnvironment();
        return diagnostic.RestoreSnapshot(snapshot);
    }

    public void Dispose()
    {
        lock (_lifetimeGate)
        {
            if (_disposeRequested) return;
            _disposeRequested = true;
            TryUnloadModule();
        }
        GC.SuppressFinalize(this);
    }

    ~Sts2EnvLibrary()
    {
        lock (_lifetimeGate)
        {
            _disposeRequested = true;
            TryUnloadModule();
        }
    }

    private T Bind<T>(string symbol) where T : Delegate =>
        Marshal.GetDelegateForFunctionPointer<T>(NativeLibrary.GetExport(_module, symbol));

    private static bool DescriptorAdvertises(JsonElement descriptor, string symbol)
    {
        if (descriptor.ValueKind != JsonValueKind.Object ||
            !descriptor.TryGetProperty("exports", out JsonElement exports) ||
            exports.ValueKind != JsonValueKind.Object ||
            !exports.TryGetProperty("symbols", out JsonElement symbols) ||
            symbols.ValueKind != JsonValueKind.Array)
            return false;
        return symbols.EnumerateArray().Any(entry => entry.ValueKind == JsonValueKind.String && entry.GetString() == symbol);
    }

    private Sts2Environment OwnHandle(IntPtr raw, string operation)
    {
        if (raw == IntPtr.Zero) throw new NativeEnvironmentException(operation, StatusError, "native handle was null");
        return new Sts2Environment(this, OwnRawHandle(raw, operation));
    }

    private void OnHandleReleased(IntPtr raw)
    {
        lock (_lifetimeGate)
        {
            try { if (_module != IntPtr.Zero) _destroy(raw); }
            finally
            {
                _ownedHandleCount--;
                TryUnloadModule();
            }
        }
    }

    private void TryUnloadModule()
    {
        if (_disposeRequested && _ownedHandleCount == 0 && _module != IntPtr.Zero)
        {
            NativeLibrary.Free(_module);
            _module = IntPtr.Zero;
        }
    }

    private string LastError(IntPtr handle) => ReadUtf8(_lastError(handle));
    private string LastResult(IntPtr handle) => ReadUtf8(_lastResult(handle));
    private static string ReadUtf8(IntPtr pointer) => pointer == IntPtr.Zero ? string.Empty : Marshal.PtrToStringUTF8(pointer) ?? string.Empty;

    private void CheckStatus(IntPtr handle, int status, string operation)
    {
        if (status == StatusOk) return;
        string detail = LastError(handle);
        switch (status)
        {
            case StatusUnported: throw new UnportedContentException(operation, detail);
            case StatusTraceLimit: throw new TraceLimitExceededException(operation, detail);
            case StatusUnsupportedOperation: throw new UnsupportedOperationException(operation, detail);
            case StatusOperationCapacity: throw new OperationCapacityExceededException(operation, detail);
            case StatusInvalidOperationRequest: throw new InvalidOperationRequestException(operation, detail);
            case StatusError: throw new NativeEnvironmentException(operation, status, detail);
            default: throw new NativeEnvironmentException(operation, status, detail.Length == 0 ? "unknown status" : detail);
        }
    }

    internal OwnedEnvHandle CloneHandle(IntPtr source)
    {
        IntPtr raw = _clone(source);
        if (raw == IntPtr.Zero) throw new NativeEnvironmentException("sts2_env_clone", StatusError, LastError(source));
        return OwnRawHandle(raw, "sts2_env_clone");
    }

    internal string InspectCombatNamed(IntPtr handle, string seed, string character, int ascension, string encounter, int actIndex)
    {
        CheckStatus(handle, _inspectCombatNamed(handle, seed, character, ascension, encounter, actIndex), "sts2_env_inspect_combat_named");
        return LastResult(handle);
    }

    internal string InspectCurrent(IntPtr handle) => ReadDiagnostic(handle, _inspectCurrent, "sts2_env_inspect_current");
    internal string ExportOracleCombatState(IntPtr handle) => ReadDiagnostic(handle, _exportOracleCombat, "sts2_env_export_oracle_combat_state");
    internal string ExportOracleRunState(IntPtr handle) => ReadDiagnostic(handle, _exportOracleRun, "sts2_env_export_oracle_run_state");
    internal string ExportOracleRewards(IntPtr handle) => ReadDiagnostic(handle, _exportOracleRewards, "sts2_env_export_oracle_rewards");
    internal string ExportCombatQueries(IntPtr handle) => ReadDiagnostic(handle, _exportCombatQueries, "sts2_env_export_combat_queries");
    internal string ExportCalculatedVars(IntPtr handle) => ReadDiagnostic(handle, _exportCalculatedVars, "sts2_env_export_calculated_vars");
    internal string ExportOrbs(IntPtr handle) => ReadDiagnostic(handle, _exportOrbs, "sts2_env_export_orbs");
    internal string ExportPowers(IntPtr handle) => ReadDiagnostic(handle, _exportPowers, "sts2_env_export_powers");

    private string ReadDiagnostic(IntPtr handle, SimpleStatusCall call, string operation)
    {
        CheckStatus(handle, call(handle), operation);
        return LastResult(handle);
    }

    internal void Reset(IntPtr handle, string seed, int character, int ascension) =>
        CheckStatus(handle, _reset(handle, seed, character, ascension), "sts2_env_reset");
    internal void ResetNamed(IntPtr handle, string seed, string character, int ascension) =>
        CheckStatus(handle, _resetNamed(handle, seed, character, ascension), "sts2_env_reset_named");
    internal void ResetCombat(IntPtr handle, string seed, int character, int ascension, int encounter, int actIndex) =>
        CheckStatus(handle, _resetCombat(handle, seed, character, ascension, encounter, actIndex), "sts2_env_reset_combat");
    internal void ValidateCombatRoot(IntPtr handle) =>
        CheckStatus(handle, _validateCombatRoot(handle), "sts2_env_validate_combat_root");
    internal void Determinize(IntPtr handle, ulong seed) =>
        CheckStatus(handle, _determinize(handle, seed), "sts2_env_determinize");
    internal void Step(IntPtr handle, Sts2Action action) =>
        CheckStatus(handle, _step(handle, ToNative(action)), "sts2_env_step");

    internal int? EventRngCounter(IntPtr handle)
    {
        int value = _eventRngCounter(handle);
        return value < 0 ? null : value;
    }

    internal Sts2Decision Decision(IntPtr handle)
    {
        int raw = _decision(handle);
        if (raw < 0)
        {
            string error = LastError(handle);
            if (error.Length != 0) throw new NativeEnvironmentException("sts2_env_decision", StatusError, error);
        }
        return new Sts2Decision(raw);
    }

    internal ulong Hash(IntPtr handle)
    {
        ulong value = _hash(handle);
        string error = value == 0 ? LastError(handle) : string.Empty;
        if (error.Length != 0) throw new NativeEnvironmentException("sts2_env_hash", StatusError, error);
        return value;
    }

    internal int[] Observe(IntPtr handle) => ReadIntegerBuffer(handle, _observe, "sts2_env_observe");

    internal byte[] ActionMask(IntPtr handle)
    {
        int capacity = _actionMask(handle, IntPtr.Zero, 0);
        CheckCount(handle, capacity, "sts2_env_action_mask");
        IntPtr buffer = Marshal.AllocHGlobal(capacity);
        try
        {
            int count = _actionMask(handle, buffer, (nuint)capacity);
            CheckCount(handle, count, "sts2_env_action_mask");
            byte[] result = new byte[count];
            Marshal.Copy(buffer, result, 0, count);
            return result;
        }
        finally { Marshal.FreeHGlobal(buffer); }
    }

    internal Sts2Action[] LegalActions(IntPtr handle)
    {
        int size = Marshal.SizeOf<NativeAction>();
        int capacity = _legalActions(handle, IntPtr.Zero, 0);
        CheckCount(handle, capacity, "sts2_env_legal_actions");
        IntPtr buffer = Marshal.AllocHGlobal(checked(size * capacity));
        try
        {
            int count = _legalActions(handle, buffer, (nuint)capacity);
            CheckCount(handle, count, "sts2_env_legal_actions");
            var actions = new Sts2Action[count];
            for (int i = 0; i < count; i++)
                actions[i] = Marshal.PtrToStructure<NativeAction>(IntPtr.Add(buffer, i * size)).ToManaged();
            return actions;
        }
        finally { Marshal.FreeHGlobal(buffer); }
    }

    private int[] ReadIntegerBuffer(IntPtr handle, BufferCountCall call, string operation)
    {
        int capacity = call(handle, IntPtr.Zero, 0);
        CheckCount(handle, capacity, operation);
        IntPtr buffer = Marshal.AllocHGlobal(checked(sizeof(int) * capacity));
        try
        {
            int count = call(handle, buffer, (nuint)capacity);
            CheckCount(handle, count, operation);
            int[] values = new int[count];
            Marshal.Copy(buffer, values, 0, count);
            return values;
        }
        finally { Marshal.FreeHGlobal(buffer); }
    }

    private void CheckCount(IntPtr handle, int count, string operation)
    {
        if (count >= 0) return;
        throw new NativeEnvironmentException(operation, StatusError, LastError(handle));
    }

    internal string ExportRunQueries(IntPtr handle, bool includeFuture)
    {
        CheckStatus(handle, _exportRunQueries(handle, includeFuture ? 1 : 0), "sts2_env_export_run_queries");
        return LastResult(handle);
    }

    internal Sts2Environment TryTransition(IntPtr handle, Sts2Action action)
    {
        IntPtr successor = IntPtr.Zero;
        int status = _tryTransition(handle, ToNative(action), out successor);
        return CompleteSuccessor(handle, status, successor, "sts2_env_try_transition");
    }

    internal Sts2EnvPreview Preview(IntPtr handle, Sts2Action action, int traceLimit, bool withResults, bool includeFuture, bool includeRng = false, bool includeCausal = false)
    {
        if (traceLimit < 0 || traceLimit > 1_000_000) throw new ArgumentOutOfRangeException(nameof(traceLimit));
        if((includeRng || includeCausal) && (!withResults || traceLimit==0)) throw new ArgumentException("Privileged diagnostics require an enabled trace with results");
        IntPtr successor = IntPtr.Zero;
        int status = withResults
            ? _previewWithResults(handle, ToNative(action), traceLimit, (includeFuture ? 1 : 0) | (includeRng ? 2 : 0) | (includeCausal ? 4 : 0), out successor)
            : _preview(handle, ToNative(action), traceLimit, out successor);
        Sts2Environment branch = CompleteSuccessor(handle, status, successor,
            withResults ? "sts2_env_preview_with_results" : "sts2_env_preview");
        try
        {
            // The native diagnostic is borrowed from the new handle, so copy it
            // before returning control to code that may call another ABI entry.
            return new Sts2EnvPreview(branch, branch.ReadLastResult());
        }
        catch
        {
            branch.Dispose();
            throw;
        }
    }

    internal Sts2EnvPreview PredictOperation(IntPtr handle, Sts2Operation operation, int traceLimit, int queryFlags)
    {
        if (traceLimit < 0 || traceLimit > 1_000_000) throw new ArgumentOutOfRangeException(nameof(traceLimit));
        if (queryFlags<0 || (queryFlags & ~7)!=0) throw new ArgumentOutOfRangeException(nameof(queryFlags));
        if((queryFlags & 6)!=0 && traceLimit==0) throw new ArgumentException("Privileged diagnostics require an enabled trace");

        // The Env lock spans hash validation and execution. Instance refs
        // retain their original query hash instead of being rebound here.
        ulong sourceHash = Hash(handle);
        if (operation.RequiresSourceBinding && operation.ExpectedSourceHash is null)
            throw new InvalidOperationRequestException("sts2_env_predict_operation", "state references require the source hash from their facts query");
        if (operation.ExpectedSourceHash is ulong expected && expected != sourceHash)
            throw new InvalidOperationRequestException("sts2_env_predict_operation", "state references belong to an earlier or different source state");
        byte[] request = Sts2OperationJson.Serialize(operation, sourceHash);
        IntPtr successor = IntPtr.Zero;
        int status = _predictOperation(handle, request, (nuint)request.Length, traceLimit, queryFlags, out successor);
        Sts2Environment branch = CompleteSuccessor(handle, status, successor, "sts2_env_predict_operation");
        try
        {
            return new Sts2EnvPreview(branch, branch.ReadLastResult());
        }
        catch
        {
            branch.Dispose();
            throw;
        }
    }

    internal Sts2EnvSnapshot CaptureSnapshot(IntPtr handle)
    {
        string json = ReadDiagnostic(handle, _exportSnapshot, "sts2_env_export_snapshot");
        return new Sts2EnvSnapshot(json, this);
    }

    internal Sts2EnvPortableSnapshot CapturePortableSnapshot(IntPtr handle) =>
        new(ReadDiagnostic(handle, _exportPortableSnapshot, "sts2_env_export_portable_snapshot"));

    internal Sts2Environment ImportSnapshot(IntPtr diagnosticHandle, string json)
    {
        byte[] utf8 = System.Text.Encoding.UTF8.GetBytes(json);
        IntPtr successor = IntPtr.Zero;
        int status = _importSnapshot(diagnosticHandle, utf8, (nuint)utf8.Length, out successor);
        return CompleteSuccessor(diagnosticHandle, status, successor, "sts2_env_import_snapshot");
    }

    private Sts2Environment CompleteSuccessor(IntPtr source, int status, IntPtr successor, string operation)
    {
        if (status != StatusOk)
        {
            if (successor != IntPtr.Zero) _destroy(successor);
            CheckStatus(source, status, operation);
        }
        if (successor == IntPtr.Zero)
            throw new NativeEnvironmentException(operation, StatusError, "native returned success without an owned successor");
        return OwnHandle(successor, operation);
    }

    private OwnedEnvHandle OwnRawHandle(IntPtr raw, string operation)
    {
        if (raw == IntPtr.Zero) throw new NativeEnvironmentException(operation, StatusError, "native handle was null");
        lock (_lifetimeGate)
        {
            if (_module == IntPtr.Zero)
            {
                _destroy(raw);
                throw new ObjectDisposedException(nameof(Sts2EnvLibrary));
            }
            _ownedHandleCount++;
            try { return new OwnedEnvHandle(this, raw); }
            catch
            {
                _ownedHandleCount--;
                _destroy(raw);
                TryUnloadModule();
                throw;
            }
        }
    }

    internal void ThrowStatus(IntPtr handle, int status, string operation) => CheckStatus(handle, status, operation);

    internal string ReadLastResult(IntPtr handle) => LastResult(handle);
    internal string ReadLastError(IntPtr handle) => LastError(handle);

    private static NativeAction ToNative(Sts2Action action) => new()
    {
        Kind = action.Kind,
        Index = action.Index,
        Target = action.Target,
        Col = action.Col,
        Row = action.Row,
    };

    internal sealed class OwnedEnvHandle : SafeHandleZeroOrMinusOneIsInvalid
    {
        private readonly Sts2EnvLibrary _owner;
        internal OwnedEnvHandle(Sts2EnvLibrary owner, IntPtr raw) : base(true)
        {
            _owner = owner;
            SetHandle(raw);
        }

        protected override bool ReleaseHandle()
        {
            IntPtr raw = handle;
            handle = IntPtr.Zero;
            try { _owner.OnHandleReleased(raw); return true; }
            catch { return false; }
        }
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate uint AbiVersionCall();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate IntPtr LastStringCall();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate IntPtr EnvStringCall(IntPtr handle);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate IntPtr CreateCall();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate IntPtr CloneCall(IntPtr handle);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate void DestroyCall(IntPtr handle);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int ResetCall(IntPtr handle, [MarshalAs(UnmanagedType.LPUTF8Str)] string seed, int character, int ascension);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int ResetNamedCall(IntPtr handle, [MarshalAs(UnmanagedType.LPUTF8Str)] string seed, [MarshalAs(UnmanagedType.LPUTF8Str)] string character, int ascension);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int ResetCombatCall(IntPtr handle, [MarshalAs(UnmanagedType.LPUTF8Str)] string seed, int character, int ascension, int encounter, int actIndex);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int InspectCombatNamedCall(IntPtr handle, [MarshalAs(UnmanagedType.LPUTF8Str)] string seed, [MarshalAs(UnmanagedType.LPUTF8Str)] string character, int ascension, [MarshalAs(UnmanagedType.LPUTF8Str)] string encounter, int actIndex);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int SimpleStatusCall(IntPtr handle);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int EventRngCounterCall(IntPtr handle);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int ImportSnapshotCall(IntPtr handle, [In, MarshalAs(UnmanagedType.LPArray, ArraySubType = UnmanagedType.U1)] byte[] data, nuint length, out IntPtr successor);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int SuccessorCall(IntPtr handle, NativeAction action, out IntPtr successor);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int PreviewCall(IntPtr handle, NativeAction action, int traceLimit, out IntPtr successor);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int PreviewWithResultsCall(IntPtr handle, NativeAction action, int traceLimit, int queryFlags, out IntPtr successor);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int PredictOperationCall(IntPtr handle, [In, MarshalAs(UnmanagedType.LPArray, ArraySubType = UnmanagedType.U1)] byte[] request, nuint length, int traceLimit, int queryFlags, out IntPtr successor);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int ExportRunQueriesCall(IntPtr handle, int flags);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int DecisionCall(IntPtr handle);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate ulong HashCall(IntPtr handle);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int DeterminizeCall(IntPtr handle, ulong seed);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int StepCall(IntPtr handle, NativeAction action);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int BufferCountCall(IntPtr handle, IntPtr output, nuint capacity);
}
