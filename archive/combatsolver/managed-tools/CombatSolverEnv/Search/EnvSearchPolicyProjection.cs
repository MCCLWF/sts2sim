using System.Collections.ObjectModel;
using System.Text.Json;
using CombatSolverEnv.Api;

namespace CombatSolver;

internal readonly record struct EnvPolicyTarget(int CreatureRef, int? CombatId, bool Valid, bool CanPlayTargeting);
internal readonly record struct EnvPolicyCard(
    int CardRef, int PileIndex, int Index, int HandIndex, string ModelId,
    bool CanPlay, int EnergyCost, int StarCost, IReadOnlyList<EnvPolicyTarget> Targets);

/// <summary>
/// Read-only portion of the original candidate legality boundary. It is
/// computed in the Mod from C++ game getter facts and remains explicitly
/// incomplete until all original policy hooks and target ordering are covered.
/// </summary>
internal sealed class EnvSearchPolicyProjection
{
    private EnvSearchPolicyProjection(ulong predictionHash, IReadOnlyList<EnvPolicyCard> cards)
    {
        PredictionHash = predictionHash; Cards = cards;
    }
    internal ulong PredictionHash { get; }
    internal IReadOnlyList<EnvPolicyCard> Cards { get; }
    internal bool Complete => false;
    internal void RequireComplete()
        => throw new EnvProjectionIncompleteException("env_search_policy_projection_missing:hook_and_target_policy");

    internal static EnvSearchPolicyProjection From(EnvCombatGameQueries queries, EnvCombatPolicyProbe probe)
    {
        if (queries.PredictionHash != probe.PredictionHash)
            throw new InvalidOperationException("env_search_policy_projection_hash_mismatch");
        Dictionary<(int pile, int index), (bool canPlay, int energy, int stars)> core = new();
        foreach (JsonElement item in probe.Document.GetProperty("cards").EnumerateArray())
            core[(item.GetProperty("pile_index").GetInt32(), item.GetProperty("index").GetInt32())] =
                (item.GetProperty("can_play").GetBoolean(), item.GetProperty("energy_cost").GetInt32(),
                 item.GetProperty("star_cost").GetInt32());
        List<EnvPolicyCard> cards = new();
        int pileIndex = 0;
        foreach (JsonElement pile in queries.Document.GetProperty("piles").EnumerateArray())
        {
            int index = 0;
            foreach (JsonElement item in pile.GetProperty("cards").EnumerateArray())
            {
                int cardRef = item.GetProperty("card_ref").GetInt32();
                int currentPile = pileIndex, currentIndex = index++;
                if (!core.TryGetValue((currentPile, currentIndex), out var legality))
                    throw new EnvProjectionIncompleteException("env_search_policy_projection_card_alignment");
                List<EnvPolicyTarget> targets = item.GetProperty("targets").EnumerateArray().Select(target =>
                    new EnvPolicyTarget(target.GetProperty("creature_ref").GetInt32(),
                        target.GetProperty("combat_id").ValueKind == JsonValueKind.Null
                            ? null : target.GetProperty("combat_id").GetInt32(),
                        target.GetProperty("is_valid_target").GetBoolean(),
                        target.GetProperty("can_play_targeting").GetBoolean())).ToList();
                cards.Add(new EnvPolicyCard(cardRef, currentPile, currentIndex,
                    item.GetProperty("hand_index").GetInt32(), item.GetProperty("model_id").GetProperty("entry").GetString()!,
                    legality.canPlay, legality.energy, legality.stars,
                    new ReadOnlyCollection<EnvPolicyTarget>(targets)));
            }
            pileIndex++;
        }
        return new(queries.PredictionHash, new ReadOnlyCollection<EnvPolicyCard>(cards));
    }
}
