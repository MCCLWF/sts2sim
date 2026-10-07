extern alias Derived;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.Loader;
using System.Runtime.CompilerServices;
using System.Text.Json;
using System.Text.Json.Nodes;
using CombatSolver;
using CombatSolver.Engine.Common;
using CombatSolver.Engine.InCombat.Simulation;
using CombatSolverEnv.Api;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.GameActions.Multiplayer;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Hooks;
using MegaCrit.Sts2.Core.Localization.DynamicVars;
using MegaCrit.Sts2.Core.Map;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Encounters;
using MegaCrit.Sts2.Core.Models.Powers;
using MegaCrit.Sts2.Core.Models.Orbs;
using MegaCrit.Sts2.Core.MonsterMoves.Intents;
using MegaCrit.Sts2.Core.MonsterMoves.MonsterMoveStateMachine;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.TestSupport;
using MegaCrit.Sts2.Core.Unlocks;
using MegaCrit.Sts2.Core.ValueProps;
using Sts2EnvCompare;

// Explicit full live roots. This compares shipped getter values; CombatSolver
// has separate CanPlay/cost policy semantics, not implemented by these queries.
internal static class Program
{
    private static void Main(string[] args)
    {
        GameBootstrap.InstallResolver(GameBootstrap.DefaultGameData);
        AssemblyLoadContext.Default.Resolving += (context, name) =>
        {
            foreach (string key in new[] { "RitsuCompatDirectory", "RitsuSharedDirectory" })
            {
                string directory = typeof(Program).Assembly.GetCustomAttributes<AssemblyMetadataAttribute>()
                    .Single(item => item.Key == key).Value!;
                string path = Path.Combine(directory, name.Name + ".dll");
                if (File.Exists(path)) return context.LoadFromAssemblyPath(path);
            }
            return null;
        };
        Check(Path.GetFullPath(args.Single()));
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static void Check(string output)
    {
        Directory.CreateDirectory(output);
        GameBootstrap.Init(testMode: false);
        GameBootstrap.LoadLocalization();
        GameBootstrap.UseInMemorySaves();
        if (TestMode.IsOn) throw new InvalidOperationException("query fixture requires TestMode=false");
        SingleThreadContext sync = SingleThreadContext.Install();
        NonInteractiveMode.AutoSlayerCheck = () => true;
        Action restore = Headless.InstallLiveCombatPresentation();
        RunManager manager = RunManager.Instance;
        CombatManager cm = CombatManager.Instance;
        void Await(Task task)
        {
            Stopwatch timeout = Stopwatch.StartNew();
            while (!task.IsCompleted && timeout.Elapsed < TimeSpan.FromSeconds(20)) { sync.Pump(); Thread.Yield(); }
            if (!task.IsCompleted) throw new TimeoutException("query fixture startup timeout");
            task.GetAwaiter().GetResult();
        }
        try
        {
            Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all, 1);
            RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(acts => acts[0].ToMutable()).ToList(),
                [], GameMode.Standard, 0, "QUERYROOT001");
            manager.SetUpNewSingleplayer(run, false); Await(manager.FinalizeStartingRelics()); manager.Launch();
            run.Map = StandardActMap.CreateFor(run, false);
            manager.CombatReplayWriter.IsEnabled = false;
            EncounterModel encounter = ModelDb.Encounter<CultistsNormal>().ToMutable();
            encounter.GenerateMonstersWithSlots(run);
            CombatState combat = new(encounter, run);
            combat.AddPlayer(player);
            foreach (var (monster, slot) in encounter.MonstersWithSlots)
                combat.AddCreature(combat.CreateCreature(monster, CombatSide.Enemy, slot));
            run.PushRoom(new CombatRoom(combat)); cm.SetUpCombat(combat);
            foreach (Creature creature in combat.Creatures) Await(cm.AfterCreatureAdded(creature));
            cm._turnState!.IsInProgress = true; cm._turnState.IsStarting = false;
            Await(Hook.BeforeCombatStart(run, combat)); Await(cm.StartTurn(cm._turnState));
            PlayerCombatState pcs = player.PlayerCombatState!;
            // Genuine game commands establish nonzero finished-play and
            // unblocked-damage history for GoldAxe/TearAsunder's calculations.
            for (int playIndex = 0; playIndex < 2; playIndex++)
            {
                CardModel playedCard = combat.CreateCard<StrikeIronclad>(player);
                pcs.Hand.AddInternal(playedCard, silent: true);
                var spending = playedCard.SpendResources(); Await(spending);
                var (energySpent, starsSpent) = spending.GetAwaiter().GetResult();
                Await(playedCard.OnPlayWrapper(new ThrowingPlayerChoiceContext(), combat.Enemies[0], false,
                    new() { EnergySpent = energySpent, EnergyValue = energySpent, StarsSpent = starsSpent, StarValue = starsSpent }));
                Await(CreatureCmd.Damage(new ThrowingPlayerChoiceContext(), player.Creature, 1m,
                    ValueProp.Unblockable | ValueProp.Unpowered, combat.Enemies[0]));
            }
            // Keep one combat card in each pile, plus X, negative cost and
            // temporary star-cost cases. All changes are explicit native APIs.
            foreach (CardPile pile in pcs.AllPiles) foreach (CardModel card in pile.Cards.ToArray())
                pile.RemoveInternal(card, silent: true);
            CardModel Add<T>(PileType pile) where T : CardModel
            {
                CardModel card = combat.CreateCard<T>(player);
                CardPile.Get(pile, player)!.AddInternal(card, silent: true);
                return card;
            }
            CardModel strike = Add<StrikeIronclad>(PileType.Hand);
            strike.UpgradeInternal(); strike.FinalizeUpgradeInternal();
            strike.DynamicVars.Damage.BaseValue = -3.50m;
            strike.BaseReplayCount = 2; strike.ExhaustOnNextPlay = true;
            strike.GiveSingleTurnRetain(); strike.GiveSingleTurnSly();
            strike.DeckVersion = player.Deck.Cards.First(card => card is StrikeIronclad);
            strike.EnergyCost.SetThisCombat(3); strike.EnergyCost.AddThisTurn(-1);
            Add<DefendIronclad>(PileType.Hand).AddKeyword(CardKeyword.Unplayable);
            CardModel whirl = Add<Whirlwind>(PileType.Hand); whirl.EnergyCost.CapturedXValue = 7;
            Add<CloakOfStars>(PileType.Draw).SetStarCostThisTurn(0);
            Add<FallingStar>(PileType.Discard).SetStarCostUntilPlayed(4);
            Add<Burn>(PileType.Exhaust);
            Rampage rampage = (Rampage)Add<Rampage>(PileType.Discard);
            rampage.UpgradeInternal(); rampage.FinalizeUpgradeInternal();
            rampage._extraDamageFromPlays = -1.2500m;
            rampage.DynamicVars.Damage.BaseValue = 8.7500m;
            Thrash thrash = (Thrash)Add<Thrash>(PileType.Discard);
            thrash.UpgradeInternal(); thrash.FinalizeUpgradeInternal();
            thrash._extraDamage = 0.1250m;
            thrash.DynamicVars.Damage.BaseValue = 6.1250m;
            string[] calculatedCards = ["ASHEN_STRIKE", "BODY_SLAM", "BULLY", "GOLD_AXE", "MIND_BLAST", "PERFECTED_STRIKE",
                "REND", "UNLEASH", "DEMONIC_SHIELD", "EXPECT_A_FIGHT", "BARRAGE", "COMPILE_DRIVER", "TEAR_ASUNDER"];
            foreach (string entry in calculatedCards)
            {
                CardModel card = combat.CreateCard(ModelDb.GetById<CardModel>(new("CARD", entry)), player);
                pcs.DiscardPile.AddInternal(card, silent: true);
                // Modify true formula operands while retaining the derived
                // BaseValue, so a BaseValue substitution must fail the diff.
                card.DynamicVars.CalculationBase.BaseValue = -1.2500m;
                if (card.DynamicVars.TryGetValue("ExtraDamage", out DynamicVar? extra)) extra.BaseValue = 0.8750m;
                else card.DynamicVars.CalculationExtra.BaseValue = 0.8750m;
            }
            player.Creature.Block = 13;
            StrengthPower strength = (StrengthPower)ModelDb.Power<StrengthPower>().ToMutable();
            strength.ApplyInternal(player.Creature, 3, true);
            Await(PowerCmd.Apply<VulnerablePower>(new ThrowingPlayerChoiceContext(), combat.Enemies[0], 2, player.Creature, null));
            Await(PowerCmd.Apply<WeakPower>(new ThrowingPlayerChoiceContext(), combat.Enemies[0], 1, player.Creature, null));
            // Actual channels cover all five orb types, duplicate instances,
            // and a capacity greater than queue count.
            Await(OrbCmd.AddSlots(player, 8));
            Await(OrbCmd.Channel<LightningOrb>(new ThrowingPlayerChoiceContext(), player));
            Await(OrbCmd.Channel<LightningOrb>(new ThrowingPlayerChoiceContext(), player));
            Await(OrbCmd.Channel<FrostOrb>(new ThrowingPlayerChoiceContext(), player));
            Await(OrbCmd.Channel<DarkOrb>(new ThrowingPlayerChoiceContext(), player));
            Await(OrbCmd.Channel<GlassOrb>(new ThrowingPlayerChoiceContext(), player));
            Await(OrbCmd.Channel<PlasmaOrb>(new ThrowingPlayerChoiceContext(), player));
            ((DarkOrb)pcs.OrbQueue.Orbs.Single(orb => orb is DarkOrb))._evokeVal = -2.5000m;
            ((GlassOrb)pcs.OrbQueue.Orbs.Single(orb => orb is GlassOrb))._passiveVal = 1.2500m;
            FocusPower focus = (FocusPower)ModelDb.Power<FocusPower>().ToMutable();
            focus.ApplyInternal(player.Creature, 2, true);
            DrawCardsNextTurnPower drawNext = (DrawCardsNextTurnPower)ModelDb.Power<DrawCardsNextTurnPower>().ToMutable();
            drawNext.ApplyInternal(player.Creature, 3, true); drawNext.AmountOnTurnStart = 7;
            // The original key deliberately ignores these Strength fields.
            strength.AmountOnTurnStart = 123; strength.SkipNextDurationTick = true;
            SurroundedPower surrounded = (SurroundedPower)ModelDb.Power<SurroundedPower>().ToMutable();
            surrounded.ApplyInternal(player.Creature, 1, true);
            RitualPower ritual = (RitualPower)ModelDb.Power<RitualPower>().ToMutable();
            ritual.ApplyInternal(player.Creature, 2, true);
            ritual._applier = combat.Enemies[0]; ritual._target = combat.Enemies[1];
            PanachePower panache = (PanachePower)ModelDb.Power<PanachePower>().ToMutable();
            panache.ApplyInternal(player.Creature, 10, true);
            panache.DynamicVars["CardsLeft"].BaseValue = -1.2500m;
            EnvLiveCards.Warmup(combat);
            CalculatedVarChecks.CheckBindingRejection(player, combat);
            CardModel playCard = Add<StrikeIronclad>(PileType.Discard); playCard.SetStarCostThisTurn(0);
            PyrePower pyre = (PyrePower)ModelDb.Power<PyrePower>().ToMutable(); pyre.ApplyInternal(player.Creature, 2, true);
            FreeAttackPower free = (FreeAttackPower)ModelDb.Power<FreeAttackPower>().ToMutable(); free.ApplyInternal(player.Creature, 1, true);
            SlothPower sloth = (SlothPower)ModelDb.Power<SlothPower>().ToMutable(); sloth.ApplyInternal(player.Creature, 2, true);
            combat.Enemies[0].Monster!.SetMoveImmediate((MoveState)combat.Enemies[0].Monster!.MoveStateMachine!.States["DARK_STRIKE_MOVE"], true);
            combat.Enemies[1].CurrentHp = 0; // Target getter checks IsAlive; no death action is run.
            NativeEnv.Configure();
            using NativeEnv.Session schema = NativeEnv.OpenCombat("DUAL001", "CULTISTS_NORMAL");
            string blank = NativeEnv.ExportBlankTemplateJson(schema);
            foreach (var (name, energy, stars, played) in new[] { ("baseline", 2, 3, 1), ("empty_resources", 0, 0, 1),
                ("blocked_hook", 2, 3, 2), ("play_pile", 2, 3, 1) })
            {
                pcs.Energy = energy; pcs.Stars = stars; sloth._cardsPlayedThisTurn = played;
                strength.SetAmount(name == "empty_resources" ? -3 : 3, silent: true);
                focus.SetAmount(name == "empty_resources" ? -4 : 2, silent: true);
                surrounded._facing = name == "empty_resources" ? SurroundedPower.Direction.Right : SurroundedPower.Direction.Left;
                ritual._wasJustAppliedByEnemy = name != "empty_resources";
                combat.Enemies[0].GetPower<WeakPower>()!.SkipNextDurationTick = name != "empty_resources";
                Creature[] creatures = combat.Creatures.ToArray();
                EnvLiveCards.Warmup(combat); EnvLiveCreatures.Warmup(combat); EnvLiveInventory.Warmup(player);
                EnvLiveCards cards = EnvLiveCards.Capture(combat, player, creatures, new Dictionary<CardModel, int>(ReferenceEqualityComparer.Instance));
                EnvLiveCreatures capturedCreatures = EnvLiveCreatures.Capture(combat, player, creatures);
                EnvLiveRootComponents components = new(cards, EnvLiveMonsters.Capture(creatures), EnvLiveRunContext.Capture(combat, player),
                    capturedCreatures, EnvLiveInventory.Capture(player),
                    EnvLiveCombatContext.Capture(combat, player, creatures), EnvLiveRoomContext.Capture(run, player, combat));
                string root = EnvLiveRootAssembler.Compose(components, schema, blank);
                if (name == "play_pile")
                {
                    // Production live capture correctly refuses a nonempty
                    // Play pile. Extend this explicit query fixture after its
                    // complete capture using only the native silent pile move
                    // and its two changed serialized operands, not seed data.
                    int index = pcs.DiscardPile.Cards.ToList().IndexOf(playCard);
                    int reference = cards.Component.GetProperty("piles")[2][index].GetInt32();
                    pcs.DiscardPile.RemoveInternal(playCard, silent: true);
                    pcs.PlayPile.AddInternal(playCard, silent: true);
                    JsonObject moved = JsonNode.Parse(root)!.AsObject();
                    JsonArray piles = moved["state"]!["player_combat"]!["piles"]!.AsArray();
                    piles[2]!.AsArray().RemoveAt(index); piles[4]!.AsArray().Add(reference);
                    moved["state"]!["cards"]!["slots"]!.AsArray().Single(slot => slot!["index"]!.GetValue<int>() == reference)!["value"]!["pile"] = (int)PileType.Play;
                    JsonObject component = JsonNode.Parse(cards.Component.GetRawText())!.AsObject();
                    component["piles"] = piles.DeepClone(); component["cards"] = moved["state"]!["cards"]!.DeepClone();
                    cards = new(JsonSerializer.SerializeToElement(component));
                    root = moved.ToJsonString();
                }
                using NativeEnv.Session canonical = NativeEnv.ImportCombatSnapshot(root);
                root = canonical.ExportSnapshotJson();
                using EnvCombatPredictionSession prediction = EnvPredictionApi.OpenCombatPrediction(EnvStateSnapshot.Parse(root));
                ulong hash = prediction.StateHash;
                JsonElement expected = ReadNative(player, combat, cards, hash);
                EnvCombatGameQueries queried = prediction.CaptureGameQueries();
                if (!JsonNode.DeepEquals(JsonNode.Parse(expected.GetRawText()), JsonNode.Parse(queried.Document.GetRawText()))
                    || queried.PredictionHash != hash || queried.PolicyProjectionComplete || queried.ThreatProjectionComplete
                    || prediction.StateHash != hash)
                    throw new InvalidOperationException($"Public game query mismatch or source mutation: {name}");
                File.WriteAllText(Path.Combine(output, name + ".root.json"), root);
                File.WriteAllText(Path.Combine(output, name + ".native.json"), expected.GetRawText());
                File.WriteAllText(Path.Combine(output, name + ".public.json"), queried.Document.GetRawText());
                if (name != "play_pile")
                {
                    // Baseline from the original search predicate, kept
                    // separate from the shipped game getter comparison.
                    CombatRootSnapshot searchRoot = CombatRootSnapshot.Capture(combat);
                    CombatPredictionSimulator search = searchRoot.ForkSimulator();
                    CalculatedVarChecks.Compare(player, combat, cards, search, prediction, name, output);
                    OrbQueryChecks.Compare(player, search, prediction, name, output);
                    PowerQueryChecks.Compare(combat, capturedCreatures, search, prediction, name, output);
                    SimulatedCombatState projected = (SimulatedCombatState)search.State.CombatState;
                    // Use the shipped projection and its API type, rather than
                    // the fixture's locally linked C ABI wrapper.
                    var fingerprints = Derived::CombatSolver.EnvSearchCardFingerprintProjection.From(
                        Derived::CombatSolverEnv.Api.EnvCombatGameQueries.Parse(queried.Document.GetRawText(), hash));
                    if (name == "baseline") CardProjectionRefusalChecks.Check(queried.Document);
                    int fingerprintCount = 0;
                    object[] policyCards = pcs.AllPiles.SelectMany((pile, pileIndex) => pile.Cards.Select((card, index) =>
                    {
                        PredictedCard predicted = search.State.FindCard(card)
                            ?? throw new InvalidOperationException("native_policy_card_missing");
                        int reference = cards.Component.GetProperty("piles")[pileIndex][index].GetInt32();
                        if (fingerprints.Cards[reference] != CombatBeamSolver.CaptureCardStateFingerprintForTesting(predicted))
                            throw new InvalidOperationException("original_card_fingerprint_mismatch:" + name + ":" + card.Id.Entry);
                        fingerprintCount++;
                        bool canPlay = projected.CanPlayCard(search, predicted, out int energyCost, out int starCost);
                        return (object)new { pile_index = pileIndex, index, card_id = card.Id.Entry,
                            can_play = canPlay, energy_cost = energyCost, star_cost = starCost };
                    })).ToArray();
                    EnvCombatPolicyProbe publicProbe = prediction.CapturePolicyProbe();
                    if (publicProbe.PolicyProjectionComplete || publicProbe.PredictionHash != hash
                        || !JsonNode.DeepEquals(JsonSerializer.SerializeToNode(policyCards),
                            JsonNode.Parse(publicProbe.Document.GetProperty("cards").GetRawText()))
                        || prediction.StateHash != hash)
                        throw new InvalidOperationException("public_policy_probe_mismatch:" + name);
                    File.WriteAllText(Path.Combine(output, name + ".native-policy.json"),
                        JsonSerializer.Serialize(policyCards));
                    File.WriteAllText(Path.Combine(output, name + ".public-policy-probe.json"),
                        publicProbe.Document.GetRawText());
                    Console.WriteLine($"original card fingerprints {name}: cards={fingerprintCount}; exact_encoder=true; complete_state_key=false");
                }
                Console.WriteLine($"native query fixture {name}: TestMode=false; full_live_root={name != "play_pile"}; explicit_native_silent_play_move={name == "play_pile"}");
                if (name == "blocked_hook")
                {
                    // Freeze after the native getter's lazy initialization,
                    // then run the actual game's downgrade independently.
                    EnvLiveCards beforeGrowth = EnvLiveCards.Capture(combat, player, creatures,
                        new Dictionary<CardModel, int>(ReferenceEqualityComparer.Instance));
                    JsonObject before = JsonNode.Parse(root)!.AsObject();
                    before["state"]!["cards"] = JsonNode.Parse(beforeGrowth.Component.GetProperty("cards").GetRawText());
                    before["state_hash"] = null;
                    using NativeEnv.Session canonicalGrowth = NativeEnv.ImportCombatSnapshot(before.ToJsonString());
                    File.WriteAllText(Path.Combine(output, "growth-before.root.json"), canonicalGrowth.ExportSnapshotJson());
                    CardModel[] growthCards = [rampage, thrash];
                    // Locate by object identity, not by card ID or slot order.
                    int[] references = growthCards.Select(card => beforeGrowth.Component.GetProperty("piles")[2]
                        [pcs.DiscardPile.Cards.ToList().IndexOf(card)].GetInt32()).ToArray();
                    foreach (CardModel card in growthCards) card.DowngradeInternal();
                    EnvLiveCards afterGrowth = EnvLiveCards.Capture(combat, player, creatures,
                        new Dictionary<CardModel, int>(ReferenceEqualityComparer.Instance));
                    File.WriteAllText(Path.Combine(output, "growth-downgrade.native.json"),
                        JsonSerializer.Serialize(references.Select(reference => new { card_ref = reference,
                            value = afterGrowth.Component.GetProperty("cards").GetProperty("slots").EnumerateArray()
                                .Single(slot => slot.GetProperty("index").GetInt32() == reference).GetProperty("value") }).ToArray()));
                }
            }
        }
        finally
        {
            cm.Reset(false); manager.CleanUp(); restore(); NonInteractiveMode.AutoSlayerCheck = () => false;
            SynchronizationContext.SetSynchronizationContext(null);
        }
    }

    private static JsonElement ReadNative(Player player, CombatState combat, EnvLiveCards capturedCards, ulong hash)
    {
        PlayerCombatState pcs = player.PlayerCombatState!;
        Creature[] roster = combat.Creatures.ToArray();
        Dictionary<Creature, int> creatureRefs = new(ReferenceEqualityComparer.Instance);
        Dictionary<PowerModel, int> powerRefs = new(ReferenceEqualityComparer.Instance);
        for (int i = 0; i < roster.Length; i++) creatureRefs.Add(roster[i], i + 1);
        int powerIndex = 0;
        foreach (Creature creature in roster) foreach (PowerModel power in creature.Powers) powerRefs.Add(power, ++powerIndex);
        object? Preventer(AbstractModel? preventer) => preventer switch
        {
            null => null,
            PowerModel power => new { kind = 2, dense_id = EnvModelCatalog.Id(power, "powers"), @ref = powerRefs[power] },
            _ => throw new InvalidOperationException("query_fixture_unexpected_preventer")
        };
        object Target(CardModel card, Creature? creature) => new { creature_ref = creature is null ? 0 : creatureRefs[creature],
            combat_id = creature?.CombatId is { } id ? (int?)id : null, is_valid_target = card.IsValidTarget(creature), can_play_targeting = card.CanPlayTargeting(creature) };
        object Card(CardModel card, int pile, int index)
        {
            bool canPlay = card.CanPlay(out UnplayableReason reason, out AbstractModel? preventer);
            CardEnergyCost energy = card.EnergyCost;
            return new { card_ref = capturedCards.Component.GetProperty("piles")[pile][index].GetInt32(), pile_index = pile,
                index, hand_index = pile == 0 ? index : -1,
                model_id = new { category = "CARD", entry = card.Id.Entry, dense_id = EnvModelCatalog.Id(card, "cards") },
                card_type = (int)card.Type, target_type = (int)card.TargetType,
                local_keywords = card.GetKeywordsWithSources(KeywordSources.Local).Order().Select(value => (int)value).ToArray(),
                effective_keywords = card.Keywords.Order().Select(value => (int)value).ToArray(),
                card_state = new { upgrade = card.CurrentUpgradeLevel, base_replay_count = card.BaseReplayCount,
                    exhaust_on_next_play = card.ExhaustOnNextPlay, sly = card.IsSlyThisTurn, retain = card.ShouldRetainThisTurn,
                    deck_version = card.DeckVersion is not null, removed = card.HasBeenRemovedFromState,
                    enchantment_id = card.Enchantment?.Id.Entry, affliction_id = card.Affliction?.Id.Entry,
                    dynamic_vars = card.DynamicVars.Select(pair => new {
                        name = pair.Key, is_calculated = pair.Value is CalculatedVar, is_string = pair.Value is StringVar,
                        base_value = new { decimal_bits = decimal.GetBits(pair.Value.BaseValue).Select(word => unchecked((uint)word)).ToArray() }
                    }).ToArray(),
                    subclass = card is Rampage or Thrash ? new { growth_damage = new {
                        decimal_bits = decimal.GetBits(card is Rampage growth ? growth._extraDamageFromPlays
                            : ((Thrash)card)._extraDamage).Select(word => unchecked((uint)word)).ToArray() }
                    } : null },
                energy_cost = new { raw = energy.GetWithModifiers(CostModifiers.None), local = energy.GetWithModifiers(CostModifiers.Local),
                    effective = energy.GetWithModifiers(CostModifiers.All), amount_to_spend = energy.GetAmountToSpend(), resolved = energy.GetResolved(),
                    costs_x = energy.CostsX, captured_x = energy.CostsX ? (int?)energy.CapturedXValue : null,
                    x_source = energy.CostsX ? "owner_energy" : null },
                star_cost = new { @base = card.BaseStarCost, local = card.CurrentStarCost, effective = card.GetStarCostWithModifiers(),
                    amount_to_spend = Math.Max(0, card.GetStarCostWithModifiers()), costs_x = card.HasStarCostX,
                    last_stars_spent = card.LastStarsSpent, x_source = card.HasStarCostX ? "owner_stars" : null },
                can_play = canPlay, unplayable_reason = (int)reason, preventer = Preventer(preventer),
                targets = new Creature?[] { null }.Concat(roster).Select(creature => Target(card, creature)).ToArray() };
        }
        object Creature(Creature creature)
        {
            MonsterModel? monster = creature.Monster;
            object? value = monster is null ? null : new {
                dense_id = EnvModelCatalog.Id(monster, "monsters"), entry = monster.Id.Entry, next_move_id = monster.NextMove.Id,
                stunned = monster.NextMove.Id == "STUNNED", intends_to_attack = monster.IntendsToAttack,
                intents = monster.NextMove.Intents.Select(intent => new { type = (int)intent.IntentType,
                    is_attack = intent is AttackIntent, multi = intent is MultiAttackIntent,
                    repeats = intent is AttackIntent attack ? attack.Repeats : 0,
                    strong = intent is DebuffIntent debuff && debuff._strong, status_count = intent is StatusIntent status ? status.CardCount : 0,
                    damage_raw = intent is AttackIntent damage ? new { decimal_bits = decimal.GetBits(damage.DamageCalc!()).Select(word => unchecked((uint)word)).ToArray() } : null }).ToArray() };
            return new { creature_ref = creatureRefs[creature], combat_id = (int)creature.CombatId!.Value,
                side = (int)creature.Side, is_alive = creature.IsAlive, monster = value };
        }
        return JsonSerializer.SerializeToElement(new { format = "sts2env.combat_prediction_queries", version = 4, prediction_hash = hash,
            query_semantics = "game", terminal = false, pending_choice = false,
            player = new { creature_ref = creatureRefs[player.Creature], energy = pcs.Energy, stars = pcs.Stars, max_energy = pcs.MaxEnergy,
                max_hand_size = CardPile.MaxCardsInHand, max_hand_size_source = "CardPile.MaxCardsInHand", turn = pcs.TurnNumber, phase = (int)pcs.Phase,
                can_pay_excess_energy_with_stars = Hook.ShouldPayExcessEnergyCostWithStars(combat, player) },
            piles = pcs.AllPiles.Select((pile, pileIndex) => new { type = (int)pile.Type, cards = pile.Cards.Select((card, index) => Card(card, pileIndex, index)).ToArray() }).ToArray(),
            roster = roster.Select(Creature).ToArray() });
    }
}
