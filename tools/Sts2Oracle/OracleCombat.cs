using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using System.Text.Json.Nodes;
using System.Threading.Tasks;
using HarmonyLib;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Entities.Potions;
using MegaCrit.Sts2.Core.Entities.Rngs;
using MegaCrit.Sts2.Core.GameActions;
using MegaCrit.Sts2.Core.GameActions.Multiplayer;
using MegaCrit.Sts2.Core.Map;
using MegaCrit.Sts2.Core.MonsterMoves.Intents;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Models.Events;
using MegaCrit.Sts2.Core.Models.Monsters;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Rewards;
using MegaCrit.Sts2.Core.TestSupport;

// `combat`: a new run enters the first monster room on the map and plays it out
// with a seeded random policy. `--encounter ENTRY` (e.g. TOADPOLES_WEAK) fights
// that encounter there instead (RunManager.EnterMapCoordDebug). Emits one JSON line per decision point: the
// combat state, then the action taken. sts2sim replays the same actions and
// diffs the states (L3).
internal static class OracleCombat
{
    // Optional consumer of the live objects behind every emitted state line
    // (tools/CombatSolverModChecks compares a live Env capture with the C++
    // replay). The observer must not change game state; its result is attached
    // to the state event as "live".
    internal static Func<RunState, CombatState, Player, JsonNode?>? StateObserver;

    private static bool _kaiserHeadlessPatchInstalled;

    internal static void InstallKaiserHeadlessPatch()
    {
        if (_kaiserHeadlessPatchInstalled) return;
        _kaiserHeadlessPatchInstalled = true;
        // These hooks only play audio/background animations and dereference
        // NAudioManager.Instance in a headless run. Keep all death logic in
        // CreatureCmd and the combat hooks intact.
        var harmony = new Harmony("sts2oracle.kaiser-headless-vfx");
        var prefix = new HarmonyMethod(typeof(OracleCombat), nameof(SkipKaiserDeathVfx));
        harmony.Patch(AccessTools.Method(typeof(Crusher), nameof(Crusher.BeforeDeath)), prefix: prefix);
        harmony.Patch(AccessTools.Method(typeof(Rocket), nameof(Rocket.BeforeDeath)), prefix: prefix);
    }

    private static bool SkipKaiserDeathVfx(ref Task __result)
    {
        __result = Task.CompletedTask;
        return false;
    }

    private sealed class Selector : ICardSelector
    {
        private readonly RunState _state;
        private readonly Player _player;
        private readonly Random _policy;

        public Selector(RunState state, Player player, Random policy)
        {
            _state = state;
            _player = player;
            _policy = policy;
        }

        public Task<IEnumerable<CardModel>> GetSelectedCards(IEnumerable<CardModel> options, int minSelect, int maxSelect)
        {
            List<CardModel> source = options.ToList();
            int count = Math.Min(maxSelect, source.Count);
            if (count < minSelect) count = Math.Min(minSelect, source.Count);
            List<CardModel> chosen = source.Select((card, index) => (card, index))
                .OrderBy(_ => _policy.Next()).Take(count).Select(x => x.card).ToList();
            EmitSelection(source, minSelect, maxSelect, chosen.Select(c => source.IndexOf(c)).ToList());
            return Task.FromResult<IEnumerable<CardModel>>(chosen);
        }

        public CardRewardSelection GetSelectedCardReward(IReadOnlyList<CardCreationResult> options,
            IReadOnlyList<MegaCrit.Sts2.Core.Entities.CardRewardAlternatives.CardRewardAlternative> alternatives)
        {
            int index = options.Count == 0 ? -1 : _policy.Next(options.Count);
            EmitSelection(options.Select(x => x.Card).ToList(), 0, 1, index < 0 ? new List<int>() : new List<int> { index });
            return new CardRewardSelection { card = index < 0 ? null : options[index].Card };
        }

        private void EmitSelection(IReadOnlyList<CardModel> cards, int min, int max, List<int> indices)
        {
            string source = "CardSelectCmd";
            foreach (StackFrame frame in new StackTrace(false).GetFrames() ?? Array.Empty<StackFrame>())
            {
                MethodBase? method = frame.GetMethod();
                if (method?.DeclaringType?.Name == "CardSelectCmd") { source = method.Name; break; }
            }
            Emit(new JsonObject
            {
                ["event"] = "decision", ["kind"] = "card_select",
                ["options"] = new JsonObject
                {
                    ["cards"] = new JsonArray(cards.Select(c => (JsonNode)(c.Id.Entry +
                        (c.CurrentUpgradeLevel > 0 ? "+" + c.CurrentUpgradeLevel : ""))).ToArray()),
                    ["min"] = min, ["max"] = max, ["can_skip"] = min == 0, ["source"] = source
                },
                ["run"] = OracleRun.DumpRun(_state, _player)
            });
            Emit(new JsonObject { ["event"] = "action", ["kind"] = "card_select",
                ["indices"] = new JsonArray(indices.Select(i => (JsonNode)i).ToArray()) });
        }
    }

    public static void Run(Dictionary<string, string> opts, SingleThreadContext sync)
    {
        InstallKaiserHeadlessPatch();
        string[] seeds = Split(opts.GetValueOrDefault("seeds") ?? opts.GetValueOrDefault("seed", ""));
        string[] characters = Split(opts.GetValueOrDefault("characters") ?? opts.GetValueOrDefault("character", "IRONCLAD"));
        string[] policySeeds = Split(opts.GetValueOrDefault("policy-seeds") ?? opts.GetValueOrDefault("policy-seed", "1"));
        if (seeds.Length == 0) throw new ArgumentException("combat requires --seed or --seeds");
        string[] probeCards = Split(opts.GetValueOrDefault("probe-cards", ""));
        if (probeCards.Length != 0)
        {
            string helper = opts.GetValueOrDefault("probe-helper", "STRIKE_IRONCLAD");
            foreach (string seed in seeds)
            foreach (string character in characters)
            foreach (string policySeed in policySeeds)
            foreach (string cardSpec in probeCards)
            {
                string cardId = cardSpec.Split('+', 2)[0];
                string deck = helper.Length == 0 ? cardSpec : cardSpec + "," + helper;
                var one = new Dictionary<string, string>(opts)
                {
                    ["seed"] = seed,
                    ["character"] = character,
                    ["policy-seed"] = policySeed,
                    ["deck"] = deck,
                    ["force-card"] = cardId,
                    ["probe-stop-after-card"] = "1",
                };
                EmitBegin(new JsonObject
                {
                    ["event"] = "begin",
                    ["seed"] = seed,
                    ["character"] = character.ToUpperInvariant(),
                    ["policy_seed"] = int.Parse(policySeed),
                    ["probe_card"] = cardSpec,
                    ["deck"] = deck,
                }, one);
                try
                {
                    OracleRngLedger.BeginRun();
                    RunOne(one, sync);
                }
                finally
                {
                    RunManager.Instance.CleanUp();
                }
            }
            return;
        }
        if (opts.GetValueOrDefault("probe-mad-science") == "1")
        {
            (CardType type, TinkerTime.RiderEffect rider)[] configurations =
            {
                (CardType.Attack, TinkerTime.RiderEffect.Sapping),
                (CardType.Attack, TinkerTime.RiderEffect.Violence),
                (CardType.Attack, TinkerTime.RiderEffect.Choking),
                (CardType.Skill, TinkerTime.RiderEffect.Energized),
                (CardType.Skill, TinkerTime.RiderEffect.Wisdom),
                (CardType.Skill, TinkerTime.RiderEffect.Chaos),
                (CardType.Power, TinkerTime.RiderEffect.Expertise),
                (CardType.Power, TinkerTime.RiderEffect.Curious),
                (CardType.Power, TinkerTime.RiderEffect.Improvement),
            };
            string helper = opts.GetValueOrDefault("probe-helper", "STRIKE_IRONCLAD");
            foreach (string seed in seeds)
            foreach (string character in characters)
            foreach (string policySeed in policySeeds)
            foreach ((CardType type, TinkerTime.RiderEffect rider) in configurations)
            foreach (int upgrade in new[] { 0, 1 })
            {
                string cardSpec = upgrade == 0 ? "MAD_SCIENCE" : "MAD_SCIENCE+1";
                string deck = helper.Length == 0 ? cardSpec : cardSpec + "," + helper;
                var one = new Dictionary<string, string>(opts)
                {
                    ["seed"] = seed,
                    ["character"] = character,
                    ["policy-seed"] = policySeed,
                    ["deck"] = deck,
                    ["force-card"] = "MAD_SCIENCE",
                    ["probe-stop-after-card"] = "1",
                    ["probe-type"] = type.ToString(),
                    ["probe-rider"] = rider.ToString(),
                };
                EmitBegin(new JsonObject
                {
                    ["event"] = "begin",
                    ["seed"] = seed,
                    ["character"] = character.ToUpperInvariant(),
                    ["policy_seed"] = int.Parse(policySeed),
                    ["probe_card"] = cardSpec,
                    ["probe_type"] = type.ToString(),
                    ["probe_rider"] = rider.ToString(),
                    ["deck"] = deck,
                }, one);
                try
                {
                    OracleRngLedger.BeginRun();
                    RunOne(one, sync);
                }
                finally
                {
                    RunManager.Instance.CleanUp();
                }
            }
            return;
        }
        string[] probePotions = Split(opts.GetValueOrDefault("probe-potions", ""));
        if (probePotions.Length != 0)
        {
            foreach (string seed in seeds)
            foreach (string character in characters)
            foreach (string policySeed in policySeeds)
            foreach (string potionId in probePotions)
            {
                PotionModel potion = ModelDb.GetById<PotionModel>(new ModelId("POTION", potionId));
                bool auto = potion.Usage == PotionUsage.Automatic;
                var one = new Dictionary<string, string>(opts)
                {
                    ["seed"] = seed,
                    ["character"] = character,
                    ["policy-seed"] = policySeed,
                    ["potion"] = potionId,
                    ["potion-target"] = "auto",
                    ["probe-stop-after-potion"] = "1",
                    ["probe-auto-potion"] = auto ? "1" : "0",
                };
                EmitBegin(new JsonObject
                {
                    ["event"] = "begin",
                    ["seed"] = seed,
                    ["character"] = character.ToUpperInvariant(),
                    ["policy_seed"] = int.Parse(policySeed),
                    ["probe_potion"] = potionId,
                    ["potion"] = potionId,
                    ["potion_target"] = "auto",
                    ["probe_auto_death"] = auto,
                }, one);
                try
                {
                    OracleRngLedger.BeginRun();
                    RunOne(one, sync);
                }
                finally
                {
                    RunManager.Instance.CleanUp();
                }
            }
            return;
        }
        foreach (string seed in seeds)
        foreach (string character in characters)
        foreach (string policySeed in policySeeds)
        {
            var one = new Dictionary<string, string>(opts)
            {
                ["seed"] = seed,
                ["character"] = character,
                ["policy-seed"] = policySeed,
            };
            EmitBegin(new JsonObject
            {
                ["event"] = "begin",
                ["seed"] = seed,
                ["character"] = character.ToUpperInvariant(),
                ["policy_seed"] = int.Parse(policySeed),
            }, one);
            try
            {
                OracleRngLedger.BeginRun();
                RunOne(one, sync);
            }
            finally
            {
                RunManager.Instance.CleanUp();
            }
        }
    }

    // Capture the actual inputs alongside every combat, including focused
    // probes. Offline replay must not guess an ascension or injected energy.
    private static void EmitBegin(JsonObject begin, Dictionary<string, string> opts)
    {
        begin["schema_version"] = 1;
        begin["ascension"] = int.Parse(opts.GetValueOrDefault("ascension", "0"));
        foreach (string key in new[] { "deck", "relics", "potion", "potion-target" })
            if (opts.TryGetValue(key, out string? value)) begin[key.Replace('-', '_')] = value;
        foreach (string key in new[] { "starting-energy", "starting-stars" })
            if (opts.TryGetValue(key, out string? value)) begin[key.Replace('-', '_')] = int.Parse(value);
        begin["take_special_rewards"] = opts.GetValueOrDefault("take-special-rewards") == "1";
        begin["probe_after_card_turn"] = opts.GetValueOrDefault("probe-after-card-turn") == "1";
        Emit(begin);
    }

    private static void RunOne(Dictionary<string, string> opts, SingleThreadContext sync)
    {
        RunState state = Oracle.StartRun(opts);
        int policySeed = int.Parse(opts.GetValueOrDefault("policy-seed", "1"));
        int maxActions = int.Parse(opts.GetValueOrDefault("max-actions", "300"));
        var policy = new Random(policySeed);
        Player player = state.Players[0];
        if (opts.TryGetValue("deck", out string? deck))
        {
            foreach (CardModel card in player.Deck.Cards.ToList()) CardPileCmd.RemoveFromDeck(card);
            foreach (string spec in Split(deck))
            {
                string[] parts = spec.Split('+', 2);
                int upgrades = parts.Length == 2 ? int.Parse(parts[1]) : 0;
                if (upgrades < 0) throw new ArgumentException("negative card upgrade: " + spec);
                CardModel card = state.CreateCard(ModelDb.GetById<CardModel>(new ModelId("CARD", parts[0])), player);
                Await(CardPileCmd.Add(card, PileType.Deck), sync);
                for (int i = 0; i < upgrades; i++) CardCmd.Upgrade(card);
                if (parts[0] == "MAD_SCIENCE" && opts.TryGetValue("probe-type", out string? probeType) &&
                    opts.TryGetValue("probe-rider", out string? probeRider))
                {
                    MadScience madScience = (MadScience)card;
                    madScience.TinkerTimeType = Enum.Parse<CardType>(probeType, ignoreCase: false);
                    madScience.TinkerTimeRider = Enum.Parse<TinkerTime.RiderEffect>(probeRider, ignoreCase: false);
                }
            }
        }
        if (opts.TryGetValue("relics", out string? relics))
            foreach (string id in Split(relics))
                Await(RelicCmd.Obtain(ModelDb.GetById<RelicModel>(new ModelId("RELIC", id)).ToMutable(), player), sync);
        PotionModel? testPotion = null;
        if (opts.TryGetValue("potion", out string? potionId) && !string.IsNullOrWhiteSpace(potionId))
        {
            testPotion = ModelDb.GetById<PotionModel>(new ModelId("POTION", potionId)).ToMutable();
            Task<PotionProcureResult> procure = PotionCmd.TryToProcure(testPotion, player);
            Await(procure, sync);
            if (!procure.GetAwaiter().GetResult().success)
                throw new InvalidOperationException("could not procure test potion " + potionId);
        }
        using IDisposable selector = CardSelectCmd.UseSelector(new Selector(state, player, policy));
        bool stopAfterProbeCard = opts.GetValueOrDefault("probe-stop-after-card") == "1";
        bool probeAfterCardTurn = opts.GetValueOrDefault("probe-after-card-turn") == "1";
        string forceCard = opts.GetValueOrDefault("force-card", "");
        bool probedCardWasPlayed = false;
        bool probeTurnEndQueued = false;
        bool stopAfterProbePotion = opts.GetValueOrDefault("probe-stop-after-potion") == "1";
        bool probeAutoPotion = opts.GetValueOrDefault("probe-auto-potion") == "1";
        bool autoPotionTurnEndQueued = false;

        Await(RunManager.Instance.SetActInternal(0), sync);
        MapCoord coord = state.Map.GetPointsInRow(1).OrderBy(p => p.coord.col).First().coord;
        string? encounterEntry = opts.GetValueOrDefault("encounter");
        var enter = new JsonObject { ["event"] = "enter", ["coord"] = new JsonArray(coord.col, coord.row) };
        if (encounterEntry != null) enter["encounter"] = encounterEntry;
        Emit(enter);
        if (encounterEntry == null)
        {
            Await(RunManager.Instance.EnterMapCoord(coord), sync);
        }
        else
        {
            EncounterModel encounter = ModelDb.GetById<EncounterModel>(new ModelId("ENCOUNTER", encounterEntry)).ToMutable();
            Await(RunManager.Instance.EnterMapCoordDebug(coord, encounter.RoomType, MapPointType.Unassigned, encounter, showTransition: false), sync);
        }

        if (opts.TryGetValue("starting-energy", out string? energy))
            player.PlayerCombatState!.Energy = int.Parse(energy);
        if (opts.TryGetValue("starting-stars", out string? stars))
            player.PlayerCombatState!.Stars = int.Parse(stars);
        if (testPotion != null)
        {
            string targetSpec = opts.GetValueOrDefault("potion-target", "player");
            Creature? potionTarget = targetSpec switch
            {
                "player" => player.Creature,
                "none" => null,
                "enemy" => CombatManager.Instance.DebugOnlyGetState()?.HittableEnemies
                    .FirstOrDefault(testPotion.IsValidTarget),
                "auto" => testPotion.TargetType switch
                {
                    TargetType.Self or TargetType.AnyPlayer => player.Creature,
                    TargetType.AnyEnemy => CombatManager.Instance.DebugOnlyGetState()?.HittableEnemies
                        .FirstOrDefault(testPotion.IsValidTarget),
                    _ => null,
                },
                _ => CombatManager.Instance.DebugOnlyGetState()?.GetCreature(uint.Parse(targetSpec)),
            };
            if (probeAutoPotion)
            {
                player.Creature.SetCurrentHpInternal(1);
            }
            else
            {
                Await(testPotion.OnUseWrapper(new ThrowingPlayerChoiceContext(), potionTarget), sync);
            }
        }

        for (int step = 0; step < maxActions; step++)
        {
            sync.Pump();
            CombatState? combat = CombatManager.Instance.DebugOnlyGetState();
            if (combat == null || !CombatManager.Instance.IsInProgress)
            {
                List<Reward> extraRewards = state.CurrentRoom is CombatRoom room && room.ExtraRewards.TryGetValue(player, out var rewards)
                    ? rewards : new List<Reward>();
                var end = new JsonObject { ["event"] = "end", ["player_hp"] = player.Creature.CurrentHp,
                    ["rng"] = RngCounters(state), ["run"] = OracleRun.DumpRun(state, player),
                    ["extra_rewards"] = new JsonArray(extraRewards.Select(OracleRun.RewardJson).ToArray()) };
                if (opts.GetValueOrDefault("take-special-rewards") == "1")
                {
                    foreach (Reward reward in extraRewards)
                        if (reward is SpecialCardReward) Await(reward.SelectUnsynchronized(), sync);
                    end["run_after_special_rewards"] = OracleRun.DumpRun(state, player);
                }
                Emit(end);
                return;
            }
            if (!AtPlayPhase(combat, player))
            {
                throw new InvalidOperationException("combat is neither over nor waiting for the player");
            }
            foreach (Creature enemy in combat.Enemies)
                if (enemy.Monster != null && enemy.CombatId.HasValue)
                    OracleRngLedger.RegisterMonster(enemy.Monster.Rng, (int)enemy.CombatId.Value);
            var stateEvent = new JsonObject { ["event"] = "state", ["state"] = Dump(state, combat, player) };
            if (StateObserver?.Invoke(state, combat, player) is { } observed) stateEvent["live"] = observed;
            Emit(stateEvent);

            if (stopAfterProbePotion)
            {
                if (probeAutoPotion && !autoPotionTurnEndQueued)
                {
                    autoPotionTurnEndQueued = true;
                    Emit(new JsonObject { ["event"] = "action", ["type"] = "end_turn" });
                    RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(
                        new EndPlayerTurnAction(player, player.PlayerCombatState!.TurnNumber));
                    continue;
                }
                return;
            }

            // Batch card probes stop at the first post-play state. This runs
            // the full source effect and any choices it asks for, while keeping
            // a generated foreign-class card from becoming the next probe.
            if (stopAfterProbeCard && probedCardWasPlayed)
            {
                if (!probeAfterCardTurn || probeTurnEndQueued) return;
                probeTurnEndQueued = true;
                Emit(new JsonObject { ["event"] = "action", ["type"] = "end_turn" });
                RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(
                    new EndPlayerTurnAction(player, player.PlayerCombatState!.TurnNumber));
                continue;
            }

            var plays = new List<(int hand, CardModel card, Creature? target)>();
            IReadOnlyList<CardModel> hand = player.PlayerCombatState!.Hand.Cards;
            for (int i = 0; i < hand.Count; i++)
            {
                CardModel card = hand[i];
                if (!card.CanPlay()) continue;
                if (card.TargetType == TargetType.AnyEnemy || card.TargetType == TargetType.AnyAlly)
                {
                    foreach (Creature c in combat.Creatures)
                    {
                        if (card.IsValidTarget(c)) plays.Add((i, card, c));
                    }
                }
                else if (card.IsValidTarget(null))
                {
                    plays.Add((i, card, null));
                }
            }
            int forcedPlay = forceCard.Length == 0 ? -1 : plays.FindIndex(p => p.card.Id.Entry == forceCard);
            if (forcedPlay >= 0 || (plays.Count > 0 && policy.NextDouble() < 0.85))
            {
                var (i, card, target) = forcedPlay >= 0 ? plays[forcedPlay] : plays[policy.Next(plays.Count)];
                if (card.Id.Entry == forceCard) probedCardWasPlayed = true;
                Emit(new JsonObject
                {
                    ["event"] = "action",
                    ["type"] = "play",
                    ["hand_index"] = i,
                    ["card"] = card.Id.Entry,
                    ["target"] = target == null ? null : (int)target.CombatId!.Value,
                });
                RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(new PlayCardAction(card, target));
            }
            else
            {
                Emit(new JsonObject { ["event"] = "action", ["type"] = "end_turn" });
                RunManager.Instance.ActionQueueSynchronizer.RequestEnqueue(
                    new EndPlayerTurnAction(player, player.PlayerCombatState.TurnNumber));
            }
        }
        throw new InvalidOperationException("combat did not finish within max-actions");
    }

    private static string[] Split(string value) =>
        value.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);

    private static bool AtPlayPhase(CombatState combat, Player player) =>
        combat.CurrentSide == CombatSide.Player &&
        player.PlayerCombatState?.Phase == PlayerTurnPhase.Play &&
        !RunManager.Instance.ActionExecutor.IsRunning;

    private static void Await(Task task, SingleThreadContext sync)
    {
        for (int i = 0; i < 100000 && !task.IsCompleted; i++) sync.Pump();
        task.GetAwaiter().GetResult();
    }

    private static void Emit(JsonObject o)
    {
        Program.Out.WriteLine(o.ToJsonString());
    }

    private static JsonObject RngCounters(RunState state)
    {
        var o = new JsonObject();
        foreach (RunRngType t in Enum.GetValues<RunRngType>())
        {
            var s = state.Rng.GetRng(t).ToSerializable();
            o[t.ToString()] = s.counter;
        }
        return o;
    }

    private static JsonArray Powers(Creature c) =>
        new(c.Powers.Select(p => (JsonNode)new JsonArray(p.Id.Entry, p.Amount)).ToArray());

    private static JsonArray Pile(CardPile pile) =>
        new(pile.Cards.Select(c => (JsonNode)(c.Id.Entry + (c.CurrentUpgradeLevel > 0 ? "+" + c.CurrentUpgradeLevel : ""))).ToArray());

    private static JsonArray HandCosts(CardPile pile) =>
        new(pile.Cards.Select(c => (JsonNode)c.EnergyCost.GetWithModifiers(CostModifiers.All)).ToArray());

    private static JsonArray Intents(Creature enemy, IReadOnlyList<Creature> targets)
    {
        var intents = new JsonArray();
        if (enemy.Monster?.NextMove == null) return intents;
        foreach (AbstractIntent intent in enemy.Monster.NextMove.Intents)
        {
            var item = new JsonObject { ["type"] = intent.IntentType.ToString() };
            if (intent is AttackIntent attack)
            {
                item["damage"] = attack.GetSingleDamage(targets, enemy);
                item["repeats"] = attack.Repeats;
            }
            intents.Add(item);
        }
        return intents;
    }

    public static JsonObject Dump(RunState state, CombatState combat, Player player)
    {
        PlayerCombatState pcs = player.PlayerCombatState!;
        var enemies = new JsonArray();
        foreach (Creature e in combat.Enemies)
        {
            if (e.Monster != null && e.CombatId.HasValue)
                OracleRngLedger.RegisterMonster(e.Monster.Rng, (int)e.CombatId.Value);
            enemies.Add(new JsonObject
            {
                ["id"] = e.Monster!.Id.Entry,
                ["combat_id"] = (int)e.CombatId!.Value,
                ["hp"] = e.CurrentHp,
                ["max_hp"] = e.MaxHp,
                ["block"] = e.Block,
                ["powers"] = Powers(e),
                ["next_move"] = e.Monster.NextMove?.Id,
                ["intents"] = Intents(e, combat.PlayerCreatures),
                ["rng_counter"] = e.Monster.Rng.ToSerializable().counter,
            });
        }
        return new JsonObject
        {
            ["round"] = combat.RoundNumber,
            ["turn"] = pcs.TurnNumber,
            ["test_subject_kills"] = state.ExtraFields.TestSubjectKills,
            ["player"] = new JsonObject
            {
                ["hp"] = player.Creature.CurrentHp,
                ["max_hp"] = player.Creature.MaxHp,
                ["block"] = player.Creature.Block,
                ["energy"] = pcs.Energy,
                ["stars"] = pcs.Stars,
                ["powers"] = Powers(player.Creature),
                ["hand"] = Pile(pcs.Hand),
                ["hand_costs"] = HandCosts(pcs.Hand),
                ["hand_afflictions"] = new JsonArray(pcs.Hand.Cards.Select(c => (JsonNode?)c.Affliction?.Id.Entry).ToArray()),
                ["draw"] = Pile(pcs.DrawPile),
                ["discard"] = Pile(pcs.DiscardPile),
                ["exhaust"] = Pile(pcs.ExhaustPile),
                ["play"] = Pile(pcs.PlayPile),
                ["relics"] = new JsonArray(player.Relics.Select(r =>
                {
                    var relic = new JsonObject { ["id"] = r.Id.Entry };
                    if (r.ShowCounter) relic["counter"] = r.DisplayAmount;
                    return (JsonNode)relic;
                }).ToArray()),
                ["potion_slots"] = new JsonArray(player.PotionSlots.Select(p => (JsonNode?)p?.Id.Entry).ToArray()),
                ["orb_capacity"] = pcs.OrbQueue.Capacity,
                ["orbs"] = new JsonArray(pcs.OrbQueue.Orbs.Select(o => (JsonNode)o.Id.Entry).ToArray()),
                ["orb_values"] = new JsonArray(pcs.OrbQueue.Orbs.Select(o => (JsonNode)JsonValue.Create((int)o.EvokeVal)!).ToArray()),
                ["pets"] = new JsonArray(pcs.Pets.Select(p => (JsonNode)new JsonObject
                {
                    ["id"] = p.Monster!.Id.Entry,
                    ["combat_id"] = (int)p.CombatId!.Value,
                    ["hp"] = p.CurrentHp,
                    ["max_hp"] = p.MaxHp,
                    ["block"] = p.Block,
                    ["powers"] = Powers(p),
                    ["next_move"] = p.Monster.NextMove?.Id,
                }).ToArray()),
            },
            ["enemies"] = enemies,
            ["rng"] = RngCounters(state),
        };
    }
}
