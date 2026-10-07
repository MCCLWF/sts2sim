using System.Collections.Frozen;
using System.Text.Json;
using CombatSolver.Engine.Common;
using CombatSolver.Engine.InCombat.Mirrors.Cards.OnPlay;

namespace CombatSolver;

// The Env trace contains game facts. Original risk dispatch, Self targets,
// deferred-entry indices and state-key counters belong to this Mod adapter.
// Mapping is audited only for the pinned default inference policy; other
// dispatch/Hook/choice/limit entries are not yet covered.
internal sealed class EnvSearchHistoryProjection
{
    private static readonly FrozenDictionary<int, string> CardEntries = EnvModelCatalog.Content.GetProperty("cards")
        .EnumerateObject().ToFrozenDictionary(card => card.Value.GetProperty("id").GetInt32(), card => card.Name);
    private EnvSearchHistoryProjection(JsonElement document) => Document = document;
    internal JsonElement Document { get; }
    internal bool Complete => false;
    internal void RequireComplete() => throw new EnvProjectionIncompleteException(
        "env_execution_unavailable:history_projection_incomplete");

    internal static EnvSearchHistoryProjection From(JsonElement snapshot)
    {
        if (!CardOnPlayMirrors.AllowInference)
            throw new EnvProjectionIncompleteException("env_history_default_inference_policy_required");
        JsonElement raw = snapshot.GetProperty("history").GetProperty("events");
        var ownerCreatures = snapshot.GetProperty("inner_snapshot").GetProperty("state")
            .GetProperty("creatures").GetProperty("slots").EnumerateArray()
            .Where(slot => slot.GetProperty("value").GetProperty("player").GetInt32() != 0)
            .ToDictionary(slot => slot.GetProperty("value").GetProperty("player").GetInt32(),
                slot => slot.GetProperty("index").GetInt32());
        if (!ownerCreatures.TryGetValue(1, out int playerCreature))
            throw new EnvProjectionIncompleteException("env_history_single_player_owner_missing");
        int[] indices = Enumerable.Repeat(-1, raw.GetArrayLength()).ToArray();
        int[] kinds = raw.EnumerateArray().Select(entry => entry[0].GetInt32()).ToArray();
        int[] counters = new int[6];
        int policyIndex = 0, rawIndex = 0;
        using MemoryStream stream = new();
        using (Utf8JsonWriter writer = new(stream))
        {
            writer.WriteStartObject();
            writer.WriteString("format", "combatsolver.env_policy_history");
            writer.WriteNumber("version", 1);
            writer.WriteNumber("prediction_hash", snapshot.GetProperty("prediction_hash").GetUInt64());
            writer.WriteBoolean("policy_projection_complete", false);
            writer.WriteStartArray("events");
            foreach (JsonElement entry in raw.EnumerateArray())
            {
                int kind = entry[0].GetInt32();
                if (kind == 9)
                {
                    if (!CardEntries.TryGetValue(entry[2].GetInt32(), out string? model))
                        throw new EnvProjectionIncompleteException("env_history_card_effect_model_unmapped");
                    PredictionRiskReason? reason = model switch
                    {
                        "RAMPAGE" => PredictionRiskReason.MethodMirrorIncomplete,
                        "VOID_FORM" => PredictionRiskReason.MethodNotMirrored,
                        _ => null,
                    };
                    if (reason.HasValue) { WriteRisk(writer, reason.Value); policyIndex++; }
                    rawIndex++;
                    continue;
                }
                indices[rawIndex] = policyIndex++;
                writer.WriteStartArray();
                for (int field = 0; field < 20; field++)
                {
                    if (field == 9 && kind is 5 or 7)
                    {
                        int parent = entry[field].GetInt32();
                        int expected = kind == 5 ? 4 : 6;
                        if (parent < 0 || parent >= rawIndex || indices[parent] < 0 || kinds[parent] != expected)
                            throw new EnvProjectionIncompleteException("env_history_deferred_parent_unmapped");
                        writer.WriteNumberValue(indices[parent]);
                    }
                    else if (field == 18 && entry[field].ValueKind == JsonValueKind.Array)
                    {
                        JsonElement play = entry[field];
                        int target = play[0].GetInt32();
                        if (target == 0 && play[9].GetInt32() == 1)
                        {
                            if (!ownerCreatures.TryGetValue(entry[3].GetInt32(), out target))
                                throw new EnvProjectionIncompleteException("env_history_self_owner_unmapped");
                        }
                        writer.WriteStartArray();
                        writer.WriteNumberValue(target);
                        for (int item = 1; item < 9; item++) play[item].WriteTo(writer);
                        writer.WriteEndArray();
                    }
                    else entry[field].WriteTo(writer);
                }
                writer.WriteNullValue(); // policy Risk reason only exists in Mod events
                writer.WriteEndArray();
                int owner = entry[3].GetInt32();
                switch (kind)
                {
                    case 1: counters[0]++; if (owner == 1 && entry[11].GetBoolean()) counters[1]++; break;
                    case 8 when owner == 1 && entry[11].GetBoolean(): counters[2]++; break;
                    case 2 when entry[4].GetInt32() == playerCreature && entry[7].GetInt32() > 0: counters[3]++; break;
                    case 4 when owner == 1: counters[4]++; break;
                    case 6 when owner == 1: counters[5]++; break;
                }
                rawIndex++;
            }
            writer.WriteEndArray();
            writer.WriteStartArray("counters");
            foreach (int count in counters) writer.WriteNumberValue(count);
            writer.WriteEndArray();
            writer.WriteEndObject();
        }
        using JsonDocument parsed = JsonDocument.Parse(stream.ToArray());
        return new(parsed.RootElement.Clone());
    }

    private static void WriteRisk(Utf8JsonWriter writer, PredictionRiskReason reason)
    {
        writer.WriteStartArray();
        writer.WriteNumberValue(9);
        for (int field = 1; field <= 10; field++) writer.WriteNumberValue(field == 9 ? -1 : 0);
        writer.WriteBooleanValue(false);
        writer.WriteNumberValue(0);
        for (int field = 13; field <= 15; field++) writer.WriteBooleanValue(false);
        writer.WriteStartArray(); writer.WriteEndArray();
        for (int field = 17; field <= 19; field++) writer.WriteNullValue();
        writer.WriteNumberValue((int)reason);
        writer.WriteEndArray();
    }
}
