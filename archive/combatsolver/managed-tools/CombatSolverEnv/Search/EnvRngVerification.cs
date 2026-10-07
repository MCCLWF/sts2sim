using CombatSolver.Engine.Common;
using CombatSolver.Engine.InCombat.Simulation;
using CombatSolverEnv.Api;
using Sts2EnvCompare;

namespace CombatSolver;

// Both sides are owned prediction branches. State getters on the native RNG
// retain its lazy frozen streams; this observer never materializes/advances one.
internal static class EnvRngVerification
{
    internal static void RequireMatch(NativeEnv.Session env, CombatPredictionRngSet native, string boundary)
    {
        RequireMatch(EnvStateSnapshot.Parse(env.ExportSnapshotJson()), native, boundary);
    }

    internal static void RequireMatch(EnvSearchBranch env, CombatPredictionRngSet native, string boundary)
    {
        EnvCombatPredictionSnapshot prediction = env.CaptureSnapshot();
        RequireMatch(EnvStateSnapshot.Parse(prediction.Document.GetProperty("inner_snapshot").GetRawText()), native, boundary);
    }

    private static void RequireMatch(EnvStateSnapshot snapshot, CombatPredictionRngSet native, string boundary)
    {
        Check(EnvRunRngStream.Shuffle, native.ShuffleState);
        Check(EnvRunRngStream.CombatCardGeneration, native.CombatCardGenerationState);
        Check(EnvRunRngStream.CombatPotionGeneration, native.CombatPotionGenerationState);
        Check(EnvRunRngStream.CombatCardSelection, native.CombatCardSelectionState);
        Check(EnvRunRngStream.CombatEnergyCosts, native.CombatEnergyCostsState);
        Check(EnvRunRngStream.CombatTargets, native.CombatTargetsState);
        Check(EnvRunRngStream.MonsterAi, native.MonsterAiState);
        Check(EnvRunRngStream.Niche, native.NicheState);
        Check(EnvRunRngStream.CombatOrbs, native.CombatOrbGenerationState);

        void Check(EnvRunRngStream stream, PredictionRngState expected)
        {
            EnvRngState actual = snapshot.GetRunRng(stream);
            EnvRngComparison.RequireMatch(actual,
                new EnvRngState(expected.Counter, expected.State0, expected.State1, expected.State2, expected.State3),
                stream, boundary);
        }
    }
}
