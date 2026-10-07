using System.Text.Json;
using MegaCrit.Sts2.Core.Entities.Cards;

namespace CombatSolverEnv.Api;

// The fixed original search's core CanPlayCard rule is Mod policy. Inputs are
// owned Env game getter facts; this never calls the game's effect/mirror code.
internal static class EnvCardPlayPolicy
{
    internal static JsonElement Project(EnvCombatGameQueries queries)
    {
        JsonElement document = queries.Document;
        JsonElement player = document.GetProperty("player");
        int energy = player.GetProperty("energy").GetInt32();
        int stars = player.GetProperty("stars").GetInt32();
        bool excess = player.GetProperty("can_pay_excess_energy_with_stars").GetBoolean();
        JsonElement piles = document.GetProperty("piles");
        bool enthralled = piles[0].GetProperty("cards").EnumerateArray()
            .Any(card => card.GetProperty("model_id").GetProperty("entry").GetString() == "ENTHRALLED");
        List<object> cards = [];
        foreach (JsonElement pile in piles.EnumerateArray())
            foreach (JsonElement card in pile.GetProperty("cards").EnumerateArray())
            {
                string model = card.GetProperty("model_id").GetProperty("entry").GetString()!;
                bool unplayable = card.GetProperty("effective_keywords").EnumerateArray()
                    .Any(keyword => keyword.GetInt32() == (int)CardKeyword.Unplayable);
                bool canPlay = false;
                int energyCost = 0, starCost = 0;
                if (!(enthralled && model != "ENTHRALLED") && !unplayable)
                {
                    JsonElement energyQuery = card.GetProperty("energy_cost");
                    JsonElement starQuery = card.GetProperty("star_cost");
                    energyCost = energyQuery.GetProperty("costs_x").GetBoolean()
                        ? energy : Math.Max(0, energyQuery.GetProperty("effective").GetInt32());
                    starCost = starQuery.GetProperty("costs_x").GetBoolean()
                        ? stars : Math.Max(0, starQuery.GetProperty("effective").GetInt32());
                    int payableEnergy = energyCost, payableStars = starCost;
                    if (payableEnergy > energy && excess)
                    {
                        payableStars += 2 * (payableEnergy - energy);
                        payableEnergy = energy;
                    }
                    // Vanilla CanPlay reports these two independent getter
                    // results even when resource or ally checks fail. Original
                    // CanPlay intentionally leaves AnyAlly to target validation.
                    UnplayableReason reason = (UnplayableReason)card.GetProperty("unplayable_reason").GetInt32();
                    canPlay = payableEnergy <= energy && payableStars <= stars
                        && (reason & (UnplayableReason.BlockedByHook | UnplayableReason.BlockedByCardLogic)) == 0
                        && card.GetProperty("card_state").GetProperty("affliction_id").GetString() != "SMOG";
                }
                cards.Add(new { pile_index = card.GetProperty("pile_index").GetInt32(),
                    index = card.GetProperty("index").GetInt32(), card_id = model,
                    can_play = canPlay, energy_cost = energyCost, star_cost = starCost });
            }
        return JsonSerializer.SerializeToElement(new { format = "combatsolver.env_policy_probe", version = 1,
            prediction_hash = queries.PredictionHash, policy_projection_complete = false,
            scope = "CanPlayCard_core", cards });
    }
}
