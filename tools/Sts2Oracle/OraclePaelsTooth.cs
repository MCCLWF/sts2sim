using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Entities.CardRewardAlternatives;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Relics;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.TestSupport;

// Runs Pael's Tooth's real pickup and first post-combat return hook in the
// shipped model. The selector makes only the pickup choice deterministic.
internal static class OraclePaelsTooth
{
    private sealed class FirstCardsSelector : ICardSelector
    {
        public Task<IEnumerable<CardModel>> GetSelectedCards(IEnumerable<CardModel> options, int minSelect, int maxSelect) =>
            Task.FromResult<IEnumerable<CardModel>>(options.Where(c => c.IsUpgradable).TakeLast(maxSelect).ToList());

        public CardRewardSelection GetSelectedCardReward(IReadOnlyList<CardCreationResult> options,
            IReadOnlyList<CardRewardAlternative> alternatives) => new() { card = options.FirstOrDefault()?.Card };
    }

    public static void Run(Dictionary<string, string> opts)
    {
        string[] seeds = (opts.GetValueOrDefault("seeds") ?? opts.GetValueOrDefault("seed", ""))
            .Split(',', StringSplitOptions.RemoveEmptyEntries);
        if (seeds.Length == 0) throw new ArgumentException("paels-tooth requires --seed or --seeds");
        foreach (string seed in seeds)
        {
            try
            {
                RunState run = Oracle.StartRun(new Dictionary<string, string>
                {
                    ["seed"] = seed, ["character"] = "IRONCLAD", ["ascension"] = "0"
                });
                var player = run.Players[0];
                PaelsTooth tooth;
                using (IDisposable selector = CardSelectCmd.UseSelector(new FirstCardsSelector()))
                {
                    tooth = (PaelsTooth)RelicCmd.Obtain(ModelDb.Relic<PaelsTooth>().ToMutable(), player)
                        .GetAwaiter().GetResult();
                }
                string[] saved = tooth.SerializableCards!.Select(c => c!.Id!.Entry).ToArray();
                var deckBefore = player.Deck.Cards.Select(c => c.Id.Entry)
                    .GroupBy(x => x).ToDictionary(g => g.Key, g => g.Count());
                tooth.AfterCombatEnd(null!).GetAwaiter().GetResult();
                var returned = player.Deck.Cards.Select(c => c.Id.Entry).GroupBy(x => x)
                    .SelectMany(g => Enumerable.Repeat(g.Key, Math.Max(0, g.Count() - deckBefore.GetValueOrDefault(g.Key))))
                    .ToArray();
                Program.Out.WriteLine(new JsonObject
                {
                    ["seed"] = seed,
                    ["saved"] = new JsonArray(saved.Select(x => (JsonNode?)JsonValue.Create(x)).ToArray()),
                    ["returned"] = new JsonArray(returned.Select(x => (JsonNode?)JsonValue.Create(x)).ToArray()),
                    ["remaining"] = tooth.SerializableCards!.Count,
                    ["display_amount"] = tooth.DisplayAmount
                }.ToJsonString());
            }
            finally
            {
                RunManager.Instance.CleanUp();
                CardSelectCmd.Reset();
            }
        }
    }
}
