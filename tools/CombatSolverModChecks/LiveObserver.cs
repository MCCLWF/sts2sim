using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text.Json.Nodes;
using CombatSolver;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Debug;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Runs;

// Attaches the mod's live capture to each oracle combat decision. The result
// is the canonical snapshot returned by the native import, or the bridge's own
// refusal text. Refusals are data for the comparison, not oracle failures.
internal static class LiveObserver
{
    internal static void Install() => OracleCombat.StateObserver = Observe;

    // AssemblyHasher asks Godot's OS singleton whether it runs in the editor
    // before hashing sts2.dll. That native call is a stub in this process, and
    // when the game logs through it first the cached answer is 0. The capture
    // pins the game build by this hash, so give it what the shipped game
    // computes: the first four bytes of the assembly's SHA-1.
    private static void PinAssemblyHash()
    {
        if (AssemblyHasher._mainAssemblyHash is { } cached && cached != 0) return;
        using FileStream source = new(typeof(AssemblyHasher).Assembly.Location, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
        AssemblyHasher._mainAssemblyHash = BinaryPrimitives.ReadInt32LittleEndian(SHA1.HashData(source));
    }

    private static JsonNode Observe(RunState run, CombatState combat, Player player)
    {
        try
        {
            PinAssemblyHash();
            EnvLiveCapture.Warmup(combat);
            // The oracle answers card selections through CardSelectCmd's test
            // selector; the game leaves it null. Everything else is the same
            // capture the mod performs.
            return new JsonObject { ["snapshot"] = JsonNode.Parse(EnvLiveCapture.CaptureRoot(combat, player)) };
        }
        catch (InvalidOperationException error)
        {
            return new JsonObject { ["error"] = error.Message };
        }
    }
}
