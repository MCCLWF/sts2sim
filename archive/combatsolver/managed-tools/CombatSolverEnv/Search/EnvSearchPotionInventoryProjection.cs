using System.Collections.Frozen;
using System.Text.Json;
using CombatSolverEnv.Api;

namespace CombatSolver;

// Original Beam inventory-key encoding belongs to the Mod; the owned Env
// snapshot only supplies game potion slots and model IDs.
internal static class EnvSearchPotionInventoryProjection
{
    private static readonly FrozenDictionary<int, string> PotionEntries =
        EnvModelCatalog.Content.GetProperty("potions").EnumerateObject()
            .ToFrozenDictionary(model => model.Value.GetProperty("id").GetInt32(), model => model.Name);

    internal static StateFingerprint Capture(EnvCombatPredictionSnapshot snapshot, int rootSlotCount)
    {
        if (rootSlotCount is < 0 or > 8)
            throw new EnvProjectionIncompleteException("env_potion_key_root_slot_count_unmapped");
        JsonElement state = snapshot.Document.GetProperty("inner_snapshot").GetProperty("state");
        JsonElement player = state.GetProperty("run").GetProperty("player");
        JsonElement slots = player.GetProperty("potions");
        if (slots.GetArrayLength() != 8
            || player.GetProperty("max_potion_slots").GetInt32() != rootSlotCount)
            throw new EnvProjectionIncompleteException("env_potion_key_slot_layout_changed");
        Dictionary<int, JsonElement> potions = state.GetProperty("potions").GetProperty("slots")
            .EnumerateArray().ToDictionary(slot => slot.GetProperty("index").GetInt32(),
                slot => slot.GetProperty("value"));
        StateFingerprintBuilder key = new();
        key.Add(rootSlotCount);
        for (int slot = 0; slot < slots.GetArrayLength(); slot++)
        {
            int reference = slots[slot].GetInt32();
            if (slot >= rootSlotCount)
            {
                if (reference != 0)
                    throw new EnvProjectionIncompleteException("env_potion_key_outside_root_slots");
                continue;
            }
            if (reference == 0)
            {
                key.Add("-"); key.Add(false);
                continue;
            }
            if (!potions.TryGetValue(reference, out JsonElement potion)
                || potion.GetProperty("owner").GetInt32() != 1
                || potion.GetProperty("removed").GetBoolean()
                || !PotionEntries.TryGetValue(potion.GetProperty("id").GetInt32(), out string? entry))
                throw new EnvProjectionIncompleteException("env_potion_key_model_or_ref_unmapped");
            key.Add(entry); key.Add(true);
        }
        return key.Finish();
    }
}
