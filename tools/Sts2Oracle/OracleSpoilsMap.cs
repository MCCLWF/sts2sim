using System;
using System.Collections.Generic;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Runs;

// Direct source probe for the Act 2 Spoils Map topology and quest marker.
internal static class OracleSpoilsMap
{
    public static void Run(Dictionary<string, string> opts, SingleThreadContext sync)
    {
        string seed = opts.GetValueOrDefault("seed", "");
        if (string.IsNullOrWhiteSpace(seed)) throw new ArgumentException("spoils-map requires --seed");
        try
        {
            RunState run = Oracle.StartRun(new Dictionary<string, string>
            {
                ["seed"] = seed,
                ["character"] = opts.GetValueOrDefault("character", "IRONCLAD"),
                ["ascension"] = opts.GetValueOrDefault("ascension", "0")
            });
            var player = run.Players[0];
            SpoilsMap card = run.CreateCard<SpoilsMap>(player);
            Await(CardPileCmd.Add(card, PileType.Deck), sync);
            run.CurrentActIndex = 1;
            Await(RunManager.Instance.GenerateMap(), sync);
            Program.Out.WriteLine(Oracle.DumpMap(run.Map).ToJsonString());
        }
        finally
        {
            RunManager.Instance.CleanUp();
        }
    }

    private static void Await(System.Threading.Tasks.Task task, SingleThreadContext sync)
    {
        for (int i = 0; i < 100000 && !task.IsCompleted; i++) sync.Pump();
        task.GetAwaiter().GetResult();
    }
}
