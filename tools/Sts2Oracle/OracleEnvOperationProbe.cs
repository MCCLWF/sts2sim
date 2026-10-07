using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Entities.CardRewardAlternatives;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Merchant;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Entities.Rngs;
using MegaCrit.Sts2.Core.Localization.DynamicVars;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Map;
using MegaCrit.Sts2.Core.Random;
using MegaCrit.Sts2.Core.Rewards;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.TestSupport;

// Observes shipped RelicCmd.Obtain and CardCmd.Transform paths. Selection policy is
// deliberately simple; reward generation, transformation, upgrades and RNG are
// all executed by sts2.dll. The normal StartRun + EnterAct fixture is the same
// unchosen Neow event reached by Env.Reset, not the earlier setup-only stage.
internal static partial class OracleEnvOperationProbe
{
    private sealed record OperationCase(string Id, string Kind, string? RelicId,
        string? RngSource, string? Mode, int[] DeckIndices, int[]? PurchaseIndices = null)
    {
        public JsonObject Parameters() => Kind == "obtain_relic"
            ? new JsonObject { ["relic_id"] = RelicId, ["insert_index"] = -1 }
            : Kind == "merchant_purchase" ? new JsonObject
            {
                ["entry_indices"] = new JsonArray(PurchaseIndices!.Select(i => (JsonNode?)JsonValue.Create(i)).ToArray()),
                ["ignore_cost"] = false
            } : new JsonObject
            {
                ["deck_indices"] = new JsonArray(DeckIndices.Select(i => (JsonNode?)JsonValue.Create(i)).ToArray()),
                ["rng_source"] = RngSource, ["mode"] = Mode
            };

        public async Task<JsonArray?> Execute(RunState run, Player player, Probe probe)
        {
            if (Kind == "obtain_relic")
            {
                await RelicCmd.Obtain(ModelDb.GetById<RelicModel>(new ModelId("RELIC", RelicId!)).ToMutable(), player);
                return null;
            }
            if (Kind == "merchant_purchase")
            {
                var room = run.CurrentRoom as MerchantRoom ?? throw new InvalidOperationException("Expected merchant fixture");
                MerchantInventory inventory = room.Inventories[0];
                var steps = new JsonArray();
                foreach (int index in PurchaseIndices!)
                {
                    MerchantEntry entry = inventory.AllEntries.ElementAt(index);
                    int firstChoice = probe.Choices.Count;
                    JsonObject before = State(run, player);
                    bool success;
                    if (entry is MerchantCardRemovalEntry removal)
                    {
                        success = await removal.OnTryPurchaseWrapper(inventory);
                        // NMerchantInventory.OnCardRemovalUsed marks this used
                        // in the live screen; headless has no inventory node.
                        if (success) removal.SetUsed();
                    }
                    else success = await entry.OnTryPurchaseWrapper(inventory);
                    steps.Add(new JsonObject
                    {
                        ["entry_index"] = index, ["success"] = success, ["before"] = before,
                        ["choices"] = new JsonArray(probe.Choices.Skip(firstChoice).Select(c => c?.DeepClone()).ToArray()),
                        ["after"] = State(run, player)
                    });
                }
                return steps;
            }
            // Resolve every original instance before transforming anything;
            // sequential calls must not rebind later indices to the new deck.
            CardModel[] cards = DeckIndices.Select(i => player.Deck.Cards[i]).ToArray();
            Rng rng = RngSource switch
            {
                "player_transformations" => player.PlayerRng.Transformations,
                "run_niche" => run.Rng.Niche,
                "event" => run.CurrentRoom is EventRoom eventRoom ? eventRoom.LocalMutableEvent.Rng
                    : throw new InvalidOperationException("Event RNG requires the active event fixture"),
                _ => throw new ArgumentException("Unsupported fixture RNG source")
            };
            if (Mode == "sequential")
                foreach (CardModel card in cards) await CardCmd.TransformToRandom(card, rng);
            else
                await CardCmd.Transform(cards.Select(c => new CardTransformation(c)), rng);
            return null;
        }
    }

    private static OperationCase[] Cases(Dictionary<string, string> opts)
    {
        string operation = opts.GetValueOrDefault("operation", "obtain_relic");
        if (operation == "merchant_purchase")
        {
            if (new[] { "relics", "rng-sources", "modes", "card-orders" }.Any(opts.ContainsKey))
                throw new ArgumentException("Merchant cases accept only --purchase-orders");
            string[] purchaseOrders = opts.GetValueOrDefault("purchase-orders", "0;5;7;10;13;0,0;5,5;7,7;10,10,10,10;13,13").Split(';');
            return purchaseOrders.Select(order =>
            {
                int[] indices = order.Split(',').Select(int.Parse).ToArray();
                if (indices.Any(i => i < 0 || i > 13)) throw new ArgumentException("Shop index must be 0..13");
                return new OperationCase($"purchase:{order}", operation, null, null, null, Array.Empty<int>(), indices);
            }).ToArray();
        }
        if (opts.ContainsKey("purchase-orders") || opts.ContainsKey("gold"))
            throw new ArgumentException("Merchant arguments require --operation merchant_purchase");
        if (operation == "obtain_relic")
        {
            if (opts.ContainsKey("rng-sources") || opts.ContainsKey("modes") || opts.ContainsKey("card-orders"))
                throw new ArgumentException("Transform arguments require --operation transform_cards");
            return Split(opts.GetValueOrDefault("relics", "CALLING_BELL,CAULDRON,ASTROLABE,CLAWS"))
                .Select(r => new OperationCase(r, operation, r, null, null, Array.Empty<int>())).ToArray();
        }
        if (operation != "transform_cards" || opts.ContainsKey("relics"))
            throw new ArgumentException("Choose obtain_relic, transform_cards or merchant_purchase; --relics is relic-only");
        string[] rngs = Split(opts.GetValueOrDefault("rng-sources", "player_transformations,run_niche,event"));
        string[] modes = Split(opts.GetValueOrDefault("modes", "batch,sequential"));
        string[] orders = opts.GetValueOrDefault("card-orders", "0;2,0,1;9,1,0").Split(';');
        if (rngs.Any(r => r is not ("player_transformations" or "run_niche" or "event"))
            || modes.Any(m => m is not ("batch" or "sequential")))
            throw new ArgumentException("Unsupported transform RNG or mode");
        return (from rng in rngs from mode in modes from order in orders
                let indices = order.Split(',').Select(int.Parse).ToArray()
                select new OperationCase($"transform:{rng}:{mode}:{order}", operation, null, rng, mode, indices)).ToArray();
    }

    private sealed class Probe : ICardSelector
    {
        private readonly RunState _run;
        private readonly Player _player;
        private readonly bool _rerollOnce;
        private readonly HashSet<Reward> _rerolled = new();
        private Reward? _activeReward;
        public JsonArray Choices { get; } = new();

        public Probe(RunState run, bool rerollOnce) { _run = run; _player = run.Players[0]; _rerollOnce = rerollOnce; }

        public Task<IEnumerable<CardModel>> GetSelectedCards(IEnumerable<CardModel> options, int minSelect, int maxSelect)
        {
            List<CardModel> cards = options.ToList();
            List<CardModel> selected = cards.Take(maxSelect).ToList();
            Choices.Add(new JsonObject
            {
                ["kind"] = "card_select", ["min_select"] = minSelect, ["max_select"] = maxSelect,
                ["options"] = Cards(cards),
                ["selected_indices"] = new JsonArray(selected.Select(c => (JsonNode?)JsonValue.Create(cards.IndexOf(c))).ToArray()),
                ["state"] = State(_run, _player)
            });
            return Task.FromResult<IEnumerable<CardModel>>(selected);
        }

        public CardRewardSelection GetSelectedCardReward(IReadOnlyList<CardCreationResult> options,
            IReadOnlyList<CardRewardAlternative> alternatives)
        {
            if (_activeReward is not CardReward reward)
                throw new InvalidOperationException("Card reward selection has no source reward instance");
            bool[] enabled = alternatives.Select(a => a.OptionId != "REROLL" || reward.CanReroll).ToArray();
            int reroll = _rerollOnce && !_rerolled.Contains(reward)
                ? Enumerable.Range(0, alternatives.Count).FirstOrDefault(i => alternatives[i].OptionId == "REROLL" && enabled[i], -1)
                : -1;
            if (reroll >= 0) _rerolled.Add(reward);
            int selectedIndex = reroll >= 0 ? options.Count + reroll : options.Count > 0 ? 0 : -1;
            Choices.Add(new JsonObject
            {
                ["kind"] = "card_reward", ["options"] = Cards(options.Select(c => c.Card)),
                ["alternatives"] = new JsonArray(alternatives.Select(a => (JsonNode?)JsonValue.Create(a.OptionId)).ToArray()),
                ["enabled_alternatives"] = new JsonArray(enabled.Select(e => (JsonNode?)JsonValue.Create(e)).ToArray()),
                ["selected_index"] = selectedIndex, ["state"] = State(_run, _player)
            });
            return reroll >= 0 ? new CardRewardSelection { alternative = alternatives[reroll] }
                : new CardRewardSelection { card = options.FirstOrDefault()?.Card };
        }

        public async Task SelectRewards(RewardsSet set)
        {
            // Custom sets close as soon as their last reward is taken. Recording
            // no final Proceed here makes an extra simulator choice a failure.
            while (!set.AllRewardsSuccessfullySelected)
            {
                // RewardsSet retains claimed objects; the screen removes their
                // buttons. Export backend indices and selection flags so the
                // consumer cannot silently renumber another reward instance.
                List<Reward> rewards = set.Rewards.ToList();
                int index = rewards.FindIndex(r => !r.SuccessfullySelected
                    && (r is not PotionReward || set.Player.PotionSlots.Any(p => p == null)));
                bool proceed = index < 0;
                Choices.Add(new JsonObject
                {
                    ["kind"] = "rewards", ["options"] = new JsonArray(rewards.Select(Reward).ToArray()),
                    ["can_proceed"] = !set.DisallowSkipping,
                    ["selected_index"] = proceed ? rewards.Count : index,
                    ["state"] = State(_run, _player)
                });
                if (proceed)
                {
                    if (set.DisallowSkipping) throw new InvalidOperationException("No claimable reward in a mandatory set");
                    RunManager.Instance.RewardsSetSynchronizer.SkipLocalRewardsSet();
                    return;
                }
                Reward? previous = _activeReward;
                _activeReward = rewards[index];
                try
                {
                    if (!await RunManager.Instance.RewardsSetSynchronizer.SelectLocalReward(rewards[index]))
                        throw new InvalidOperationException("The probe's selected reward was not claimable");
                }
                finally { _activeReward = previous; }
            }
        }
    }

    public static void Run(Dictionary<string, string> opts, SingleThreadContext sync)
    {
        string[] seeds = Split(opts.GetValueOrDefault("seeds") ?? opts.GetValueOrDefault("seed", ""));
        OperationCase[] cases = Cases(opts);
        string[] setupRelics = opts.ContainsKey("setup-relics") ? Split(opts["setup-relics"]) : Array.Empty<string>();
        string rewardPolicy = opts.GetValueOrDefault("card-reward-policy", "first");
        if (rewardPolicy is not ("first" or "reroll_once")) throw new ArgumentException("Unknown card reward policy");
        if (seeds.Length == 0) throw new ArgumentException("env-operation requires --seed or --seeds");
        if (seeds.Distinct().Count() != seeds.Length || cases.Length == 0 || cases.Select(c => c.Id).Distinct().Count() != cases.Length)
            throw new ArgumentException("Seeds and operation cases must be nonempty and unique");
        foreach (string seed in seeds)
        foreach (OperationCase operation in cases)
        {
            Func<RewardsSet, Task>? previous = RewardsSet.testSelector;
            try
            {
                RunState run = Oracle.StartRun(new Dictionary<string, string>
                {
                    ["seed"] = seed, ["character"] = opts.GetValueOrDefault("character", "IRONCLAD"),
                    ["ascension"] = opts.GetValueOrDefault("ascension", "0")
                });
                bool merchantFixture = operation.Kind == "merchant_purchase";
                if (merchantFixture) run.ExtraFields.StartedWithNeow = false;
                Await(RunManager.Instance.EnterAct(0), sync);
                Player player = run.Players[0];
                if (!merchantFixture && (run.CurrentRoom is not EventRoom room || room.LocalMutableEvent.Id.Entry != "NEOW"))
                    throw new InvalidOperationException("Expected the natural unchosen Neow event fixture");
                if (operation.DeckIndices.Any(i => i < 0 || i >= player.Deck.Cards.Count)
                    || operation.DeckIndices.Distinct().Count() != operation.DeckIndices.Length)
                    throw new ArgumentException("Transform indices must be distinct live deck instances");
                var probe = new Probe(run, rewardPolicy == "reroll_once");
                RewardsSet.testSelector = probe.SelectRewards;
                JsonObject initial = State(run, player);
                var setup = new JsonArray();
                using (CardSelectCmd.UseSelector(probe))
                foreach (string relic in setupRelics)
                {
                    JsonObject before = State(run, player);
                    Await(RelicCmd.Obtain(ModelDb.GetById<RelicModel>(new ModelId("RELIC", relic)).ToMutable(), player), sync);
                    if (probe.Choices.Count != 0) throw new ArgumentException("Setup relics with choices require a separate operation fixture");
                    setup.Add(new JsonObject { ["relic_id"] = relic, ["before"] = before, ["after"] = State(run, player) });
                }
                int gold = int.Parse(opts.GetValueOrDefault("gold", "10000"));
                if (merchantFixture)
                {
                    if (gold < 0) throw new ArgumentException("Fixture gold must be nonnegative");
                    Await(PlayerCmd.SetGold(gold, player), sync);
                    MapPoint point = run.Map.StartingMapPoint;
                    point.PointType = MapPointType.Shop;
                    Await(RunManager.Instance.EnterMapCoord(point.coord), sync);
                    if (run.CurrentRoom is not MerchantRoom) throw new InvalidOperationException("Expected injected merchant room");
                }
                var result = new JsonObject
                {
                    ["format"] = "sts2oracle.env_operation", ["version"] = merchantFixture ? 3 : 2,
                    ["seed"] = seed, ["character"] = player.Character.Id.Entry,
                    ["ascension"] = run.AscensionLevel, ["relic_id"] = operation.RelicId,
                    ["case_id"] = operation.Id, ["kind"] = operation.Kind, ["params"] = operation.Parameters(),
                    ["initial"] = initial, ["setup"] = setup, ["card_reward_policy"] = rewardPolicy,
                    ["fixture"] = merchantFixture ? "new_run_map_start_shop_override" : "new_run_enter_act_0_unchosen_neow",
                    ["fixture_gold"] = merchantFixture ? gold : null,
                    ["oracle_mode"] = new JsonObject
                    {
                        ["test_mode"] = TestMode.IsOn,
                        ["production_branches"] = TestMode.IsOn ? new JsonArray("MerchantPotionEntry.CalcCost", "CallingBell.GenerateRewards", "Cauldron.GenerateRewards") : new JsonArray(),
                        ["limitation"] = TestMode.IsOn
                            ? "Headless TestMode remains enabled for presentation and selector plumbing; Headless production-branch transpilers restore random generation. This is a shipped-DLL fixture, not an unpatched interactive run."
                            : "TestMode=false shipped DLL; NonInteractive mode and scoped presentation-only adapters skip missing Godot visuals, with in-memory veteran saves. This is an injected fixture, not a live game capture."
                    },
                    ["source"] = State(run, player)
                };
                Task<JsonArray?> execution;
                using (CardSelectCmd.UseSelector(probe))
                {
                    execution = operation.Execute(run, player, probe);
                    Await(execution, sync);
                }
                if (merchantFixture) result["steps"] = execution.Result;
                result["choices"] = probe.Choices;
                result["final"] = State(run, player);
                Program.Out.WriteLine(result.ToJsonString());
            }
            finally
            {
                RewardsSet.testSelector = previous;
                CardSelectCmd.Reset();
                RunManager.Instance.CleanUp();
            }
        }
    }

    private static JsonObject State(RunState run, Player player)
    {
        JsonObject rng = new(), playerRng = new();
        foreach (RunRngType type in Enum.GetValues<RunRngType>()) rng[Snake(type.ToString())] = Rng(run.Rng.GetRng(type));
        foreach (var entry in player.PlayerRng.ToSerializable().Rngs)
            playerRng[Snake(entry.Key.ToString())] = SerializableRng(entry.Value);
        EventModel? ev = (run.CurrentRoom as EventRoom)?.LocalMutableEvent;
        var state = new JsonObject
        {
            ["act_index"] = run.CurrentActIndex, ["act_floor"] = run.ActFloor, ["total_floor"] = run.TotalFloor,
            ["room_type"] = run.CurrentRoom?.RoomType.ToString(), ["event_id"] = ev?.Id.Entry,
            ["hp"] = player.Creature.CurrentHp, ["max_hp"] = player.Creature.MaxHp, ["gold"] = player.Gold,
            ["deck"] = Cards(player.Deck.Cards),
            ["relics"] = new JsonArray(player.Relics.Select(r => (JsonNode)Relic(r)).ToArray()),
            ["potions"] = new JsonArray(player.PotionSlots.Select(p => p == null ? null : (JsonNode)Potion(p)).ToArray()),
            ["rng"] = rng, ["player_rng"] = playerRng, ["event_rng"] = ev == null ? null : Rng(ev.Rng)
        };
        if (run.CurrentRoom is MerchantRoom room) state["shop"] = Shop(room.Inventories[0]);
        if (ev?.Id.Entry == "CRYSTAL_SPHERE") state["crystal_sphere"] = CrystalSphereState();
        return state;
    }

    private static JsonObject Shop(MerchantInventory inventory)
    {
        List<MerchantEntry> entries = inventory.AllEntries.ToList();
        List<MerchantEntry> stocked = entries.Where(e => e.IsStocked).ToList();
        var rows = new JsonArray();
        for (int i = 0; i < entries.Count; ++i)
        {
            MerchantEntry entry = entries[i];
            string kind = entry switch
            {
                MerchantCardEntry => i < inventory.CharacterCardEntries.Count ? "character_card" : "colorless_card",
                MerchantRelicEntry => "relic", MerchantPotionEntry => "potion", MerchantCardRemovalEntry => "card_removal",
                _ => throw new NotSupportedException("Unknown shop entry")
            };
            int slot = entry switch
            {
                MerchantCardEntry c => kind == "character_card" ? inventory.CharacterCardEntries.ToList().IndexOf(c) : inventory.ColorlessCardEntries.ToList().IndexOf(c),
                MerchantRelicEntry r => inventory.RelicEntries.ToList().IndexOf(r),
                MerchantPotionEntry p => inventory.PotionEntries.ToList().IndexOf(p), _ => 0
            };
            JsonNode? item = entry switch
            {
                MerchantCardEntry c => c.CreationResult == null ? null : Card(c.CreationResult.Card),
                MerchantRelicEntry r => r.Model == null ? null : Relic(r.Model),
                MerchantPotionEntry p => p.Model == null ? null : Potion(p.Model), _ => null
            };
            var row = new JsonObject
            {
                ["entry_index"] = i, ["kind"] = kind, ["slot"] = slot,
                ["stocked"] = entry.IsStocked, ["action_index"] = stocked.IndexOf(entry),
                ["cost"] = entry.Cost, ["buyable"] = entry.IsStocked && entry.EnoughGold, ["item"] = item
            };
            if (entry is MerchantCardEntry card) row["on_sale"] = card.IsOnSale;
            rows.Add(row);
        }
        return new JsonObject { ["card_removal_used"] = inventory.CardRemovalEntry?.Used ?? false, ["entries"] = rows };
    }

    private static JsonArray Cards(IEnumerable<CardModel> cards) => new(cards.Select(c => (JsonNode)Card(c)).ToArray());
    private static JsonObject Card(CardModel c) => new()
    {
        ["id"] = c.Id.Entry, ["upgrade"] = c.CurrentUpgradeLevel, ["type"] = (int)c.Type,
        ["rarity"] = (int)c.Rarity, ["pile"] = c.Pile == null ? 0 : (int)c.Pile.Type,
        ["floor_added"] = c.FloorAddedToDeck ?? -1,
        ["enchantment"] = c.Enchantment == null ? null : new JsonObject { ["entry"] = c.Enchantment.Id.Entry, ["amount"] = c.Enchantment.Amount },
        ["affliction"] = c.Affliction == null ? null : new JsonObject { ["entry"] = c.Affliction.Id.Entry, ["amount"] = c.Affliction.Amount },
        ["dynamic_vars"] = Vars(c.DynamicVars)
    };
    private static JsonObject Relic(RelicModel r) => new()
    {
        ["id"] = r.Id.Entry, ["status"] = (int)r.Status, ["floor_added"] = r.FloorAddedToDeck,
        ["melted"] = r.IsMelted, ["wax"] = r.IsWax, ["dynamic_vars"] = Vars(r.DynamicVars)
    };
    private static JsonObject Potion(PotionModel p) => new()
    {
        ["id"] = p.Id.Entry, ["rarity"] = (int)p.Rarity, ["dynamic_vars"] = Vars(p.DynamicVars)
    };
    private static JsonNode Reward(Reward reward)
    {
        JsonObject result = reward switch
        {
        RelicReward r => new JsonObject { ["type"] = "Relic", ["relic"] = r.Relic == null ? null : Relic(r.Relic) },
        PotionReward p => new JsonObject { ["type"] = "Potion", ["potion"] = p.Potion == null ? null : Potion(p.Potion) },
        CardReward c => new JsonObject { ["type"] = "Card", ["cards"] = Cards(c.Cards) },
        GoldReward g => new JsonObject { ["type"] = "Gold", ["amount"] = g.Amount },
        _ => throw new NotSupportedException("Probe reward observation: " + reward.GetType().Name)
        };
        result["selected"] = reward.SuccessfullySelected;
        return result;
    }
    private static JsonArray Vars(DynamicVarSet vars) => new(vars.Select(v => (JsonNode)new JsonObject
    {
        ["name"] = v.Key, ["base_value"] = new JsonObject
        { ["decimal_bits"] = new JsonArray(decimal.GetBits(v.Value.BaseValue).Select(n => (JsonNode?)JsonValue.Create(unchecked((uint)n))).ToArray()) }
    }).ToArray());
    private static JsonObject Rng(Rng rng) => SerializableRng(rng.ToSerializable());
    private static JsonObject SerializableRng(MegaCrit.Sts2.Core.Saves.SerializableRng rng) => new()
    {
        ["counter"] = rng.counter, ["s0"] = rng.state0, ["s1"] = rng.state1, ["s2"] = rng.state2, ["s3"] = rng.state3
    };
    private static string Snake(string name) => string.Concat(name.Select((c, i) => char.IsUpper(c) && i > 0 ? "_" + char.ToLowerInvariant(c) : char.ToLowerInvariant(c).ToString()));
    private static string[] Split(string value)
    {
        string[] entries = value.Split(',', StringSplitOptions.TrimEntries);
        if (entries.Any(string.IsNullOrEmpty) || entries.Distinct().Count() != entries.Length)
            throw new ArgumentException("Lists must contain nonempty unique entries");
        return entries;
    }
    private static void Await(Task task, SingleThreadContext sync)
    {
        Stopwatch elapsed = Stopwatch.StartNew();
        while (!task.IsCompleted)
        {
            sync.Pump();
            if (!task.IsCompleted && elapsed.Elapsed > TimeSpan.FromSeconds(30)) throw new TimeoutException("Oracle operation exceeded 30 seconds");
            if (!task.IsCompleted) Thread.Sleep(1);
        }
        sync.Pump();
        task.GetAwaiter().GetResult();
    }
}
