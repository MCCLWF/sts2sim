extern alias Derived;
using System.Text.Json;
using System.Text.Json.Nodes;

internal static class CardProjectionRefusalChecks
{
    internal static void Check(JsonElement gameQueries)
    {
        string originalText = gameQueries.GetRawText();
        ulong predictionHash = gameQueries.GetProperty("prediction_hash").GetUInt64();

        JsonObject enchantment = CloneDocument();
        FirstCard(enchantment)["card_state"]!.AsObject()["enchantment_id"] = JsonValue.Create("STEADY");
        ExpectRefusal(enchantment, "card_attachment_fingerprint_unmapped");

        JsonObject affliction = CloneDocument();
        FirstCard(affliction)["card_state"]!.AsObject()["affliction_id"] = JsonValue.Create("SMOG");
        ExpectRefusal(affliction, "card_attachment_fingerprint_unmapped");

        JsonObject subclass = CloneDocument();
        FirstCard(subclass)["model_id"]!.AsObject()["entry"] = JsonValue.Create("CLAW");
        ExpectRefusal(subclass, "card_subclass_fingerprint_unmapped");

        JsonObject stringVariable = CloneDocument();
        JsonObject cardState = FirstCard(stringVariable)["card_state"]!.AsObject();
        JsonObject firstVariable = cardState["dynamic_vars"]!.AsArray()[0]!.AsObject();
        firstVariable["is_calculated"] = JsonValue.Create(false);
        firstVariable["is_string"] = JsonValue.Create(true);
        ExpectRefusal(stringVariable, "card_string_var_fingerprint_unclassified");

        if (!string.Equals(originalText, gameQueries.GetRawText(), StringComparison.Ordinal))
            throw new InvalidOperationException("card_projection_refusal_mutated_source");

        JsonObject CloneDocument() => JsonNode.Parse(originalText)!.AsObject();

        void ExpectRefusal(JsonObject document, string suffix)
        {
            try
            {
                _ = Derived::CombatSolver.EnvSearchCardFingerprintProjection.From(
                    Derived::CombatSolverEnv.Api.EnvCombatGameQueries.Parse(document.ToJsonString(), predictionHash));
            }
            catch (Derived::CombatSolver.EnvProjectionIncompleteException error)
            {
                if (!error.Message.EndsWith(suffix, StringComparison.Ordinal))
                    throw new InvalidOperationException("card_projection_refusal_mismatch:" + suffix + ":" + error.Message);
                return;
            }

            throw new InvalidOperationException("card_projection_refusal_missing:" + suffix);
        }
    }

    private static JsonObject FirstCard(JsonObject document)
        => document["piles"]!.AsArray()[0]!.AsObject()["cards"]!.AsArray()[0]!.AsObject();
}
