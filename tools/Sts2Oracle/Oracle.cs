using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Map;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Random;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Saves;
using MegaCrit.Sts2.Core.Unlocks;

internal static class Oracle
{
    public static int Run(string cmd, Dictionary<string, string> opts)
    {
        SingleThreadContext sync = SingleThreadContext.Install();
        string envTestMode = opts.GetValueOrDefault("oracle-test-mode", "on");
        if (opts.ContainsKey("oracle-test-mode") && (cmd is not ("run" or "env-operation" or "env-crystal-sphere") || envTestMode is not ("on" or "off")))
            throw new ArgumentException("--oracle-test-mode on|off is only supported by run or Env operation probes");
        GameBootstrap.Init(testMode: cmd != "trial-confirmation" && envTestMode == "on");
        GameBootstrap.UseInMemorySaves();
        GameBootstrap.LoadLocalization();
        switch (cmd)
        {
            case "trial-confirmation":
                MegaCrit.Sts2.Core.Helpers.NonInteractiveMode.AutoSlayerCheck = () => true;
                Action restoreTrialRooms = Headless.InstallEnvOperationPresentation();
                Action restoreTrialDeath = Headless.InstallAbandonPresentation();
                try { OracleEnvOperationProbe.RunTrialConfirmation(opts, sync); }
                finally { restoreTrialDeath(); restoreTrialRooms(); }
                Program.Out.Flush();
                return 0;
            case "env-operation":
            case "env-crystal-sphere":
                Action? restorePresentation = null;
                Action? restoreRoomPresentation = null;
                Action? restoreRewardsPresentation = null;
                try
                {
                    if (envTestMode == "off")
                    {
                        MegaCrit.Sts2.Core.Helpers.NonInteractiveMode.AutoSlayerCheck = () => true;
                        restorePresentation = Headless.InstallLiveCombatPresentation();
                        restoreRoomPresentation = Headless.InstallEnvOperationPresentation();
                        restoreRewardsPresentation = Headless.InstallEnvRewardsPresentation();
                    }
                    if (cmd == "env-crystal-sphere") OracleEnvOperationProbe.RunCrystalSphere(opts, sync);
                    else OracleEnvOperationProbe.Run(opts, sync);
                }
                finally { restoreRewardsPresentation?.Invoke(); restoreRoomPresentation?.Invoke(); restorePresentation?.Invoke(); }
                Program.Out.Flush();
                return 0;
            case "combat":
                if (opts.ContainsKey("rng-ledger")) OracleRngLedger.Install();
                OracleCombat.Run(opts, sync);
                Program.Out.Flush();
                return 0;
            case "run":
                if (opts.ContainsKey("rng-ledger")) OracleRngLedger.Install();
                Action? restoreRunVisualReads = null;
                Action? restoreRunRooms = null;
                Action? restoreRunRewards = null;
                Action? restoreRunDeath = null;
                Action? restoreRunCombat = null;
                try
                {
                    if (envTestMode == "off")
                    {
                        MegaCrit.Sts2.Core.Helpers.NonInteractiveMode.AutoSlayerCheck = () => true;
                        restoreRunRooms = Headless.InstallEnvOperationPresentation();
                        restoreRunRewards = Headless.InstallEnvRewardsPresentation();
                        restoreRunDeath = Headless.InstallAbandonPresentation();
                        restoreRunCombat = Headless.InstallLiveCombatPresentation();
                        // Install factories before patching callers: Harmony's
                        // caller recompilation can inline an unpatched NCard.Create.
                        restoreRunVisualReads = Headless.InstallProductionVisualReads();
                    }
                    OracleRun.Run(opts, sync);
                }
                finally { restoreRunCombat?.Invoke(); restoreRunDeath?.Invoke(); restoreRunRewards?.Invoke(); restoreRunRooms?.Invoke(); restoreRunVisualReads?.Invoke(); }
                Program.Out.Flush();
                return 0;
            case "crystal-sphere":
                OracleCrystalSphere.Run(opts, sync);
                Program.Out.Flush();
                return 0;
            case "fake-merchant":
                OracleFakeMerchant.Run(opts);
                Program.Out.Flush();
                return 0;
            case "orobas-enchanted":
                OracleOrobasEnchanted.Run(opts);
                Program.Out.Flush();
                return 0;
            case "paels-growth":
                OraclePaelsGrowth.Run(opts);
                Program.Out.Flush();
                return 0;
            case "paels-tooth":
                OraclePaelsTooth.Run(opts);
                Program.Out.Flush();
                return 0;
            case "spoils-map":
                OracleSpoilsMap.Run(opts, sync);
                Program.Out.Flush();
                return 0;
            case "newrun":
            {
                // Batch form: --seeds a,b --characters x,y --ascensions 0,10 (JSON lines).
                string[] seeds = Split(opts.GetValueOrDefault("seeds") ?? opts.GetValueOrDefault("seed", ""));
                string[] chars = Split(opts.GetValueOrDefault("characters") ?? opts.GetValueOrDefault("character", "IRONCLAD"));
                string[] ascs = Split(opts.GetValueOrDefault("ascensions") ?? opts.GetValueOrDefault("ascension", "0"));
                foreach (string seed in seeds)
                foreach (string ch in chars)
                foreach (string asc in ascs)
                {
                    var one = new Dictionary<string, string> { ["seed"] = seed, ["character"] = ch, ["ascension"] = asc };
                    Program.Out.WriteLine(NewRun(one).ToJsonString());
                    RunManager.Instance.CleanUp();
                }
                Program.Out.Flush();
                return 0;
            }
            default:
                Console.Error.WriteLine("usage: Sts2Oracle run --seeds S1,S2 --characters IRONCLAD --policy-seeds 1,2 [--ascension N]");
                return 2;
        }
    }

    private static string[] Split(string s) => s.Split(',', StringSplitOptions.RemoveEmptyEntries);

    // StartRunLobby.BeginRunLocally + NGame.StartNewSingleplayerRun + NGame.StartRun,
    // for a profile with every epoch revealed and every act discovered.
    // New run through the real setup path; leaves RunManager holding it.
    public static RunState StartRun(Dictionary<string, string> opts)
    {
        string seed = SeedHelper.CanonicalizeSeed(opts["seed"]);
        string characterEntry = opts.GetValueOrDefault("character", "IRONCLAD").ToUpperInvariant();
        int ascension = int.Parse(opts.GetValueOrDefault("ascension", "0"));
        CharacterModel character = ModelDb.GetById<CharacterModel>(new ModelId("CHARACTER", characterEntry));
        UnlockState unlocks = UnlockState.all;

        // ActModel.GetRandomList, minus the "undiscovered act" override (all acts discovered).
        var actRng = new Rng(StringHelper.GetDeterministicHashCode(seed), "act_selection");
        var acts = new List<ActModel>();
        foreach (IReadOnlyList<ActModel> candidates in ModelDb.ActsByIndex)
        {
            acts.Add(actRng.NextItem(candidates.Where(a => a.IsUnlocked(unlocks)).ToList())!);
        }

        Player player = Player.CreateForNewRun(character, unlocks, 1uL);
        RunState state = RunState.CreateForNewRun(new[] { player }, acts.Select(a => a.ToMutable()).ToList(),
            Array.Empty<ModifierModel>(), GameMode.Standard, ascension, seed);
        RunManager.Instance.SetUpNewSingleplayer(state, shouldSave: false);
        RunManager.Instance.FinalizeStartingRelics().GetAwaiter().GetResult();
        RunManager.Instance.Launch();
        return state;
    }

    private static JsonObject NewRun(Dictionary<string, string> opts)
    {
        RunState state = StartRun(opts);
        string seed = SeedHelper.CanonicalizeSeed(opts["seed"]);
        string characterEntry = opts.GetValueOrDefault("character", "IRONCLAD").ToUpperInvariant();
        int ascension = int.Parse(opts.GetValueOrDefault("ascension", "0"));
        SerializableRun save = RunManager.Instance.ToSave(null);
        save.SaveTime = 0;  // wall-clock fields; everything else is deterministic
        save.StartTime = 0;
        var result = new JsonObject
        {
            ["seed"] = seed,
            ["character"] = characterEntry,
            ["ascension"] = ascension,
            ["save"] = JsonNode.Parse(JsonSerializationUtility.ToJson(save)),
        };

        var maps = new JsonArray();
        for (int i = 0; i < state.Acts.Count; i++)
        {
            state.CurrentActIndex = i;
            maps.Add(DumpMap(StandardActMap.CreateFor(state, replaceTreasureWithElites: false)));
        }
        state.CurrentActIndex = 0;
        result["maps"] = maps;
        return result;
    }

    private static JsonArray Coord(MapCoord c) => new JsonArray(c.col, c.row);

    internal static JsonObject DumpMap(ActMap map)
    {
        var points = new JsonArray();
        foreach (MapPoint p in map.GetAllMapPoints().OrderBy(p => p.coord.row).ThenBy(p => p.coord.col))
        {
            points.Add(new JsonObject
            {
                ["coord"] = Coord(p.coord),
                ["type"] = p.PointType.ToString(),
                ["spoils_map_quest"] = p.Quests.Any(q => q is MegaCrit.Sts2.Core.Models.Cards.SpoilsMap),
                ["children"] = new JsonArray(p.Children.OrderBy(c => c.coord.row).ThenBy(c => c.coord.col)
                    .Select(c => (JsonNode)Coord(c.coord)).ToArray()),
            });
        }
        return new JsonObject
        {
            ["cols"] = map.GetColumnCount(),
            ["rows"] = map.GetRowCount(),
            ["start"] = Coord(map.StartingMapPoint.coord),
            ["boss"] = Coord(map.BossMapPoint.coord),
            ["second_boss"] = map.SecondBossMapPoint == null ? null : Coord(map.SecondBossMapPoint.coord),
            ["starts"] = new JsonArray(map.startMapPoints.OrderBy(p => p.coord.col)
                .Select(p => (JsonNode)Coord(p.coord)).ToArray()),
            ["points"] = points,
        };
    }
}
