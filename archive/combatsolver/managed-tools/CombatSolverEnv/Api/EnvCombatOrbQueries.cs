using System.Text.Json;

namespace CombatSolverEnv.Api;

public readonly record struct EnvCombatOrbValue(int Ref, string Id, decimal Passive, decimal Evoke);

/// <summary>Owned game orb queue and shipped passive/evoke getter values.</summary>
public sealed class EnvCombatOrbQueries
{
    private EnvCombatOrbQueries(JsonElement document, int capacity, EnvCombatOrbValue[] orbs)
    { Document = document; Capacity = capacity; Orbs = Array.AsReadOnly(orbs); }
    public JsonElement Document { get; }
    public ulong PredictionHash => Document.GetProperty("prediction_hash").GetUInt64();
    public bool OriginalStateKeyComplete => false;
    public int Capacity { get; }
    public IReadOnlyList<EnvCombatOrbValue> Orbs { get; }

    internal static EnvCombatOrbQueries Parse(string json, ulong expectedHash)
    {
        using JsonDocument parsed = JsonDocument.Parse(json);
        JsonElement document = parsed.RootElement;
        if (document.GetProperty("format").GetString() != "sts2env.combat_orb_queries"
            || document.GetProperty("version").GetInt32() != 2
            || document.GetProperty("prediction_hash").GetUInt64() != expectedHash
            || document.GetProperty("query_semantics").GetString() != "game"
            || document.TryGetProperty("original_state_key_complete", out _))
            throw new InvalidOperationException("Combat orb query protocol or source hash mismatch");
        int capacity = document.GetProperty("capacity").GetInt32();
        HashSet<int> references = [];
        EnvCombatOrbValue[] orbs = document.GetProperty("orbs").EnumerateArray().Select(orb =>
        {
            int reference = orb.GetProperty("orb_ref").GetInt32();
            string id = orb.GetProperty("orb_id").GetString()!;
            if (reference < 1 || !references.Add(reference)
                || id is not ("LIGHTNING_ORB" or "FROST_ORB" or "DARK_ORB" or "GLASS_ORB" or "PLASMA_ORB"))
                throw new InvalidOperationException("Combat orb query identity mismatch");
            decimal ReadDecimal(string field)
            {
                JsonElement bits = orb.GetProperty(field).GetProperty("decimal_bits");
                if (bits.GetArrayLength() != 4) throw new InvalidOperationException("Combat orb decimal shape mismatch");
                return new decimal(bits.EnumerateArray().Select(word => unchecked((int)word.GetUInt32())).ToArray());
            }
            return new EnvCombatOrbValue(reference, id, ReadDecimal("passive_value"), ReadDecimal("evoke_value"));
        }).ToArray();
        if (capacity is < 0 or > 10 || orbs.Length > capacity)
            throw new InvalidOperationException("Combat orb query capacity mismatch");
        return new(document.Clone(), capacity, orbs);
    }
}
