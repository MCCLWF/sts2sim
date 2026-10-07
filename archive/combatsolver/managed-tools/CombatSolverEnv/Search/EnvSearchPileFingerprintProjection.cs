using System.Text.Json;
using CombatSolverEnv.Api;

namespace CombatSolver;

// Original Beam pile retention and cycle-pile component from owned game card facts.
// The full cycle key also needs enemy, potion and search-boundary policy inputs.
internal sealed class EnvSearchPileFingerprintProjection
{
    private static readonly char[] Markers = ['H', 'D', 'C', 'X'];
    private readonly StateFingerprint[][] _cards;
    private EnvSearchPileFingerprintProjection(ulong predictionHash, StateFingerprint[][] cards,
        StateFingerprint unordered, StateFingerprint cyclePileShape)
    {
        PredictionHash = predictionHash; _cards = cards;
        UnorderedPileKey = unordered; CyclePileShapeKey = cyclePileShape;
    }

    internal ulong PredictionHash { get; }
    internal StateFingerprint UnorderedPileKey { get; }
    internal StateFingerprint CyclePileShapeKey { get; }

    internal static EnvSearchPileFingerprintProjection From(EnvCombatGameQueries queries,
        EnvSearchCardFingerprintProjection fingerprints)
    {
        if (queries.PredictionHash != fingerprints.PredictionHash)
            throw new EnvProjectionIncompleteException("env_pile_card_fingerprint_hash_mismatch");
        JsonElement piles = queries.Document.GetProperty("piles");
        if (piles.GetArrayLength() != 5)
            throw new EnvProjectionIncompleteException("env_pile_query_count_mismatch");
        StateFingerprintBuilder unordered = new(), cycle = new();
        StateFingerprint[][] cards = new StateFingerprint[4][];
        for (int pileIndex = 0; pileIndex < 4; pileIndex++)
        {
            JsonElement rows = piles[pileIndex].GetProperty("cards");
            cards[pileIndex] = new StateFingerprint[rows.GetArrayLength()];
            ulong first = 0, second = 0, cycleFirst = 0, cycleSecond = 0;
            int index = 0;
            foreach (JsonElement card in rows.EnumerateArray())
            {
                int reference = card.GetProperty("card_ref").GetInt32();
                if (!fingerprints.Cards.TryGetValue(reference, out StateFingerprint fingerprint))
                    throw new EnvProjectionIncompleteException("env_pile_card_fingerprint_missing");
                cards[pileIndex][index++] = fingerprint;
                first = unchecked(first + StateFingerprintBuilder.MixFirst(fingerprint.First));
                second = unchecked(second + StateFingerprintBuilder.MixSecond(fingerprint.Second));
                StateFingerprintBuilder structural = new();
                structural.Add(card.GetProperty("model_id").GetProperty("entry").GetString());
                structural.Add(card.GetProperty("card_state").GetProperty("upgrade").GetInt32());
                StateFingerprint shape = structural.Finish();
                cycleFirst = unchecked(cycleFirst + StateFingerprintBuilder.MixFirst(shape.First));
                cycleSecond = unchecked(cycleSecond + StateFingerprintBuilder.MixSecond(shape.Second));
            }
            unordered.Add(Markers[pileIndex]); unordered.Add(index);
            unordered.Add(first); unordered.Add(second);
            cycle.Add(Markers[pileIndex]); cycle.Add(index);
            cycle.Add(cycleFirst); cycle.Add(cycleSecond);
        }
        return new(queries.PredictionHash, cards, unordered.Finish(), cycle.Finish());
    }

    // AppendPile contract for the four original combat piles. Each invariant
    // bit selects the original unordered branch for H/D/C/X respectively.
    internal void AppendStateKeyPiles(ref StateFingerprintBuilder key, int pileOrderInvariantMask)
    {
        if ((pileOrderInvariantMask & ~15) != 0)
            throw new EnvProjectionIncompleteException("env_pile_order_invariant_mask_unknown");
        for (int pileIndex = 0; pileIndex < 4; pileIndex++)
        {
            StateFingerprint[] cards = _cards[pileIndex];
            key.Add(Markers[pileIndex]); key.Add(cards.Length);
            if ((pileOrderInvariantMask & (1 << pileIndex)) != 0)
            {
                ulong first = 0, second = 0;
                foreach (StateFingerprint card in cards)
                {
                    first = unchecked(first + StateFingerprintBuilder.MixFirst(card.First));
                    second = unchecked(second + StateFingerprintBuilder.MixSecond(card.Second));
                }
                key.Add(first); key.Add(second);
            }
            else
            {
                StateFingerprintBuilder ordered = new();
                ordered.Add(cards.Length);
                foreach (StateFingerprint card in cards)
                {
                    ordered.Add(card.First); ordered.Add(card.Second);
                }
                StateFingerprint pile = ordered.Finish();
                key.Add(pile.First); key.Add(pile.Second);
            }
        }
    }
}
