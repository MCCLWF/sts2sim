using System.Runtime.CompilerServices;
using System.Text.Json;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.GameActions.Multiplayer;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Potions;
using MegaCrit.Sts2.Core.Models.Relics;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Unlocks;

internal static class Program
{
    private static void Main()
    {
        GameBootstrap.InstallResolver(GameBootstrap.DefaultGameData);
        Check();
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static void Check()
    {
        GameBootstrap.Init(testMode: false);
        NonInteractiveMode.AutoSlayerCheck = () => true;
        GameBootstrap.UseInMemorySaves();
        GameBootstrap.LoadLocalization();
        var rows = new List<object>();
        foreach (bool sozu in new[] { false, true })
        foreach (bool occupied in new[] { false, true })
        {
            Player player = Player.CreateForNewRun<Silent>(UnlockState.all, 1);
            RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(acts => acts[0].ToMutable()).ToArray(),
                [], GameMode.Standard, 10, "ENTROPICSOZU");
            RunManager.Instance.SetUpNewSingleplayer(run, shouldSave: false);
            RunManager.Instance.Launch();
            PotionModel brew = ModelDb.Potion<EntropicBrew>().ToMutable();
            player.AddPotionInternal(brew, silent: true);
            PotionModel? retained = occupied ? ModelDb.Potion<FirePotion>().ToMutable() : null;
            if (retained != null) player.AddPotionInternal(retained, silent: true);
            if (sozu) player.AddRelicInternal(ModelDb.Relic<Sozu>().ToMutable(), silent: true);
            int before = run.Rng.CombatPotionGeneration.ToSerializable().counter;
            // Shipped OnUseWrapper and the shipped EntropicBrew.OnUse, no rules patches.
            brew.OnUseWrapper(new ThrowingPlayerChoiceContext(), player.Creature).GetAwaiter().GetResult();
            int draws = run.Rng.CombatPotionGeneration.ToSerializable().counter - before;
            if (draws != 2 * (sozu ? 1 : player.MaxPotionCount - (occupied ? 1 : 0)))
                throw new InvalidOperationException("unexpected native RNG count");
            if (player.PotionSlots.Contains(brew) || (occupied && player.PotionSlots[1] != retained) ||
                (sozu ? player.Potions.Count() != (occupied ? 1 : 0) : player.HasOpenPotionSlots))
                throw new InvalidOperationException("unexpected native potion slots");
            rows.Add(new { sozu, occupied, draws, slots = player.PotionSlots.Select(p => p?.Id.Entry).ToArray() });
            RunManager.Instance.CleanUp();
        }
        Console.WriteLine(JsonSerializer.Serialize(new { game_version = "v0.111.0", test_mode = false,
            scope = "shipped Entropic Brew wrapper, Sozu refusal, slots and RNG outside combat", rows }));
    }
}
