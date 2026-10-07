using System.Text.Json;

namespace CombatSolverEnv.Api;

/// <summary>Owned game power fields in game hook-listener order, without search filtering.</summary>
public sealed class EnvCombatPowerQueries
{
    private EnvCombatPowerQueries(JsonElement document) => Document = document;
    public JsonElement Document { get; }
    public ulong PredictionHash => Document.GetProperty("prediction_hash").GetUInt64();
    internal static EnvCombatPowerQueries Parse(string json, ulong expectedHash)
    {
        using JsonDocument parsed = JsonDocument.Parse(json);
        JsonElement document = parsed.RootElement;
        if (document.GetProperty("format").GetString() != "sts2env.combat_power_queries"
            || document.GetProperty("version").GetInt32() != 1
            || document.GetProperty("query_semantics").GetString() != "game"
            || document.GetProperty("prediction_hash").GetUInt64() != expectedHash)
            throw new InvalidOperationException("Combat power query protocol or source hash mismatch");
        HashSet<int> references = [];
        foreach (JsonElement power in document.GetProperty("powers").EnumerateArray())
        {
            int reference = power.GetProperty("power_ref").GetInt32();
            string? model = power.GetProperty("power_id").GetString();
            if (reference < 1 || !references.Add(reference) || string.IsNullOrWhiteSpace(model))
                throw new InvalidOperationException("Combat power query identity mismatch");
            _ = power.GetProperty("owner_combat_id").GetUInt32();
            _ = power.GetProperty("applier_combat_id").GetUInt32();
            _ = power.GetProperty("target_combat_id").GetUInt32();
            _ = power.GetProperty("amount").GetInt32();
            _ = power.GetProperty("amount_on_turn_start").GetInt32();
            _ = power.GetProperty("skip_next_duration_tick").GetBoolean();
            JsonElement ritual = power.GetProperty("ritual_flag"), surrounded = power.GetProperty("surrounded_direction");
            if (model == "RITUAL_POWER") _ = ritual.GetBoolean();
            else if (ritual.ValueKind != JsonValueKind.Null) throw new InvalidOperationException("Unexpected Ritual field");
            if (model == "SURROUNDED_POWER")
            {
                if (surrounded.GetInt32() is not (0 or 1)) throw new InvalidOperationException("Invalid Surrounded direction");
            }
            else if (surrounded.ValueKind != JsonValueKind.Null) throw new InvalidOperationException("Unexpected Surrounded field");
            HashSet<string> variables = new(StringComparer.Ordinal);
            foreach (JsonElement variable in power.GetProperty("dynamic_vars").EnumerateArray())
            {
                string? name = variable.GetProperty("name").GetString();
                JsonElement bits = variable.GetProperty("base_value").GetProperty("decimal_bits");
                if (string.IsNullOrWhiteSpace(name) || !variables.Add(name) || bits.GetArrayLength() != 4)
                    throw new InvalidOperationException("Combat power query variable shape mismatch");
                _ = variable.GetProperty("is_calculated").GetBoolean();
                _ = variable.GetProperty("is_string").GetBoolean();
                _ = new decimal(bits.EnumerateArray().Select(word => unchecked((int)word.GetUInt32())).ToArray());
            }
        }
        return new(document.Clone());
    }
}
