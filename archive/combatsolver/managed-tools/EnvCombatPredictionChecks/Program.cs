using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.Loader;
using System.Text.Json;
using CombatSolver;
using CombatSolver.Engine.InCombat.Simulation;
using CombatSolver.Engine.InCombat.Mirrors.Cards.OnPlay;
using CombatSolverEnv.Api;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.TestSupport;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Localization.DynamicVars;
using Sts2EnvCompare;

internal static class Program
{
    private static void Main(string[] args)
    {
        GameBootstrap.InstallResolver(GameBootstrap.DefaultGameData);
        AssemblyLoadContext.Default.Resolving += (context, name) =>
        {
            foreach (string key in new[] { "RitsuCompatDirectory", "RitsuSharedDirectory" })
            {
                string directory = typeof(Program).Assembly.GetCustomAttributes<AssemblyMetadataAttribute>().Single(item => item.Key == key).Value!;
                string path = Path.Combine(directory, name.Name + ".dll");
                if (File.Exists(path)) return context.LoadFromAssemblyPath(path);
            }
            return null;
        };
        bool nativeOnly = args.Contains("--native-only", StringComparer.Ordinal);
        int selection = Array.IndexOf(args, "--scenario");
        string? scenario = selection < 0 ? null : selection + 1 < args.Length ? args[selection + 1]
            : throw new ArgumentException("--scenario requires a scenario name");
        int output = Array.IndexOf(args, "--out");
        string? directory = output < 0 ? null : output + 1 < args.Length ? args[output + 1]
            : throw new ArgumentException("--out requires a directory");
        Check(nativeOnly, scenario, directory);
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static void Check(bool nativeOnly, string? selectedScenario, string? directory)
    {
        GameBootstrap.Init(testMode: false);
        GameBootstrap.LoadLocalization();
        GameBootstrap.UseInMemorySaves();
        if (TestMode.IsOn) throw new InvalidOperationException("terminal_fixture_requires_TestMode_off");
        typeof(CombatSolver.Entry).GetProperty(nameof(CombatSolver.Entry.Logger))!.SetValue(null,
            new CombatSolverLog(Path.Combine(Path.GetTempPath(), "sts2env-combat-prediction-checks")));
        SolverController.DisplayServerNameProvider = static () => "headless";
        NativeEnv.Configure();
        try
        {
            if (directory is not null) Directory.CreateDirectory(directory);
            string[] scenarios = ["strike", "rampage", "rampage_then_turn", "rampage_repeat", "body_slam", "enemy", "phase_one", "poison", "enemy_end", "forced_end", "fire_potion", "player_poison", "extra_turn_poison", "enemy_end_poison", "power_retire", "power_retire_reacquire"];
            if (selectedScenario is not null && !scenarios.Contains(selectedScenario, StringComparer.Ordinal))
                throw new ArgumentException("unknown --scenario:" + selectedScenario);
            foreach (string scenario in scenarios.Where(scenario => selectedScenario is null || selectedScenario == scenario))
            {
                using ScenarioFixture fixture = new(scenario);
                using var native = fixture.Replay();
                Console.WriteLine($"beam_env_node scenario={scenario} observed={native.EnvVerificationObserved} owned_prediction_branch={native.EnvBranchAttached} projection_history={native.EnvProjectionHistoryCount} policy_cards={native.EnvPolicyCardCount} threat_creatures={native.EnvThreatCreatureCount} original_card_fingerprints={native.EnvOriginalCardFingerprintCount} original_key_ready={native.EnvProjectionHasOriginalKey} reason={native.EnvFailureReason ?? "none"}");
                if (!nativeOnly && (!native.EnvVerificationObserved || !native.EnvBranchAttached || native.EnvFailureReason is not null))
                    throw new InvalidOperationException("beam_owned_env_verification_failed:" + scenario + ":" + native.EnvFailureReason);
                if (!nativeOnly && (native.EnvUnorderedPileKey != native.NativeUnorderedPileKey
                    || native.EnvPotionInventoryKey != native.NativePotionInventoryKey
                    || native.EnvCyclePileShapeKey != native.NativeCyclePileShapeKey
                    || native.EnvCycleShapeKey != native.NativeCycleShapeKey
                    || native.EnvStateKeyPileSegment != native.NativeStateKeyPileSegment))
                    throw new InvalidOperationException("beam_search_key_component_mismatch:" + scenario
                        + ":native_unordered=" + native.NativeUnorderedPileKey
                        + ":env_unordered=" + native.EnvUnorderedPileKey
                        + ":native_potion_inventory=" + native.NativePotionInventoryKey
                        + ":env_potion_inventory=" + native.EnvPotionInventoryKey
                        + ":native_cycle_pile=" + native.NativeCyclePileShapeKey
                        + ":env_cycle_pile=" + native.EnvCyclePileShapeKey
                        + ":native_cycle=" + native.NativeCycleShapeKey
                        + ":env_cycle=" + native.EnvCycleShapeKey
                        + ":native_state_pile=" + native.NativeStateKeyPileSegment
                        + ":env_state_pile=" + native.EnvStateKeyPileSegment);
                JsonElement expected = SemanticProjection.Native(native.Simulator, fixture);
                if (directory is not null)
                {
                    File.WriteAllText(Path.Combine(directory, scenario + ".root.json"), fixture.RootJson);
                    File.WriteAllText(Path.Combine(directory, scenario + ".native.json"), expected.GetRawText());
                    // Baseline for P2 policy history work. Keep this separate
                    // from the semantic state projection until Env emits the
                    // same ordered entries and all six counters.
                    var history = native.Simulator.History;
                    File.WriteAllText(Path.Combine(directory, scenario + ".native-history.json"),
                        JsonSerializer.Serialize(new
                        {
                            entries = history.Entries.Select(entry => new
                            {
                                entry.Index,
                                kind = entry.GetType().Name,
                                card = entry switch
                                {
                                    CombatPredictionCardPlayStartedEntry started => started.Card.Id,
                                    CombatPredictionCardPlayFinishedEntry finished => finished.Card.Id,
                                    CombatPredictionCardDrawnEntry drawn => drawn.Card.Id,
                                    CombatPredictionCardDrawResolvedEntry resolved => resolved.Card.Id,
                                    _ => null
                                },
                                receiver_combat_id = entry is CombatPredictionDamageReceivedEntry damage
                                    ? damage.Receiver.CombatId : null,
                                unblocked_damage = entry is CombatPredictionDamageReceivedEntry dealt
                                    ? dealt.Result.UnblockedDamage : (int?)null,
                                risk = entry is CombatPredictionRiskEntry risk ? risk.Reason.ToString() : null
                            }).ToArray(),
                            counters = history.GetCounters(fixture.Player)
                        }));
                }
                if (nativeOnly)
                {
                    Console.WriteLine($"native {scenario}: {expected.GetRawText()}");
                    continue;
                }
                using NativeEnv.Session ordinaryRoot = NativeEnv.ImportCombatSnapshot(fixture.RootJson);
                EnvStateSnapshot canonicalRoot = EnvStateSnapshot.Parse(ordinaryRoot.ExportSnapshotJson());
                using EnvCombatPredictionSession source = EnvPredictionApi.OpenCombatPrediction(canonicalRoot);
                string before = source.CaptureSnapshot().Document.GetRawText();
                ulong hash = source.StateHash;
                using EnvCombatPredictionSession branch = PredictChain(source, fixture.Actions);
                EnvCombatPredictionSnapshot result = branch.CaptureSnapshot();
                if (scenario == "rampage") CheckUnsupportedInferencePolicy(result);
                if (scenario == "body_slam") CompareTerminalCalculatedVars(native.Simulator, fixture, branch);
                CompareSupportedHistory(native.Simulator.History, fixture.Player, result, scenario);
                JsonElement actual = SemanticProjection.Env(result, fixture);
                if (directory is not null)
                {
                    File.WriteAllText(Path.Combine(directory, scenario + ".env.json"), actual.GetRawText());
                    File.WriteAllText(Path.Combine(directory, scenario + ".snapshot.json"), result.Document.GetRawText());
                    File.WriteAllText(Path.Combine(directory, scenario + ".mod-history.json"),
                        EnvSearchHistoryProjection.From(result.Document).Document.GetRawText());
                }
                if (scenario == "enemy_end_poison")
                {
                    JsonElement vanilla = fixture.ObserveVanillaPlayerPoison();
                    if (directory is not null) File.WriteAllText(Path.Combine(directory, scenario + ".vanilla-trace.json"), vanilla.GetRawText());
                    Console.WriteLine("vanilla player poison timing only: HP3 to HP1; real AfterSideTurnStart entered at T+1, player side, after drawing 5 cards; TestMode=false; shadow_successor=false");
                }
                RequireSame(expected, actual, scenario);
                if (source.StateHash != hash || source.CaptureSnapshot().Document.GetRawText() != before)
                    throw new InvalidOperationException("combat_prediction_changed_source:" + scenario);
                using EnvCombatPredictionSession restored = EnvPredictionApi.RestoreCombatPrediction(result);
                if (restored.StateHash != branch.StateHash
                    || !JsonElement.DeepEquals(restored.CaptureSnapshot().Document, result.Document))
                    throw new InvalidOperationException("combat_prediction_roundtrip_changed_document:" + scenario);
                if (fixture.ExpectTerminal)
                {
                    if (branch.Terminal is null || branch.LegalActions().Count != 0)
                        throw new InvalidOperationException("combat_prediction_terminal_not_stationary:" + scenario);
                    EnvPredictionStatus rejected = branch.TryPredict(new(EnvCombatActionKind.EndTurn), out EnvCombatPredictionSession? denied, out string? reason);
                    denied?.Dispose();
                    if (rejected != EnvPredictionStatus.Error || denied is not null || string.IsNullOrEmpty(reason)
                        || branch.StateHash != result.PredictionHash
                        || !JsonElement.DeepEquals(branch.CaptureSnapshot().Document, result.Document))
                        throw new InvalidOperationException("combat_prediction_terminal_failure_changed_source:" + scenario);
                }
                else if (branch.Terminal is not null || branch.LegalActions().Count == 0)
                    throw new InvalidOperationException("combat_prediction_nonterminal_boundary_mismatch:" + scenario);
                Console.WriteLine($"passed {scenario}: terminal={branch.Terminal}; semantic_projection=true; full_state_comparison=false");
            }
            Console.WriteLine($"combat prediction checks: compared=terminal,flags,player_turn,energy,stars,phase,round,side,hp,max_hp,block,hp_display,attachment,power_ids_amounts,enemy_roster,pile_card_ids_upgrades_attachments,potion_slots,9_rng_full_states,history_entry_kinds_including_risk,history_risk_reasons,history_six_counters,card_snapshot_and_play_fields,draw_kind,damage_result_fields,damage_source_kind_and_id,attack_hit_result_fields; full_state_comparison=false; full_history_payload_comparison=false; TestMode=false; native_only={nativeOnly}; installed_game_acceptance=false");
        }
        finally { CombatSolver.Entry.Logger.Journal.Dispose(); }
    }

    private static void CheckUnsupportedInferencePolicy(EnvCombatPredictionSnapshot snapshot)
    {
        bool original = CardOnPlayMirrors.AllowInference;
        try
        {
            CardOnPlayMirrors.AllowInference = false;
            try { _ = EnvSearchHistoryProjection.From(snapshot.Document); }
            catch (EnvProjectionIncompleteException error)
                when (error.Message == "env_history_default_inference_policy_required")
            {
                Console.WriteLine("Mod history: nondefault inference policy explicitly refused");
                return;
            }
            throw new InvalidOperationException("mod_history_accepted_unaudited_inference_policy");
        }
        finally { CardOnPlayMirrors.AllowInference = original; }
    }

    private static void CompareTerminalCalculatedVars(CombatPredictionSimulator simulator,
        ScenarioFixture fixture, EnvCombatPredictionSession branch)
    {
        EnvCombatCalculatedVars values = branch.CaptureCalculatedVars();
        var player = simulator.State.GetPlayerCombatState(fixture.Player);
        var piles = new[] { player.Hand, player.DrawPile, player.DiscardPile, player.ExhaustPile, player.PlayPile };
        JsonElement state = branch.CaptureSnapshot().Document.GetProperty("inner_snapshot").GetProperty("state");
        var creatures = state.GetProperty("creatures").GetProperty("slots").EnumerateArray().ToDictionary(
            slot => slot.GetProperty("index").GetInt32(), slot => slot.GetProperty("value").GetProperty("combat_id").GetInt32());
        int count = 0;
        foreach (JsonElement row in values.Document.GetProperty("cards").EnumerateArray())
        {
            var card = piles[row.GetProperty("pile_index").GetInt32()].Cards[row.GetProperty("index").GetInt32()];
            if (card.Preview.Id.Entry != row.GetProperty("card_id").GetString())
                throw new InvalidOperationException("terminal_calculated_card_identity_mismatch");
            CalculatedVar variable = (CalculatedVar)card.Preview.DynamicVars[row.GetProperty("variable").GetString()!];
            foreach (JsonElement target in row.GetProperty("values").EnumerateArray())
            {
                int reference = target.GetProperty("target_ref").GetInt32();
                var creature = reference == 0 ? null : fixture.RootCreatures.Single(value => value.CombatId == creatures[reference]);
                if (!CalculatedVarSpecRegistry.TryCalculate(variable, simulator, card, creature, out decimal prediction))
                    throw new InvalidOperationException("terminal_original_calculated_spec_missing");
                // The native game getter reads the global CombatManager. A
                // controlled inactive flag reproduces its documented terminal
                // branch without changing the retained original predictor.
                bool active = CombatManager.Instance._turnState!.IsInProgress;
                decimal game;
                try { CombatManager.Instance._turnState.IsInProgress = false; game = variable.Calculate(creature); }
                finally { CombatManager.Instance._turnState.IsInProgress = active; }
                int cardRef = row.GetProperty("card_ref").GetInt32();
                if (!decimal.GetBits(prediction).SequenceEqual(decimal.GetBits(values.GetValue(cardRef, variable.Name,
                        reference, EnvCalculatedValueSemantics.OriginalPrediction)))
                    || !decimal.GetBits(game).SequenceEqual(decimal.GetBits(values.GetValue(cardRef, variable.Name,
                        reference, EnvCalculatedValueSemantics.Game))) || game == prediction)
                    throw new InvalidOperationException("terminal_game_and_original_calculated_values_mismatch");
                count++;
            }
        }
        if (count == 0) throw new InvalidOperationException("terminal_calculated_comparison_empty");
        Console.WriteLine($"terminal calculated vars: values={count}; game_and_original_prediction_exact_bits=true; distinct_semantics=true");
    }

    // Exact ordered entry kinds and the original state-key counters are a
    // first policy-history gate. No entry kinds are excluded. Known OnPlay
    // risk reasons must match the pinned original; other payloads and policy
    // dispatch sites remain incomplete while the adapter is unfinished.
    private static void CompareSupportedHistory(CombatPredictionHistory native, Player owner,
        EnvCombatPredictionSnapshot env, string scenario)
    {
        if (env.HistoryProjectionComplete)
            throw new InvalidOperationException("history_claims_complete_before_policy_adapter:" + scenario);
        string[] names = ["CardPlayStarted", "CardPlayFinished", "DamageReceived", "CreatureAttacked",
            "CardDrawn", "CardDrawResolved", "CardGenerated", "CardGenerationResolved", "OrbChanneled", "Risk"];
        CombatPredictionHistoryEntry[] nativeEntries = native.Entries.ToArray();
        JsonElement projectedHistory = EnvSearchHistoryProjection.From(env.Document).Document;
        JsonElement[] envEntries = projectedHistory.GetProperty("events").EnumerateArray().ToArray();
        string[] nativeKinds = nativeEntries
            .Select(entry => entry.GetType().Name.Replace("CombatPrediction", "").Replace("Entry", ""))
            .ToArray();
        string[] envKinds = envEntries
            .Select(entry => names[entry[0].GetInt32()]).ToArray();
        if (!nativeKinds.SequenceEqual(envKinds))
            throw new InvalidOperationException("history_kind_sequence_mismatch:" + scenario + ":native="
                + string.Join(',', nativeKinds) + ":env=" + string.Join(',', envKinds));
        Dictionary<int, int> combatIdByRef = env.Document.GetProperty("inner_snapshot")
            .GetProperty("state").GetProperty("creatures").GetProperty("slots")
            .EnumerateArray().ToDictionary(slot => slot.GetProperty("index").GetInt32(),
                slot => slot.GetProperty("value").GetProperty("combat_id").GetInt32());
        string CardName(int id) => EnvModelCatalog.Content.GetProperty("cards").EnumerateObject()
            .Single(item => item.Value.GetProperty("id").GetInt32() == id).Name;
        for (int index = 0; index < nativeEntries.Length; index++)
        {
            JsonElement eventEntry = envEntries[index];
            if (nativeEntries[index] is CombatPredictionRiskEntry risk
                && eventEntry[20].GetInt32() != (int)risk.Reason)
                throw new InvalidOperationException("history_risk_reason_mismatch:" + scenario + ":" + index);
            if (nativeEntries[index].Index != index)
                throw new InvalidOperationException("native_history_index_mismatch:" + scenario + ":" + index);
            if (nativeEntries[index] is CombatPredictionCardDrawResolvedEntry drawnResolved
                && eventEntry[9].GetInt32() != drawnResolved.OriginalEntry.Index)
                throw new InvalidOperationException("history_draw_parent_mismatch:" + scenario + ":" + index);
            if (nativeEntries[index] is CombatPredictionCardGenerationResolvedEntry generatedResolved
                && eventEntry[9].GetInt32() != generatedResolved.OriginalEntry.Index)
                throw new InvalidOperationException("history_generation_parent_mismatch:" + scenario + ":" + index);
            CombatPredictionCardSnapshot? card = nativeEntries[index] switch
            {
                CombatPredictionCardPlayStartedEntry started => started.Card,
                CombatPredictionCardPlayFinishedEntry finished => finished.Card,
                CombatPredictionCardDrawnEntry drawn => drawn.Card,
                CombatPredictionCardDrawResolvedEntry resolved => resolved.Card,
                CombatPredictionCardGeneratedEntry generated => generated.Card,
                CombatPredictionCardGenerationResolvedEntry generated => generated.Card,
                CombatPredictionDamageReceivedEntry { CardSource: { } source } => source,
                _ => null,
            };
            if (card is { } snapshot)
            {
                JsonElement facts = eventEntry[17];
                if (facts.ValueKind != JsonValueKind.Array
                    || !ReferenceEquals(snapshot.Owner, owner)
                    || facts[0].GetInt32() != 1
                    || CardName(eventEntry[2].GetInt32()) != snapshot.Id
                    || facts[1].GetInt32() != snapshot.UpgradeLevel
                    || facts[2].GetInt32() != (int)snapshot.Type)
                    throw new InvalidOperationException("history_card_snapshot_mismatch:" + scenario + ":" + index);
            }
            if (nativeEntries[index] is CombatPredictionCardPlayStartedEntry or CombatPredictionCardPlayFinishedEntry)
            {
                MegaCrit.Sts2.Core.Entities.Cards.CardPlay play = nativeEntries[index] switch
                {
                    CombatPredictionCardPlayStartedEntry started => started.CardPlay,
                    CombatPredictionCardPlayFinishedEntry finished => finished.CardPlay,
                    _ => throw new InvalidOperationException("unreachable card play history kind"),
                };
                JsonElement facts = eventEntry[18];
                int target = play.Target?.CombatId is { } combatId ? checked((int)combatId) : -1;
                int projectedTarget = facts[0].GetInt32() == 0 ? -1 : combatIdByRef[facts[0].GetInt32()];
                if (eventEntry[3].GetInt32() != 1 || !ReferenceEquals(play.Player, owner)
                    || target != projectedTarget || facts[1].GetInt32() != (int)play.ResultPile
                    || facts[2].GetInt32() != play.Resources.EnergySpent
                    || facts[3].GetInt32() != play.Resources.EnergyValue
                    || facts[4].GetInt32() != play.Resources.StarsSpent
                    || facts[5].GetInt32() != play.Resources.StarValue
                    || facts[6].GetBoolean() != play.IsAutoPlay
                    || facts[7].GetInt32() != play.PlayIndex
                    || facts[8].GetInt32() != play.PlayCount)
                    throw new InvalidOperationException("history_card_play_mismatch:" + scenario + ":" + index
                        + ":native=" + JsonSerializer.Serialize(new { player = 1, target,
                            resultPile = (int)play.ResultPile, play.Resources.EnergySpent,
                            play.Resources.EnergyValue, play.Resources.StarsSpent, play.Resources.StarValue,
                            play.IsAutoPlay, play.PlayIndex, play.PlayCount })
                        + ":env=" + facts.GetRawText());
            }
            if (nativeEntries[index] is CombatPredictionCardDrawnEntry drawnEntry
                && eventEntry[11].GetBoolean() != drawnEntry.FromHandDraw)
                throw new InvalidOperationException("history_draw_kind_mismatch:" + scenario + ":" + index);
            if (nativeEntries[index] is CombatPredictionCardPlayFinishedEntry finishedEntry
                && eventEntry[11].GetBoolean() != finishedEntry.WasEthereal)
                throw new InvalidOperationException("history_ethereal_mismatch:" + scenario + ":" + index);
            if (nativeEntries[index] is CombatPredictionDamageReceivedEntry damage)
            {
                if (!SameHit(eventEntry, damage.Result, combatIdByRef, eventEntry[4].GetInt32(), 6, 7, 8, 12, 13, 14, 15)
                    || combatIdByRef[eventEntry[4].GetInt32()] != checked((int)damage.Receiver.CombatId!.Value))
                    throw new InvalidOperationException("history_damage_payload_mismatch:" + scenario + ":" + index);
                JsonElement source = eventEntry[19];
                if (source.ValueKind != JsonValueKind.Array
                    || source[0].GetInt32() != (int)damage.Source.Kind
                    || source[1].GetString() != (damage.Source.Id ?? ""))
                    throw new InvalidOperationException("history_damage_source_mismatch:" + scenario + ":" + index
                        + ":native=" + damage.Source + ":env=" + source.GetRawText());
            }
            else if (nativeEntries[index] is CombatPredictionCreatureAttackedEntry attack)
            {
                JsonElement[] envHits = eventEntry[16].EnumerateArray().ToArray();
                if (combatIdByRef[eventEntry[4].GetInt32()] != checked((int)attack.Attacker.CombatId!.Value)
                    || eventEntry[10].GetInt32() != attack.HitResults.Count
                    || envHits.Length != attack.HitResults.Count)
                    throw new InvalidOperationException("history_attack_shape_mismatch:" + scenario + ":" + index);
                for (int hit = 0; hit < envHits.Length; hit++)
                {
                    JsonElement value = envHits[hit];
                    if (!SameHit(value, attack.HitResults[hit], combatIdByRef, value[0].GetInt32(), 2, 3, 4, 1, 5, 6, 7))
                        throw new InvalidOperationException("history_attack_hit_mismatch:" + scenario + ":" + index + ":" + hit);
                }
            }
        }
        CombatHistoryCounters c = native.GetCounters(owner);
        int[] nativeCounters = [c.FinishedPlays, c.EtherealPlays, c.LightningChannels,
            c.UnblockedHitsReceived, c.CardsDrawn, c.CardsGenerated];
        int[] envCounters = projectedHistory.GetProperty("counters").EnumerateArray()
            .Select(value => value.GetInt32()).ToArray();
        if (!nativeCounters.SequenceEqual(envCounters))
            throw new InvalidOperationException("history_counter_mismatch:" + scenario);
    }

    private static bool SameHit(JsonElement value, MegaCrit.Sts2.Core.Entities.Creatures.DamageResult native,
        Dictionary<int, int> combatIdByRef, int receiverRef,
        int blocked, int unblocked, int overkill, int props, int blockBroken, int fullyBlocked, int targetKilled)
        => combatIdByRef[receiverRef] == checked((int)native.Receiver.CombatId!.Value)
            && value[blocked].GetInt32() == native.BlockedDamage
            && value[unblocked].GetInt32() == native.UnblockedDamage
            && value[overkill].GetInt32() == native.OverkillDamage
            && value[props].GetInt32() == (int)native.Props
            && value[blockBroken].GetBoolean() == native.WasBlockBroken
            && value[fullyBlocked].GetBoolean() == native.WasFullyBlocked
            && value[targetKilled].GetBoolean() == native.WasTargetKilled;

    private static EnvCombatPredictionSession PredictChain(EnvCombatPredictionSession source, EnvCombatAction[] actions)
    {
        EnvCombatPredictionSession current = source.Clone();
        try
        {
            foreach (EnvCombatAction action in actions)
            {
                EnvCombatPredictionSession successor = current.Predict(action);
                current.Dispose(); current = successor;
            }
            return current;
        }
        catch { current.Dispose(); throw; }
    }

    private static void RequireSame(JsonElement expected, JsonElement actual, string path)
    {
        if (JsonElement.DeepEquals(expected, actual)) return;
        if (expected.ValueKind == JsonValueKind.Object && actual.ValueKind == JsonValueKind.Object)
            foreach (JsonProperty item in expected.EnumerateObject())
            {
                if (!actual.TryGetProperty(item.Name, out JsonElement value)) throw new InvalidOperationException("missing_projection:" + path + "." + item.Name);
                RequireSame(item.Value, value, path + "." + item.Name);
            }
        else if (expected.ValueKind == JsonValueKind.Array && actual.ValueKind == JsonValueKind.Array && expected.GetArrayLength() == actual.GetArrayLength())
            for (int index = 0; index < expected.GetArrayLength(); index++) RequireSame(expected[index], actual[index], path + "[" + index + "]");
        throw new InvalidOperationException($"combat_projection_mismatch:{path};native={expected.GetRawText()};env={actual.GetRawText()}");
    }
}
