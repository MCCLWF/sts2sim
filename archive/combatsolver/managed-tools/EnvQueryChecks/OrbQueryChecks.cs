extern alias Derived;
using System.Reflection;
using System.Text.Json;
using CombatSolver;
using CombatSolver.Engine.InCombat.Mirrors.Orbs;
using CombatSolver.Engine.InCombat.Simulation;
using CombatSolverEnv.Api;
using MegaCrit.Sts2.Core.Entities.Players;

internal static class OrbQueryChecks
{
    internal static void Compare(Player player, CombatPredictionSimulator native,
        EnvCombatPredictionSession env, string name, string output)
    {
        ulong hash = env.StateHash;
        string before = env.CaptureSnapshot().Document.GetRawText();
        EnvCombatOrbQueries actual = env.CaptureOrbs();
        SimOrbQueue queue = native.State.GetPlayerCombatState(player).OrbQueue;
        object Bits(decimal value) => new { decimal_bits = decimal.GetBits(value).Select(word => unchecked((uint)word)).ToArray() };
        if (actual.Capacity != queue.Capacity || actual.Orbs.Count != queue.Orbs.Count)
            throw new InvalidOperationException("orb_queue_shape_mismatch:" + name);
        object[] orbs = queue.Orbs.Select((orb, index) =>
        {
            decimal passive = OrbMirrors.GetPassiveValue(native, orb), evoke = OrbMirrors.GetEvokeValue(native, orb);
            var live = player.PlayerCombatState!.OrbQueue.Orbs[index];
            if (!decimal.GetBits(passive).SequenceEqual(decimal.GetBits(live.PassiveVal))
                || !decimal.GetBits(evoke).SequenceEqual(decimal.GetBits(live.EvokeVal)))
                throw new InvalidOperationException("game_original_orb_value_mismatch:" + name);
            EnvCombatOrbValue value = actual.Orbs[index];
            if (value.Id != orb.Id.Entry || !decimal.GetBits(passive).SequenceEqual(decimal.GetBits(value.Passive))
                || !decimal.GetBits(evoke).SequenceEqual(decimal.GetBits(value.Evoke)))
                throw new InvalidOperationException("env_orb_value_mismatch:" + name + ":" + index);
            return (object)new { orb_ref = index + 1, orb_id = orb.Id.Entry,
                passive_value = Bits(passive), evoke_value = Bits(evoke) };
        }).ToArray();
        JsonElement expected = JsonSerializer.SerializeToElement(new {
            format = "sts2env.combat_orb_queries", version = 2, prediction_hash = hash,
            query_semantics = "game", capacity = queue.Capacity, orbs });
        // Invoke the pinned original encoder itself, including its ref builder.
        MethodInfo append = typeof(CombatBeamSolver).GetMethod("AppendOrbs", BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic)!;
        object[] arguments = [new StateFingerprintBuilder(), native, queue];
        append.Invoke(null, arguments);
        StateFingerprint original = ((StateFingerprintBuilder)arguments[0]).Finish();
        var projected = new Derived::CombatSolver.EnvSearchOrbFingerprintProjection(
            Derived::CombatSolverEnv.Api.EnvCombatOrbQueries.Parse(actual.Document.GetRawText(), hash));
        if (projected.Fingerprint != original || projected.PredictionHash != hash
            || !JsonElement.DeepEquals(expected, actual.Document) || env.StateHash != hash
            || env.CaptureSnapshot().Document.GetRawText() != before)
            throw new InvalidOperationException("orb_key_or_document_mismatch:" + name);
        using EnvCombatPredictionSession sibling = env.Clone();
        if (!JsonElement.DeepEquals(actual.Document, sibling.CaptureOrbs().Document)
            || !JsonElement.DeepEquals(actual.Document, env.CaptureOrbs().Document))
            throw new InvalidOperationException("orb_query_clone_or_purity_mismatch:" + name);
        File.WriteAllText(Path.Combine(output, name + ".native-orbs.json"), expected.GetRawText());
        File.WriteAllText(Path.Combine(output, name + ".public-orbs.json"), actual.Document.GetRawText());
        Console.WriteLine($"original orb fingerprint {name}: capacity={queue.Capacity}; orbs={orbs.Length}; game_original_env_exact_bits=true; original_encoder=true; complete_state_key=false");
    }
}
