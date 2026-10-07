using System.Runtime.CompilerServices;
using System.Text.Json;
using MegaCrit.Sts2.Core.Entities.CardRewardAlternatives;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Map;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Rewards;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.TestSupport;
using MegaCrit.Sts2.Core.Unlocks;

// Narrow shipped-DLL primitive, with TestMode=false. This observes the exact
// alternative list that CardReward.Populate passes to the live screen's
// RefreshOptions; it does not fabricate a Godot screen or run a UI simulator.
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
        int cases = 0;
        foreach (string seed in new[] { "RFUI0001", "RFUI0002", "RFUI0003", "RFUI0004" })
        foreach (CharacterModel character in ModelDb.AllCharacters.Where(c => c.Id.Entry is "IRONCLAD" or "SILENT" or "REGENT" or "NECROBINDER" or "DEFECT"))
        {
            try
            {
                Player player = Player.CreateForNewRun(character, UnlockState.all, 1);
                RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(a => a[0].ToMutable()).ToArray(),
                    [], GameMode.Standard, 0, seed);
                RunManager.Instance.SetUpNewSingleplayer(run, shouldSave: false);
                run.AppendToMapPointHistory(MapPointType.Monster, RoomType.Monster, null);
                var reward = new CardReward(CardCreationOptions.ForRoom(player, RoomType.Monster), 3, player) { CanReroll = true };
                reward.Populate();
                var before = new
                {
                    can_reroll = reward.CanReroll,
                    cards = reward.Cards.Select(c => c.Id.Entry).ToArray(),
                    alternatives = CardRewardAlternative.Generate(reward).Select(a => a.OptionId).ToArray(),
                    rng = player.PlayerRng.Rewards.ToSerializable()
                };
                reward.Reroll();
                var after = new
                {
                    can_reroll = reward.CanReroll,
                    cards = reward.Cards.Select(c => c.Id.Entry).ToArray(),
                    alternatives = CardRewardAlternative.Generate(reward).Select(a => a.OptionId).ToArray(),
                    rng = player.PlayerRng.Rewards.ToSerializable()
                };
                if (!before.alternatives.SequenceEqual(new[] { "Skip", "REROLL" })
                    || !after.alternatives.SequenceEqual(new[] { "Skip" }) || after.can_reroll || TestMode.IsOn)
                    throw new InvalidOperationException("Native reward UI alternative contract changed");
                Console.WriteLine(JsonSerializer.Serialize(new { seed, character = character.Id.Entry, test_mode = TestMode.IsOn,
                    scope = "native CardReward.Reroll / Generate alternatives supplied to RefreshOptions; no live screen capture", before, after },
                    new JsonSerializerOptions { IncludeFields = true }));
                ++cases;
            }
            finally { RunManager.Instance.CleanUp(); }
        }
        if (cases != 20) throw new InvalidOperationException($"Expected 20 native cases, executed {cases}");
    }
}
