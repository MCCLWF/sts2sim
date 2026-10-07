using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text.Json.Nodes;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Events;
using MegaCrit.Sts2.Core.Random;
using MegaCrit.Sts2.Core.Runs;

// Direct original-game inventory probe. It invokes the shipped event's own
// BeginEvent, which shuffles the fake relic list and creates/prices six entries.
internal static class OracleFakeMerchant
{
    private static readonly FieldInfo CounterField = typeof(Rng).GetField("_counter", BindingFlags.Instance | BindingFlags.NonPublic)
        ?? throw new MissingFieldException(typeof(Rng).FullName, "_counter");

    public static void Run(Dictionary<string, string> opts)
    {
        string[] seeds = (opts.GetValueOrDefault("seeds") ?? opts.GetValueOrDefault("seed", ""))
            .Split(',', StringSplitOptions.RemoveEmptyEntries);
        if (seeds.Length == 0) throw new ArgumentException("fake-merchant requires --seed or --seeds");
        foreach (string seed in seeds)
        {
            try
            {
                var setup = new Dictionary<string, string>
                {
                    ["seed"] = seed, ["character"] = "IRONCLAD", ["ascension"] = "0"
                };
                RunState run = Oracle.StartRun(setup);
                run.CurrentActIndex = 1;
                var player = run.Players[0];
                PlayerCmd.SetGold(300, player).GetAwaiter().GetResult();
                var merchant = (FakeMerchant)ModelDb.Event<FakeMerchant>().ToMutable();
                merchant.BeginEvent(player, null, isPreFinished: false).GetAwaiter().GetResult();
                var entries = new JsonArray(merchant.Inventory.RelicEntries.Select(e =>
                    (JsonNode?)new JsonObject { ["id"] = e.Model!.Id.Entry, ["cost"] = e.Cost }).ToArray());
                Program.Out.WriteLine(new JsonObject
                {
                    ["seed"] = seed,
                    ["event_rng_counter"] = Convert.ToInt32(CounterField.GetValue(merchant.Rng)),
                    ["shops_rng_counter"] = Convert.ToInt32(CounterField.GetValue(player.PlayerRng.Shops)),
                    ["entries"] = entries
                }.ToJsonString());
            }
            finally
            {
                RunManager.Instance.CleanUp();
            }
        }
    }
}
