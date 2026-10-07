using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Entities.CardRewardAlternatives;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.RestSite;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Enchantments;
using MegaCrit.Sts2.Core.Models.Relics;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.TestSupport;

// Direct source probe for the choice on Pael's Growth pickup and the Clone
// rest-site option. The headless UI is absent, so only card selection is
// supplied by this deterministic test selector; the original models run.
internal static class OraclePaelsGrowth
{
    private sealed class FirstCardSelector : ICardSelector
    {
        public Task<IEnumerable<CardModel>> GetSelectedCards(IEnumerable<CardModel> options, int minSelect, int maxSelect) =>
            Task.FromResult<IEnumerable<CardModel>>(options.Take(maxSelect).ToList());

        public CardRewardSelection GetSelectedCardReward(IReadOnlyList<CardCreationResult> options,
            IReadOnlyList<CardRewardAlternative> alternatives) => new() { card = options.FirstOrDefault()?.Card };
    }

    public static void Run(Dictionary<string, string> opts)
    {
        string[] seeds = (opts.GetValueOrDefault("seeds") ?? opts.GetValueOrDefault("seed", ""))
            .Split(',', StringSplitOptions.RemoveEmptyEntries);
        if (seeds.Length == 0) throw new ArgumentException("paels-growth requires --seed or --seeds");
        foreach (string seed in seeds)
        {
            try
            {
                RunState run = Oracle.StartRun(new Dictionary<string, string>
                {
                    ["seed"] = seed, ["character"] = "IRONCLAD", ["ascension"] = "0"
                });
                var player = run.Players[0];
                using IDisposable selector = CardSelectCmd.UseSelector(new FirstCardSelector());
                RelicCmd.Obtain(ModelDb.Relic<PaelsGrowth>().ToMutable(), player).GetAwaiter().GetResult();
                CardModel selected = player.Deck.Cards.Single(c => c.Enchantment is Clone);
                int before = player.Deck.Cards.Count;
                new CloneRestSiteOption(player).OnSelect().GetAwaiter().GetResult();
                int copies = player.Deck.Cards.Count(c => c.Id == selected.Id && c.Enchantment is Clone);
                Program.Out.WriteLine(new JsonObject
                {
                    ["seed"] = seed,
                    ["selected"] = selected.Id.Entry,
                    ["enchantment_amount"] = selected.Enchantment!.Amount,
                    ["deck_before"] = before,
                    ["deck_after"] = player.Deck.Cards.Count,
                    ["copies"] = copies
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
