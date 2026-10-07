using System.Collections.ObjectModel;
using System.Text.Json;
using CombatSolverEnv.Api;

namespace CombatSolver;

/// <summary>
/// Original card-key encoding from owned Env values. Ordinary numeric-var
/// cards are supported; missing subclass or attachment operands refuse the
/// projection. This does not provide the complete original state key.
/// </summary>
internal sealed class EnvSearchCardFingerprintProjection
{
    private EnvSearchCardFingerprintProjection(ulong predictionHash,
        IReadOnlyDictionary<int, StateFingerprint> cards)
    { PredictionHash = predictionHash; Cards = cards; }

    internal ulong PredictionHash { get; }
    internal IReadOnlyDictionary<int, StateFingerprint> Cards { get; }

    internal static EnvSearchCardFingerprintProjection From(EnvCombatGameQueries queries)
    {
        Dictionary<int, StateFingerprint> cards = new();
        foreach (JsonElement pile in queries.Document.GetProperty("piles").EnumerateArray())
            foreach (JsonElement card in pile.GetProperty("cards").EnumerateArray())
                cards.Add(card.GetProperty("card_ref").GetInt32(), Capture(card));
        return new(queries.PredictionHash, new ReadOnlyDictionary<int, StateFingerprint>(cards));
    }

    private static StateFingerprint Capture(JsonElement card)
    {
        string model = card.GetProperty("model_id").GetProperty("entry").GetString()!;
        JsonElement input = card.GetProperty("card_state");
        if (model is "CLAW" or "GENETIC_ALGORITHM" or "MAUL" or "MAD_SCIENCE" or "THE_SCYTHE")
            throw new EnvProjectionIncompleteException("env_card_fingerprint_missing:" + model + ":card_subclass_fingerprint_unmapped");
        if (input.GetProperty("enchantment_id").ValueKind != JsonValueKind.Null
            || input.GetProperty("affliction_id").ValueKind != JsonValueKind.Null)
            throw new EnvProjectionIncompleteException("env_card_fingerprint_missing:" + model + ":card_attachment_fingerprint_unmapped");
        StateFingerprintBuilder key = new(); // Pinned upstream's numeric encoder.
        key.Add(model);
        key.Add(input.GetProperty("upgrade").GetInt32());
        key.Add(card.GetProperty("energy_cost").GetProperty("costs_x").GetBoolean());
        key.Add(card.GetProperty("energy_cost").GetProperty("local").GetInt32());
        key.Add(card.GetProperty("star_cost").GetProperty("costs_x").GetBoolean());
        key.Add(card.GetProperty("star_cost").GetProperty("local").GetInt32());
        key.Add(input.GetProperty("base_replay_count").GetInt32());
        key.Add(input.GetProperty("exhaust_on_next_play").GetBoolean());
        key.Add(input.GetProperty("sly").GetBoolean());
        key.Add(input.GetProperty("retain").GetBoolean());
        ulong keywords = 0;
        SortedSet<int> overflow = [];
        foreach (JsonElement keyword in card.GetProperty("local_keywords").EnumerateArray())
        {
            int value = keyword.GetInt32();
            if ((uint)value < 64) keywords |= 1UL << value;
            else overflow.Add(value);
        }
        key.Add(keywords); key.Add(overflow.Count);
        foreach (int value in overflow) key.Add(value);
        key.Add(input.GetProperty("deck_version").GetBoolean());
        key.Add(input.GetProperty("removed").GetBoolean());
        key.Add((string?)null); // EnchantmentStateSupport.Append(null).
        key.Add((string?)null); key.Add(0); // No affliction.
        ulong first = 0, second = 0;
        int count = 0;
        foreach (JsonElement variable in input.GetProperty("dynamic_vars").EnumerateArray())
        {
            // SemanticStateFieldPolicy classifies CalculatedVar as Derived.
            // No card StringVar is classified in the fixed upstream policy.
            if (variable.GetProperty("is_calculated").GetBoolean()) continue;
            if (variable.GetProperty("is_string").GetBoolean())
                throw new EnvProjectionIncompleteException("env_card_fingerprint_missing:" + model + ":card_string_var_fingerprint_unclassified");
            StateFingerprintBuilder item = new();
            item.Add(variable.GetProperty("name").GetString());
            AppendDecimal(ref item, variable.GetProperty("base_value"));
            StateFingerprint fingerprint = item.Finish();
            first = unchecked(first + StateFingerprintBuilder.MixFirst(fingerprint.First));
            second = unchecked(second + StateFingerprintBuilder.MixSecond(fingerprint.Second));
            count++;
        }
        key.Add(count); key.Add(first); key.Add(second);
        if (model == "RAMPAGE") AppendDecimal(ref key, input.GetProperty("subclass").GetProperty("growth_damage"));
        return key.Finish();
    }

    private static void AppendDecimal(ref StateFingerprintBuilder key, JsonElement value)
    {
        // Add(decimal) consumes signed int words, including scale/sign.
        JsonElement bits = value.GetProperty("decimal_bits");
        if (bits.GetArrayLength() != 4)
            throw new EnvProjectionIncompleteException("env_card_fingerprint_decimal_shape");
        foreach (JsonElement word in bits.EnumerateArray()) key.Add(unchecked((int)word.GetUInt32()));
    }
}
