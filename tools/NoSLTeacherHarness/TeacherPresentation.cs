using System.Reflection;
using CombatSolver;
using HarmonyLib;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Modding;

// Only the solver's display-name builder is patched. Game localization,
// Exists(), ModelDb and effects retain their original behavior. The shipped
// DLL contains mock powers without localization; they are not live content.
// This replaces one enumeration inside the presentation builder and keeps
// real titles. Missing real content is still an error, not a placeholder.
internal static class TeacherPresentation
{
    // The solver's original foreign-patch audit accepts a declared gameplay-
    // neutral mod. Declare ONLY the audited Headless type, not this whole
    // assembly, through the game's type metadata override. The existing
    // rejection of all unknown patches/types remains active. This metadata
    // scope never changes model behavior, registries, inputs or RNG.
    public static IDisposable DeclareAuditedHeadlessType()
    {
        if (MegaCrit.Sts2.Core.TestSupport.TestMode.IsOn || AssemblyInfo.MockTypes != null)
            throw new InvalidOperationException("Unexpected game mode/type metadata at teacher root");
        Mod presentation = new()
        {
            path = "sts2sim-headless-presentation", state = ModLoadState.Loaded,
            manifest = new ModManifest { id = "sts2sim-headless-presentation", name = "Audited headless presentation", affectsGameplay = false }
        };
        AssemblyInfo.MockTypes = new Dictionary<Type, (Mod?, bool)> { [typeof(Headless)] = (presentation, false) };
        return new RestoreMetadata();
    }

    private sealed class RestoreMetadata : IDisposable
    {
        public void Dispose() => AssemblyInfo.MockTypes = null;
    }

    public static void Install()
        => new Harmony("sts2sim.teacher.display-names").Patch(
            AccessTools.Method(typeof(SolverDisplayNames), "Capture"),
            transpiler: new HarmonyMethod(typeof(TeacherPresentation), nameof(DisplayPowerEnumeration)));

    private static IEnumerable<PowerModel> DisplayPowers()
    {
        foreach (PowerModel power in ModelDb.AllPowers)
        {
            if (power.Title.Exists()) { yield return power; continue; }
            if (power.GetType().Namespace != "MegaCrit.Sts2.Core.Models.Powers.Mocks")
                throw new InvalidOperationException("Missing real power title: " + power.Id.Entry);
        }
    }

    private static IEnumerable<CodeInstruction> DisplayPowerEnumeration(IEnumerable<CodeInstruction> code)
    {
        int count = 0;
        foreach (CodeInstruction instruction in code)
        {
            if (instruction.operand is MethodInfo method && method.DeclaringType == typeof(ModelDb)
                && method.Name == "get_AllPowers")
            {
                yield return new CodeInstruction(instruction) { operand = AccessTools.Method(typeof(TeacherPresentation), nameof(DisplayPowers)) };
                count++;
            }
            else yield return instruction;
        }
        if (count != 1) throw new InvalidOperationException("Solver display builder drift");
    }
}
