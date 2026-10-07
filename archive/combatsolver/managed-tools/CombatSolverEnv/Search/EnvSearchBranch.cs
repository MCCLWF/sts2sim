using CombatSolverEnv.Api;
using System.Text.Json;

namespace CombatSolver;

// One owned execution branch for the original search. Native simulation is
// never invoked here. Read documents are immutable and belong to this branch;
// callers still need the complete original-policy projection before executing
// a Beam request with Env as its backend.
internal sealed class EnvSearchBranch : IDisposable
{
    private readonly object _gate = new();
    private EnvCombatPredictionSession? _session;
    private EnvCombatPredictionSnapshot? _snapshot;
    private EnvSearchProjection? _projection;
    private EnvSearchHistoryProjection? _historyProjection;
    private EnvSearchPolicyProjection? _policyProjection;
    private EnvSearchThreatProjection? _threatProjection;
    private EnvCombatGameQueries? _gameQueries;
    private EnvSearchCardFingerprintProjection? _cardFingerprints;
    private EnvSearchPileFingerprintProjection? _pileFingerprints;
    private StateFingerprint? _potionInventoryKey;
    private EnvCombatCalculatedVars? _calculatedVars;
    private EnvSearchOrbFingerprintProjection? _orbFingerprint;
    private EnvCombatPowerQueries? _powerInputs;
    private EnvSearchPowerFingerprintProjection? _powerFingerprints;

    private readonly EnvSearchPowerLedger _powerLedger;
    private readonly int _rootPotionSlotCount;
    private readonly EnvSearchCycleEnemyProjection _cycleEnemies;
    private EnvSearchBranch(EnvCombatPredictionSession session, EnvSearchPowerLedger ledger,
        int rootPotionSlotCount, EnvSearchCycleEnemyProjection cycleEnemies)
    { _session = session; _powerLedger = ledger; _rootPotionSlotCount = rootPotionSlotCount; _cycleEnemies = cycleEnemies; }
    internal static EnvSearchBranch Open(EnvStateSnapshot root)
    {
        JsonElement run = root.State.GetProperty("run");
        if (!run.GetProperty("profile").GetProperty("full_veteran_compatible").GetBoolean()
            || !run.GetProperty("player").GetProperty("profile").GetProperty("full_veteran_compatible").GetBoolean())
            throw new EnvProjectionIncompleteException("env_search_live_profile_rules_unported");
        int rootPotionSlotCount = root.State.GetProperty("run").GetProperty("player")
            .GetProperty("max_potion_slots").GetInt32();
        if (rootPotionSlotCount is < 0 or > 8)
            throw new EnvProjectionIncompleteException("env_potion_key_root_slot_count_unmapped");
        EnvSearchCycleEnemyProjection cycleEnemies = EnvSearchCycleEnemyProjection.CaptureRoot(root);
        EnvCombatPredictionSession owned = EnvPredictionApi.OpenCombatPrediction(root);
        try
        {
            EnvCombatPowerQueries powers = owned.CapturePowerQueries();
            return new(owned, EnvSearchPowerLedger.CaptureRoot(powers), rootPotionSlotCount, cycleEnemies) { _powerInputs = powers };
        }
        catch { owned.Dispose(); throw; }
    }

    private EnvCombatPredictionSession RequireSession()
        => _session ?? throw new ObjectDisposedException(nameof(EnvSearchBranch));

    internal ulong StateHash { get { lock (_gate) return RequireSession().StateHash; } }
    internal EnvCombatPredictionSnapshot CaptureSnapshot()
    {
        lock (_gate)
        {
            EnvCombatPredictionSession session = RequireSession();
            return _snapshot ??= session.CaptureSnapshot();
        }
    }
    internal EnvSearchProjection CaptureProjection()
    {
        lock (_gate)
        {
            EnvCombatPredictionSession session = RequireSession();
            EnvCombatPredictionSnapshot snapshot = _snapshot ??= session.CaptureSnapshot();
            return _projection ??= EnvSearchProjection.From(snapshot,
                _historyProjection ??= EnvSearchHistoryProjection.From(snapshot.Document));
        }
    }
    internal EnvSearchPolicyProjection CapturePolicyProjection()
    {
        lock (_gate)
        {
            EnvCombatPredictionSession session = RequireSession();
            return _policyProjection ??= EnvSearchPolicyProjection.From(
                _gameQueries ??= session.CaptureGameQueries(), session.CapturePolicyProbe());
        }
    }
    internal EnvSearchThreatProjection CaptureThreatProjection()
    {
        lock (_gate)
        {
            EnvCombatPredictionSession session = RequireSession();
            return _threatProjection ??= EnvSearchThreatProjection.From(_gameQueries ??= session.CaptureGameQueries());
        }
    }
    internal EnvSearchCardFingerprintProjection CaptureCardFingerprints()
    {
        lock (_gate)
        {
            EnvCombatPredictionSession session = RequireSession();
            return _cardFingerprints ??= EnvSearchCardFingerprintProjection.From(
                _gameQueries ??= session.CaptureGameQueries());
        }
    }
    internal string InspectJson() { lock (_gate) return RequireSession().InspectJson(); }
    internal StateFingerprint CapturePotionInventoryKey()
    {
        lock (_gate)
        {
            EnvCombatPredictionSession session = RequireSession();
            return _potionInventoryKey ??= EnvSearchPotionInventoryProjection.Capture(
                _snapshot ??= session.CaptureSnapshot(), _rootPotionSlotCount);
        }
    }
    internal StateFingerprint CaptureCycleShapeKey(SearchBoundaryReason boundary)
    {
        lock (_gate)
        {
            EnvCombatPredictionSession session = RequireSession();
            EnvCombatPredictionSnapshot snapshot = _snapshot ??= session.CaptureSnapshot();
            EnvCombatGameQueries queries = _gameQueries ??= session.CaptureGameQueries();
            StateFingerprint pile = (_pileFingerprints ??= EnvSearchPileFingerprintProjection.From(queries,
                _cardFingerprints ??= EnvSearchCardFingerprintProjection.From(queries))).CyclePileShapeKey;
            StateFingerprint potions = _potionInventoryKey ??= EnvSearchPotionInventoryProjection.Capture(
                snapshot, _rootPotionSlotCount);
            StateFingerprintBuilder key = new();
            key.Add(pile.First); key.Add(pile.Second);
            key.Add(_cycleEnemies.CaptureAliveMask(snapshot));
            key.Add(potions.First); key.Add(potions.Second);
            key.Add((int)boundary);
            return key.Finish();
        }
    }
    internal EnvSearchPileFingerprintProjection CapturePileFingerprints()
    {
        lock (_gate)
        {
            EnvCombatPredictionSession session = RequireSession();
            EnvCombatGameQueries queries = _gameQueries ??= session.CaptureGameQueries();
            return _pileFingerprints ??= EnvSearchPileFingerprintProjection.From(queries,
                _cardFingerprints ??= EnvSearchCardFingerprintProjection.From(queries));
        }
    }
    internal EnvCombatGameQueries CaptureGameQueries()
    {
        lock (_gate)
        {
            EnvCombatPredictionSession session = RequireSession();
            return _gameQueries ??= session.CaptureGameQueries();
        }
    }
    internal EnvCombatPolicyProbe CapturePolicyProbe() { lock (_gate) return RequireSession().CapturePolicyProbe(); }
    internal EnvCombatCalculatedVars CaptureCalculatedVars()
    {
        lock (_gate)
        {
            EnvCombatPredictionSession session = RequireSession();
            return _calculatedVars ??= session.CaptureCalculatedVars();
        }
    }
    internal EnvSearchBranch Fork() { lock (_gate) return new(RequireSession().Clone(), _powerLedger, _rootPotionSlotCount, _cycleEnemies); }
    internal EnvSearchOrbFingerprintProjection CaptureOrbFingerprint()
    {
        lock (_gate)
        {
            EnvCombatPredictionSession session = RequireSession();
            return _orbFingerprint ??= new(session.CaptureOrbs());
        }
    }

    internal EnvCombatPowerQueries CapturePowerQueries()
    {
        lock (_gate)
        {
            EnvCombatPredictionSession session = RequireSession();
            return _powerInputs ??= session.CapturePowerQueries();
        }
    }
    internal EnvSearchPowerFingerprintProjection CapturePowerFingerprints()
    {
        lock (_gate)
        {
            EnvCombatPredictionSession session = RequireSession();
            return _powerFingerprints ??= EnvSearchPowerFingerprintProjection.From(_powerInputs ??= session.CapturePowerQueries(),
                _powerLedger.Retired(_snapshot ??= session.CaptureSnapshot()));
        }
    }

    /// <summary>
    /// Hard boundary for a future same-policy Beam adapter. Companion
    /// verification may inspect partial projections, but an execution caller
    /// must prove every state-key, policy, threat and history contract first.
    /// </summary>
    internal void RequireExecutionReady()
    {
        lock (_gate)
        {
            EnvCombatPredictionSession session = RequireSession();
            EnvCombatPredictionSnapshot snapshot = _snapshot ??= session.CaptureSnapshot();
            (_historyProjection ??= EnvSearchHistoryProjection.From(snapshot.Document)).RequireComplete();
            (_projection ??= EnvSearchProjection.From(snapshot, _historyProjection)).RequireOriginalStateKeyReady();
            _cardFingerprints ??= EnvSearchCardFingerprintProjection.From(_gameQueries ??= session.CaptureGameQueries());
            _orbFingerprint ??= new(session.CaptureOrbs());
            _powerFingerprints ??= EnvSearchPowerFingerprintProjection.From(_powerInputs ??= session.CapturePowerQueries(),
                _powerLedger.Retired(_snapshot ??= session.CaptureSnapshot()));
            (_policyProjection ??= EnvSearchPolicyProjection.From(
                _gameQueries ??= session.CaptureGameQueries(), session.CapturePolicyProbe())).RequireComplete();
            (_threatProjection ??= EnvSearchThreatProjection.From(_gameQueries ??= session.CaptureGameQueries())).RequireComplete();
        }
    }

    internal EnvSearchBranch Predict(PlanAction planned, uint? playerCombatId)
    {
        lock (_gate)
        {
            EnvCombatPredictionSession source = RequireSession();
            ulong before = source.StateHash;
            EnvCombatAction action = EnvSearchActionMapper.Map(planned, playerCombatId, source);
            EnvCombatPredictionSession? owned = null;
            owned = source.Predict(action);
            try
            {
                if (source.StateHash != before)
                    throw new InvalidOperationException("prediction_source_mutated");
                EnvSearchBranch successor = new(owned, _powerLedger, _rootPotionSlotCount, _cycleEnemies);
                owned = null;
                return successor;
            }
            finally { owned?.Dispose(); }
        }
    }

    public void Dispose()
    {
        lock (_gate)
        {
            _session?.Dispose();
            _session = null;
            _snapshot = null;
            _projection = null;
            _historyProjection = null;
            _policyProjection = null;
            _threatProjection = null;
            _gameQueries = null;
            _cardFingerprints = null;
            _pileFingerprints = null;
            _potionInventoryKey = null;
            _calculatedVars = null;
            _orbFingerprint = null;
            _powerInputs = null;
            _powerFingerprints = null;
        }
    }
}
