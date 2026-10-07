using System.Text.Json;

namespace Sts2Env;

/// <summary>A single owned native Env. Calls on this instance are serialized.</summary>
public sealed class Sts2Environment : IDisposable
{
    private readonly object _gate = new();
    private readonly Sts2EnvLibrary _library;
    private readonly Sts2EnvLibrary.OwnedEnvHandle _handle;
    private bool _disposed;

    internal Sts2Environment(Sts2EnvLibrary library, Sts2EnvLibrary.OwnedEnvHandle handle)
    {
        _library = library;
        _handle = handle;
    }

    public void Reset(string seed, int character, int ascension = 0)
    {
        ArgumentNullException.ThrowIfNull(seed);
        Invoke(handle => { _library.Reset(handle, seed, character, ascension); return 0; });
    }

    public void ResetNamed(string seed, string character, int ascension = 0)
    {
        ArgumentNullException.ThrowIfNull(seed);
        ArgumentNullException.ThrowIfNull(character);
        Invoke(handle => { _library.ResetNamed(handle, seed, character, ascension); return 0; });
    }

    public void ResetCombat(string seed, int character, int ascension, int encounter, int actIndex = 0)
    {
        ArgumentNullException.ThrowIfNull(seed);
        Invoke(handle => { _library.ResetCombat(handle, seed, character, ascension, encounter, actIndex); return 0; });
    }

    /// <summary>Initializes this handle and returns the copied native diagnostic JSON.</summary>
    public string InspectCombatNamed(string seed, string character, int ascension, string encounter, int actIndex = 0)
    {
        ArgumentNullException.ThrowIfNull(seed);
        ArgumentNullException.ThrowIfNull(character);
        ArgumentNullException.ThrowIfNull(encounter);
        return Invoke(handle => _library.InspectCombatNamed(handle, seed, character, ascension, encounter, actIndex));
    }

    public string InspectCurrent() => Invoke(_library.InspectCurrent);
    public string ExportOracleCombatState() => Invoke(_library.ExportOracleCombatState);
    public string ExportOracleRunState() => Invoke(_library.ExportOracleRunState);
    public string ExportOracleRewards() => Invoke(_library.ExportOracleRewards);
    public int? EventRngCounter => Invoke(_library.EventRngCounter);
    public void ValidateCombatRoot() => Invoke(handle => { _library.ValidateCombatRoot(handle); return 0; });
    public void Determinize(ulong seed) => Invoke(handle => { _library.Determinize(handle, seed); return 0; });
    public Sts2Environment Clone() => Invoke(handle => new Sts2Environment(_library, _library.CloneHandle(handle)));
    public ulong StateHash => Invoke(_library.Hash);
    public Sts2Decision Decision => Invoke(_library.Decision);
    public Sts2Action[] LegalActions() => Invoke(_library.LegalActions);
    public byte[] ActionMask() => Invoke(_library.ActionMask);
    public int[] Observe() => Invoke(_library.Observe);

    public Sts2Decision Step(Sts2Action action) => Invoke(handle =>
    {
        _library.Step(handle, action);
        return _library.Decision(handle);
    });

    /// <summary>Returns an independent predicted successor. This handle remains unchanged.</summary>
    public Sts2Environment TryTransition(Sts2Action action) => Invoke(handle => _library.TryTransition(handle, action));

    /// <summary>Alias for TryTransition.</summary>
    public Sts2Environment Predict(Sts2Action action) => TryTransition(action);

    /// <summary>Returns an independent successor and copied factual trace JSON.</summary>
    public Sts2EnvPreview Preview(Sts2Action action, int traceLimit = 16_384) =>
        Invoke(handle => _library.Preview(handle, action, traceLimit, withResults: false, includeFuture: false));

    /// <summary>Preview plus before/after run facts. Optional flags request privileged future, RNG or model/Hook diagnostics.</summary>
    public Sts2EnvPreview PreviewWithResults(Sts2Action action, int traceLimit = 16_384, bool includeFuture = false, bool includeRng = false, bool includeCausal = false) =>
        Invoke(handle => _library.Preview(handle, action, traceLimit, withResults: true, includeFuture: includeFuture, includeRng: includeRng, includeCausal: includeCausal));

    /// <summary>
    /// Purely predicts one typed native operation and returns an independently
    /// owned successor plus copied operation/preview JSON. The source hash is
    /// captured from this Env while its call lock is held.
    /// </summary>
    public Sts2EnvPreview PredictOperation(Sts2Operation operation, int traceLimit = 16_384, int queryFlags = 0)
    {
        ArgumentNullException.ThrowIfNull(operation);
        return Invoke(handle => _library.PredictOperation(handle, operation, traceLimit, queryFlags));
    }

    public Sts2EnvSnapshot CaptureSnapshot() => Invoke(_library.CaptureSnapshot);

    public Sts2EnvPortableSnapshot CapturePortableSnapshot() => Invoke(_library.CapturePortableSnapshot);

    public Sts2Environment RestorePortableSnapshot(Sts2EnvPortableSnapshot snapshot)
    {
        ArgumentNullException.ThrowIfNull(snapshot);
        return ImportSnapshotJson(snapshot.Json);
    }

    /// <summary>Restores a snapshot captured by this loaded library session.</summary>
    public Sts2Environment RestoreSnapshot(Sts2EnvSnapshot snapshot)
    {
        ArgumentNullException.ThrowIfNull(snapshot);
        if (!ReferenceEquals(snapshot.Owner, _library)) throw new SnapshotSessionMismatchException();
        return Invoke(handle => _library.ImportSnapshot(handle, snapshot.Json));
    }

    /// <summary>
    /// Imports untrusted snapshot JSON through the native transactional validator.
    /// Snapshot pool references remain native-runtime scoped; use CaptureSnapshot when possible.
    /// </summary>
    public Sts2Environment ImportSnapshotJson(string json)
    {
        ArgumentNullException.ThrowIfNull(json);
        return Invoke(handle => _library.ImportSnapshot(handle, json));
    }

    public string ExportRunQueries(bool includeFuture = false) =>
        Invoke(handle => _library.ExportRunQueries(handle, includeFuture));
    public string ExportCombatQueries() => Invoke(_library.ExportCombatQueries);
    public string ExportCalculatedVars() => Invoke(_library.ExportCalculatedVars);
    public string ExportOrbs() => Invoke(_library.ExportOrbs);
    public string ExportPowers() => Invoke(_library.ExportPowers);

    public JsonDocument ExportRunFacts(bool includeFuture = false) => JsonDocument.Parse(ExportRunQueries(includeFuture));
    public JsonDocument ExportCombatFacts() => JsonDocument.Parse(ExportCombatQueries());
    public JsonDocument ExportCalculatedVarFacts() => JsonDocument.Parse(ExportCalculatedVars());
    public JsonDocument ExportOrbFacts() => JsonDocument.Parse(ExportOrbs());
    public JsonDocument ExportPowerFacts() => JsonDocument.Parse(ExportPowers());

    internal string ReadLastResult() => Invoke(_library.ReadLastResult);
    public string ReadLastError() => Invoke(_library.ReadLastError);

    public void Dispose()
    {
        lock (_gate)
        {
            if (_disposed) return;
            _disposed = true;
            _handle.Dispose();
        }
        GC.SuppressFinalize(this);
    }

    private T Invoke<T>(Func<IntPtr, T> operation)
    {
        lock (_gate)
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            bool addedReference = false;
            try
            {
                _handle.DangerousAddRef(ref addedReference);
                return operation(_handle.DangerousGetHandle());
            }
            finally
            {
                if (addedReference) _handle.DangerousRelease();
            }
        }
    }
}
