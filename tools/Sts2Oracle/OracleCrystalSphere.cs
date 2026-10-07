using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Reflection;
using System.Text.Json.Nodes;
using System.Threading.Tasks;
using Godot;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Gold;
using MegaCrit.Sts2.Core.Events.Custom.CrystalSphereEvent;
using MegaCrit.Sts2.Core.Events.Custom.CrystalSphereEvent.CrystalSphereItems;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Models.Events;
using MegaCrit.Sts2.Core.Random;
using MegaCrit.Sts2.Core.Rewards;
using MegaCrit.Sts2.Core.Runs;

// Exercises the shipped CrystalSphere event and minigame directly. CellClicked,
// item placement, reveal callbacks and ToReward are game methods; this class
// only observes their state and serializes it for comparison with sts2sim.
internal static class OracleCrystalSphere
{
    private sealed record Click(CrystalSphereMinigame.CrystalSphereToolType Tool, int X, int Y);

    private static readonly FieldInfo CounterField = typeof(Rng).GetField("_counter", BindingFlags.Instance | BindingFlags.NonPublic)
        ?? throw new MissingFieldException(typeof(Rng).FullName, "_counter");

    public static void Run(Dictionary<string, string> opts, SingleThreadContext sync)
    {
        string[] seeds = Split(opts.GetValueOrDefault("seeds") ?? opts.GetValueOrDefault("seed", ""));
        if (seeds.Length == 0) throw new ArgumentException("crystal-sphere requires --seed or --seeds");
        string[] clicks = SplitClicks(opts.GetValueOrDefault("clicks", "B:5,5;S:0,0;B:10,10;S:10,0;B:5,10;S:0,10"));
        string choice = opts.GetValueOrDefault("choice", "payment-plan").ToLowerInvariant();
        string character = opts.GetValueOrDefault("character", "IRONCLAD").ToUpperInvariant();
        int ascension = int.Parse(opts.GetValueOrDefault("ascension", "0"), CultureInfo.InvariantCulture);
        int gold = int.Parse(opts.GetValueOrDefault("gold", "300"), CultureInfo.InvariantCulture);

        foreach (string seed in seeds)
        {
            try
            {
                RunOne(seed, character, ascension, gold, choice, clicks);
            }
            finally
            {
                RunManager.Instance.CleanUp();
            }
        }
        Program.Out.Flush();
    }

    private static void RunOne(string seed, string character, int ascension, int gold, string choice, string[] clickArgs)
    {
        var runOpts = new Dictionary<string, string>
        {
            ["seed"] = seed,
            ["character"] = character,
            ["ascension"] = ascension.ToString(CultureInfo.InvariantCulture)
        };
        RunState state = Oracle.StartRun(runOpts);
        // Crystal Sphere is an Act 2+ event. Its own event RNG is derived from
        // the run seed, player slot, and event ID by EventModel.BeginEvent.
        state.CurrentActIndex = Math.Min(1, state.Acts.Count - 1);
        var player = state.Players[0];
        PlayerCmd.SetGold(gold, player).GetAwaiter().GetResult();
        EventModel crystalSphere = ModelDb.Event<CrystalSphere>().ToMutable();
        bool eventAllowed = crystalSphere.IsAllowed(state);
        crystalSphere.BeginEvent(player, null, isPreFinished: false).GetAwaiter().GetResult();

        int cost = decimal.ToInt32(crystalSphere.DynamicVars["UncoverFutureCost"].BaseValue);
        Rng eventRng = crystalSphere.Rng;
        int afterCost = Counter(eventRng);
        int divinationCount = choice switch
        {
            "uncover-future" or "future" => crystalSphere.DynamicVars["UncoverFutureProphesizeCount"].IntValue,
            "payment-plan" or "payment" => crystalSphere.DynamicVars["PaymentPlanCount"].IntValue,
            _ => throw new ArgumentException($"choice must be uncover-future or payment-plan, got '{choice}'")
        };
        List<Click> parsedClicks = ParseClicks(clickArgs).ToList();
        if (parsedClicks.Count > divinationCount)
            throw new ArgumentException($"{choice} allows {divinationCount} cell clicks, got {parsedClicks.Count}");
        HashSet<CardModel> deckBeforeChoice = new(player.Deck.Cards, ReferenceEqualityComparer.Instance);
        int goldBeforeChoice = player.Gold;
        if (choice is "uncover-future" or "future")
            PlayerCmd.LoseGold(cost, player, GoldLossType.Spent).GetAwaiter().GetResult();
        else
            CardPileCmd.AddCurseToDeck<Debt>(player).GetAwaiter().GetResult();
        HashSet<CardModel> deckBeforeMinigame = new(player.Deck.Cards, ReferenceEqualityComparer.Instance);
        var game = new CrystalSphereMinigame(player, eventRng, divinationCount);
        int afterPlacement = Counter(eventRng);

        List<CrystalSphereItem> revealed = new();
        Dictionary<CrystalSphereItem, int> itemIndexes = new(ReferenceEqualityComparer.Instance);
        for (int i = 0; i < game.Items.Count; i++)
        {
            CrystalSphereItem item = game.Items[i];
            itemIndexes[item] = i;
            item.Revealed += revealed.Add;
        }

        var result = new JsonObject
        {
            ["event"] = "crystal_sphere",
            ["seed"] = seed,
            ["character"] = character,
            ["ascension"] = ascension,
            ["act_index"] = state.CurrentActIndex,
            ["event_id"] = crystalSphere.Id.Entry,
            ["event_allowed_at_entry"] = eventAllowed,
            ["gold_at_event_entry"] = gold,
            ["minigame_choice"] = choice,
            ["minigame_divination_count"] = divinationCount,
            ["gold_before_choice"] = goldBeforeChoice,
            ["gold_after_choice"] = player.Gold,
            ["cards_added_by_choice"] = DumpAddedCards(deckBeforeChoice, player.Deck.Cards),
            ["uncover_future_cost"] = cost,
            ["uncover_future_prophesize_count"] = crystalSphere.DynamicVars["UncoverFutureProphesizeCount"].IntValue,
            ["payment_plan_count"] = crystalSphere.DynamicVars["PaymentPlanCount"].IntValue,
            ["event_rng_counter_after_cost"] = afterCost,
            ["event_rng_counter_after_placement"] = afterPlacement,
            ["placed_all_items"] = game.PlacedAllItems,
            ["initial_grid"] = DumpGrid(game, itemIndexes),
            ["items"] = DumpItems(game),
            ["clicks"] = new JsonArray(),
            ["reveal_order"] = new JsonArray(),
            ["rewards"] = new JsonArray()
        };
        var clickResults = (JsonArray)result["clicks"]!;
        var revealOrder = (JsonArray)result["reveal_order"]!;
        foreach (Click click in parsedClicks)
        {
            int priorReveals = revealed.Count;
            game.SetTool(click.Tool);
            int counterBefore = Counter(eventRng);
            game.CellClicked(game.cells[click.X, click.Y]).GetAwaiter().GetResult();
            int counterAfter = Counter(eventRng);
            var revealedNow = new JsonArray();
            foreach (CrystalSphereItem item in revealed.Skip(priorReveals))
            {
                int index = itemIndexes[item];
                revealOrder.Add(index);
                revealedNow.Add(index);
            }
            clickResults.Add(new JsonObject
            {
                ["tool"] = click.Tool.ToString(),
                ["x"] = click.X,
                ["y"] = click.Y,
                ["divinations_remaining"] = game.DivinationCount,
                ["revealed_now"] = revealedNow,
                ["event_rng_counter_before"] = counterBefore,
                ["event_rng_counter_after"] = counterAfter,
                ["grid"] = DumpGrid(game, itemIndexes)
            });
        }

        // This is the same item-to-reward projection used by
        // OneOffSynchronizer.OfferCrystalSphereRewards. It does not invoke the
        // player's reward-selection UI, so the report describes the rewards
        // prepared by the game rather than a policy's later selection.
        int rewardCounterBefore = Counter(eventRng);
        var rewards = (JsonArray)result["rewards"]!;
        List<(CrystalSphereItem item, int index, Reward reward)> preparedRewards = new();
        foreach (CrystalSphereItem item in revealed)
        {
            Reward? reward = item.ToReward(player, eventRng);
            if (reward == null) continue;
            int index = itemIndexes[item];
            preparedRewards.Add((item, index, reward));
        }
        result["event_rng_counter_after_reward_construction"] = Counter(eventRng);
        foreach ((CrystalSphereItem item, int index, Reward reward) in preparedRewards)
        {
            reward.Populate();
            var rewardResult = new JsonObject
            {
                ["item_index"] = index,
                ["item"] = DumpItem(item, index),
                ["reward_type"] = reward.GetType().Name
            };
            switch (reward)
            {
                case PotionReward potion when potion.Potion != null:
                    rewardResult["potion_id"] = potion.Potion.Id.Entry;
                    break;
                case RelicReward relic when relic.Relic != null:
                    rewardResult["relic_id"] = relic.Relic.Id.Entry;
                    break;
                case GoldReward goldReward:
                    rewardResult["gold"] = goldReward.Amount;
                    break;
                case CardReward card:
                    rewardResult["card_ids"] = new JsonArray(card.Cards.Select(c => (JsonNode?)JsonValue.Create(c.Id.Entry)).ToArray());
                    break;
            }
            rewards.Add(rewardResult);
        }
        result["event_rng_counter_before_rewards"] = rewardCounterBefore;
        result["event_rng_counter_after_rewards"] = Counter(eventRng);
        result["cards_added_by_reveal"] = DumpAddedCards(deckBeforeMinigame, player.Deck.Cards);
        result["reward_selection_performed"] = false;
        result["reward_population_performed"] = true;
        Program.Out.WriteLine(result.ToJsonString());
    }

    private static JsonObject DumpGrid(CrystalSphereMinigame game, Dictionary<CrystalSphereItem, int> indexes)
    {
        string[] hiddenRows = new string[game.cells.GetLength(1)];
        string[] itemRows = new string[game.cells.GetLength(1)];
        for (int y = 0; y < game.cells.GetLength(1); y++)
        {
            char[] hidden = new char[game.cells.GetLength(0)];
            char[] items = new char[game.cells.GetLength(0)];
            for (int x = 0; x < game.cells.GetLength(0); x++)
            {
                CrystalSphereCell cell = game.cells[x, y];
                hidden[x] = cell.IsHidden ? 'H' : '.';
                items[x] = cell.Item == null ? '.' : ItemGlyph(indexes[cell.Item]);
            }
            hiddenRows[y] = new string(hidden);
            itemRows[y] = new string(items);
        }
        return new JsonObject
        {
            ["width"] = game.cells.GetLength(0),
            ["height"] = game.cells.GetLength(1),
            ["hidden_rows_y0_to_y10"] = new JsonArray(hiddenRows.Select(s => (JsonNode?)JsonValue.Create(s)).ToArray()),
            ["item_index_rows_y0_to_y10"] = new JsonArray(itemRows.Select(s => (JsonNode?)JsonValue.Create(s)).ToArray())
        };
    }

    private static JsonArray DumpItems(CrystalSphereMinigame game)
    {
        var result = new JsonArray();
        for (int i = 0; i < game.Items.Count; i++) result.Add(DumpItem(game.Items[i], i));
        return result;
    }

    private static JsonArray DumpAddedCards(HashSet<CardModel> before, IEnumerable<CardModel> after)
    {
        var result = new JsonArray();
        foreach (CardModel card in after)
            if (!before.Contains(card)) result.Add(card.Id.Entry);
        return result;
    }

    private static JsonObject DumpItem(CrystalSphereItem item, int index)
    {
        SerializableCrystalSphereItem serializable = item.ToSerializable();
        Vector2I position = item.Position;
        Vector2I size = item.Size;
        var result = new JsonObject
        {
            ["index"] = index,
            ["kind"] = serializable.type.ToString(),
            ["class"] = item.GetType().Name,
            ["x"] = position.X,
            ["y"] = position.Y,
            ["width"] = size.X,
            ["height"] = size.Y
        };
        if (serializable.type == CrystalSphereItemType.CardReward)
            result["card_rarity"] = serializable.cardRarity.ToString();
        if (serializable.type == CrystalSphereItemType.Potion)
            result["potion_rarity"] = serializable.potionRarity.ToString();
        if (serializable.type == CrystalSphereItemType.Gold)
        {
            result["is_big_gold"] = serializable.isBigGold;
            result["gold"] = serializable.isBigGold ? 30 : 10;
        }
        return result;
    }

    private static int Counter(Rng rng) => (int)CounterField.GetValue(rng)!;

    private static char ItemGlyph(int index) => index < 10 ? (char)('0' + index) : (char)('A' + index - 10);

    private static string[] Split(string value) => value.Split(',', StringSplitOptions.RemoveEmptyEntries);

    private static string[] SplitClicks(string value) => value.Split(';', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);

    private static IEnumerable<Click> ParseClicks(IEnumerable<string> values)
    {
        foreach (string value in values)
        {
            string[] parts = value.Split(':', StringSplitOptions.TrimEntries);
            if (parts.Length != 2) throw new ArgumentException($"invalid click '{value}', expected B:x,y or S:x,y");
            CrystalSphereMinigame.CrystalSphereToolType tool = parts[0].ToUpperInvariant() switch
            {
                "B" or "BIG" => CrystalSphereMinigame.CrystalSphereToolType.Big,
                "S" or "SMALL" => CrystalSphereMinigame.CrystalSphereToolType.Small,
                _ => throw new ArgumentException($"invalid Crystal Sphere tool '{parts[0]}'")
            };
            string[] coord = parts[1].Split(',', StringSplitOptions.TrimEntries);
            if (coord.Length != 2) throw new ArgumentException($"invalid click coordinate '{parts[1]}', expected x,y");
            int x = int.Parse(coord[0], CultureInfo.InvariantCulture);
            int y = int.Parse(coord[1], CultureInfo.InvariantCulture);
            if (x < 0 || x >= 11 || y < 0 || y >= 11)
                throw new ArgumentOutOfRangeException(nameof(values), $"Crystal Sphere coordinate {x},{y} is outside the 11x11 grid");
            yield return new Click(tool, x, y);
        }
    }
}
