using System.Reflection;
using System.Runtime.CompilerServices;
using System.Text.Json;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Factories;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Map;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Relics;
using MegaCrit.Sts2.Core.Rewards;
using MegaCrit.Sts2.Core.Rooms;
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
        Player player = Player.CreateForNewRun<Regent>(UnlockState.all, 1);
        RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(acts => acts[0].ToMutable()).ToArray(),
            [], GameMode.Standard, 10, "CANDYOPTIONS");
        RunManager.Instance.SetUpNewSingleplayer(run, shouldSave: false);
        run.AppendToMapPointHistory(MapPointType.Monster, RoomType.Monster, null);

        var rug = ModelDb.Relic<DingyRug>().ToMutable();
        player.AddRelicInternal(rug, silent: true);
        var creation = CardCreationOptions.ForRoom(player, RoomType.Monster).WithFlags(CardCreationFlags.IsFromCombat);
        var reward = new CardReward(creation, 3, player);
        reward.Populate();
        const BindingFlags Private = BindingFlags.Instance | BindingFlags.NonPublic;
        CardCreationOptions options = (CardCreationOptions)typeof(CardReward).GetProperty("Options", Private)!.GetValue(reward)!;
        CardCreationOptions reroll = (CardCreationOptions)typeof(CardReward).GetProperty("RerollOptions", Private)!.GetValue(reward)!;
        string[] Pools(CardCreationOptions value) => value.CardPools.Select(p => p.Id.Entry).ToArray();
        string[] initialCards = reward.Cards.Select(c => c.Id.Entry).ToArray();
        bool aliases = ReferenceEquals(options, reroll) && ReferenceEquals(options, creation);
        string[] initialPools = Pools(options);
        player.RemoveRelicInternal(rug, silent: true);
        reward.Reroll();
        var alias = new { aliases, initial_pools = initialPools, reroll_pools = Pools(reroll),
            initial_cards = initialCards, reroll_cards = reward.Cards.Select(c => c.Id.Entry).ToArray() };

        var candy = (LastingCandy)ModelDb.Relic<LastingCandy>().ToMutable();
        player.AddRelicInternal(candy, silent: true);
        candy.CombatRewardsSeen = 1;
        var parent = CardCreationOptions.ForRoom(player, RoomType.Monster)
            .WithFlags(CardCreationFlags.IsCardReward | CardCreationFlags.IsFromCombat | CardCreationFlags.NoUpgradeRoll);
        var cards = new List<CardCreationResult>();
        int before = player.PlayerRng.Rewards.ToSerializable().counter;
        bool modified = candy.TryModifyCardRewardOptions(player, cards, parent);
        var flags = new { modified, parent_flags = parent.Flags.ToString(),
            rewards_draws = player.PlayerRng.Rewards.ToSerializable().counter - before,
            cards = cards.Select(c => c.Card.Id.Entry).ToArray() };
        Console.WriteLine(JsonSerializer.Serialize(new { game_version = "v0.111.0", test_mode = false,
            scope = "native reward options alias/reroll and LastingCandy primitive", alias, flags }));
        RunManager.Instance.CleanUp();
    }
}
