using System.Text.Json;

namespace CombatSolverEnv.Api;

public enum EnvCalculatedValueSemantics { Game, OriginalPrediction }

/// <summary>
/// Owned game calculated values and raw formula operands for each target.
/// The Mod derives the pinned original's retained-state calculation policy.
/// This covers the audited formulas, not the complete score/policy contract.
/// </summary>
public sealed class EnvCombatCalculatedVars
{
    private readonly Dictionary<(int Card, string Variable, int Target), (decimal Game, decimal Prediction)> _values = [];
    private EnvCombatCalculatedVars(JsonElement document) => Document = document;
    public JsonElement Document { get; }
    public ulong PredictionHash => Document.GetProperty("prediction_hash").GetUInt64();
    public bool PolicyProjectionComplete => false;

    /// <summary>Target ref 0 means null; references belong to this owned branch.</summary>
    public decimal GetValue(int cardRef, string variable, int targetRef, EnvCalculatedValueSemantics semantics)
    {
        if (!Enum.IsDefined(semantics)) throw new ArgumentOutOfRangeException(nameof(semantics));
        ArgumentException.ThrowIfNullOrWhiteSpace(variable);
        if (!_values.TryGetValue((cardRef, variable, targetRef), out var value))
            throw new InvalidOperationException("Calculated variable/target is absent from this branch");
        return semantics == EnvCalculatedValueSemantics.Game ? value.Game : value.Prediction;
    }

    internal static EnvCombatCalculatedVars Parse(string json, ulong expectedHash)
    {
        using JsonDocument parsed = JsonDocument.Parse(json);
        JsonElement document = parsed.RootElement;
        if (document.GetProperty("format").GetString() != "sts2env.combat_calculated_vars"
            || document.GetProperty("version").GetInt32() != 2
            || document.GetProperty("prediction_hash").GetUInt64() != expectedHash
            || document.GetProperty("query_semantics").GetString() != "game"
            || document.TryGetProperty("policy_projection_complete", out _)
            || document.TryGetProperty("prediction_semantics", out _))
            throw new InvalidOperationException("Combat calculated variable protocol or source hash mismatch");
        EnvCombatCalculatedVars result = new(document.Clone());
        foreach (JsonElement card in result.Document.GetProperty("cards").EnumerateArray())
        {
            int reference = card.GetProperty("card_ref").GetInt32();
            string variable = card.GetProperty("variable").GetString()!;
            int type = card.GetProperty("type").GetInt32();
            if (reference < 1 || string.IsNullOrWhiteSpace(variable) || type is < 1 or > 3)
                throw new InvalidOperationException("Combat calculated variable identity mismatch");
            _ = ReadDecimal(card.GetProperty("base_value"));
            string model = card.GetProperty("card_id").GetString()!;
            if (model is not ("BODY_SLAM" or "DEMONIC_SHIELD" or "PERFECTED_STRIKE" or "ASHEN_STRIKE"
                or "MIND_BLAST" or "BULLY" or "REND" or "GOLD_AXE" or "UNLEASH" or "EXPECT_A_FIGHT"
                or "BARRAGE" or "COMPILE_DRIVER" or "TEAR_ASUNDER"))
                throw new InvalidOperationException("Original calculated formula projection is unaudited: " + model);
            decimal basis = ReadDecimal(card.GetProperty("calculation_base"));
            decimal extra = ReadDecimal(card.GetProperty("calculation_extra"));
            foreach (JsonElement value in card.GetProperty("values").EnumerateArray())
            {
                int target = value.GetProperty("target_ref").GetInt32();
                if (value.TryGetProperty("prediction_value", out _))
                    throw new InvalidOperationException("Game calculation query contains search policy output");
                // Fixed CalculatedVarSpecRegistry reads branch operands after
                // terminal. This rule belongs to the Mod, not game Calculate.
                decimal prediction = basis + extra * ReadDecimal(value.GetProperty("raw_multiplier"));
                if (target < 0 || !result._values.TryAdd((reference, variable, target),
                    (ReadDecimal(value.GetProperty("game_value")), prediction)))
                    throw new InvalidOperationException("Combat calculated variable target identity mismatch");
            }
        }
        return result;
    }

    private static decimal ReadDecimal(JsonElement value)
    {
        JsonElement bits = value.GetProperty("decimal_bits");
        if (bits.GetArrayLength() != 4) throw new InvalidOperationException("Combat calculated decimal shape mismatch");
        return new decimal(bits.EnumerateArray().Select(word => unchecked((int)word.GetUInt32())).ToArray());
    }
}
