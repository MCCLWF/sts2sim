using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Models.Enchantments;
using MegaCrit.Sts2.Core.Models.Events;
using MegaCrit.Sts2.Core.Models.Relics;
using MegaCrit.Sts2.Core.Runs;

// Original-game Archaic Tooth probe with an upgraded, enchanted starter card.
internal static class OracleOrobasEnchanted
{
    public static void Run(Dictionary<string, string> opts)
    {
        string[] seeds = (opts.GetValueOrDefault("seeds") ?? opts.GetValueOrDefault("seed", ""))
            .Split(',', StringSplitOptions.RemoveEmptyEntries);
        if (seeds.Length == 0) throw new ArgumentException("orobas-enchanted requires --seed or --seeds");
        foreach (string seed in seeds)
        {
            try
            {
                RunState run = Oracle.StartRun(new Dictionary<string, string>
                {
                    ["seed"] = seed, ["character"] = "IRONCLAD", ["ascension"] = "0"
                });
                var player = run.Players[0];
                Bash bash = player.Deck.Cards.OfType<Bash>().First();
                CardCmd.Upgrade(bash);
                CardCmd.Enchant<Sharp>(bash, 2m);
                var eventModel = (Orobas)ModelDb.Event<Orobas>().ToMutable();
                eventModel.BeginEvent(player, null, isPreFinished: false).GetAwaiter().GetResult();
                var tooth = (ArchaicTooth)ModelDb.Relic<ArchaicTooth>().ToMutable();
                if (!tooth.SetupForPlayer(player)) throw new InvalidOperationException("Archaic Tooth setup failed");
                var preview = CardModel.FromSerializable(tooth.AncientCard!);
                RelicCmd.Obtain(tooth, player).GetAwaiter().GetResult();
                var transformed = player.Deck.Cards.OfType<Break>().Single();
                Program.Out.WriteLine(new JsonObject
                {
                    ["seed"] = seed,
                    ["preview"] = Dump(preview),
                    ["transformed"] = Dump(transformed),
                    ["bash_remaining"] = player.Deck.Cards.OfType<Bash>().Count()
                }.ToJsonString());
            }
            finally
            {
                RunManager.Instance.CleanUp();
            }
        }
    }

    private static JsonObject Dump(CardModel card) => new()
    {
        ["id"] = card.Id.Entry,
        ["upgrade"] = card.CurrentUpgradeLevel,
        ["enchantment"] = card.Enchantment?.Id.Entry,
        ["enchantment_amount"] = card.Enchantment?.Amount
    };
}
