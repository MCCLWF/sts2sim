using CombatSolverEnv.Api;

namespace CombatSolver;

// Exact AppendOrbs portion of the original state key, using its numeric
// encoder. The complete combat/policy key is still separately gated.
internal sealed class EnvSearchOrbFingerprintProjection(EnvCombatOrbQueries queries)
{
    internal ulong PredictionHash => queries.PredictionHash;
    internal void AppendTo(ref StateFingerprintBuilder key)
    {
        key.Add('O');
        key.Add(queries.Capacity);
        key.Add(queries.Orbs.Count);
        foreach (EnvCombatOrbValue orb in queries.Orbs)
        {
            key.Add(orb.Id);
            key.Add(orb.Passive);
            key.Add(orb.Evoke);
        }
    }
    internal StateFingerprint Fingerprint
    {
        get { StateFingerprintBuilder key = new(); AppendTo(ref key); return key.Finish(); }
    }
}
