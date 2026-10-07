using System.Diagnostics;
using System.Text.Json;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Models;
using CombatSolver.Engine.InCombat.Simulation;
using CombatSolver.Engine.Common;
using Sts2EnvCompare;
using CombatSolverEnv.Api;

namespace CombatSolver;

// Verification is scoped to one frozen native root, including portfolio runs.
// A failure disables Env verification for that root, not a native candidate.
internal sealed class EnvBranchVerificationContext(
    string seed, string encounter, int ascension, int actIndex, Action<string> report)
{
    private string? _failure;
    private long _checkedTransitions;
    private int _rootMonstersMatched;
    private int _liveRootImported;
    internal string Seed { get; } = seed;
    internal string Encounter { get; } = encounter;
    internal int Ascension { get; } = ascension;
    internal int ActIndex { get; } = actIndex;
    internal EnvLiveCards? LiveCards { get; set; }
    internal EnvLiveMonsters? LiveMonsters { get; set; }
    internal EnvLiveRunContext? LiveRun { get; set; }
    internal EnvLiveCreatures? LiveCreatures { get; set; }
    internal EnvLiveInventory? LiveInventory { get; set; }
    internal EnvLiveCombatContext? LiveCombat { get; set; }
    internal EnvLiveRoomContext? LiveRoom { get; set; }
    internal void RootMonstersMatched()
    {
        if (Interlocked.Exchange(ref _rootMonstersMatched, 1) == 0)
            report("BRANCH_ENV_ROOT_MONSTERS_MATCH scope=monster_rng_and_fsm source=live_vs_seed_snapshot_import full_root=false");
    }
    internal void MarkLiveRootImported()
    {
        if (Interlocked.Exchange(ref _liveRootImported, 1) == 0)
            report("BRANCH_ENV_LIVE_ROOT_IMPORTED source=live_snapshot_import full_root=false");
    }
    internal bool Disabled => Volatile.Read(ref _failure) is not null;
    internal bool LiveRootImported => Volatile.Read(ref _liveRootImported) != 0;
    internal string? FailureReason => Volatile.Read(ref _failure);

    internal void Reject(string reason, int turn, int actionCount)
    {
        if (Interlocked.CompareExchange(ref _failure, reason, null) is null)
            report($"BRANCH_ENV_DISABLED turn={turn} action_count={actionCount} reason={reason} " +
                   $"env_used=false scope=structural_and_combat_rng root_source={(LiveRootImported ? "live_snapshot_import" : "seed_snapshot_import")} native_search=continues");
    }

    internal void Checked(int actions, int turn, int actionCount, double elapsed)
    {
        long count = Interlocked.Add(ref _checkedTransitions, actions);
        if (count <= 8)
            report($"BRANCH_ENV_CHECK turn={turn} action_count={actionCount} transitions={count} " +
                   $"verification_ms={elapsed:F3} source_unchanged=true successor_match=true " +
                   $"scope=structural_and_combat_rng root_source={(LiveRootImported ? "live_snapshot_import" : "seed_snapshot_import")} backend=native_with_env_verification");
    }

    internal void Summarize(int expanded, int transitions)
        => report($"BRANCH_ENV_SUMMARY native_expanded={expanded} native_transitions={transitions} " +
                  $"env_checked_transitions_root_cumulative={Interlocked.Read(ref _checkedTransitions)} " +
                  $"env_disabled={Disabled} reason={FailureReason ?? "none"} " +
                  $"scope=structural_and_combat_rng root_source={(LiveRootImported ? "live_snapshot_import" : "seed_snapshot_import")} native_timing_includes_verification=true");
}

internal sealed partial class SimulationSnapshot
{
    internal EnvSearchBranch? EnvBranch { get; set; }
    internal bool EnvVerificationObserved { get; set; }
    internal int EnvActionCount { get; set; }
}

internal sealed partial class CombatBeamSolver
{
    private void VerifyEnvReplay(SimulationSnapshot snapshot, SimulationSnapshot? parent,
                                  IReadOnlyList<PlanAction> actions, bool startsAfterTurnSetup = false)
    {
        EnvBranchVerificationContext? context = root.EnvVerification;
        if (context is null || context.Disabled || snapshot.EnvVerificationObserved)
            return;
        // ResetCombat exposes the player play boundary. Native helpers can also
        // replay an empty pre-setup root, which has no equivalent Env boundary yet.
        if (parent is null && _includeTurnSetup && !startsAfterTurnSetup)
        {
            if (actions.Count != 0)
                context.Reject("pre_setup_replay_requires_root_adapter", snapshot.Turn, actions.Count);
            return;
        }
        if (startsAfterTurnSetup && snapshot.BoundaryReason != SearchBoundaryReason.None)
        {
            context.Reject($"turn_setup_boundary:{snapshot.BoundaryReason}", snapshot.Turn, 0);
            return;
        }
        snapshot.EnvVerificationObserved = true;
        snapshot.EnvActionCount = (parent?.EnvActionCount ?? 0) + actions.Count;
        EnvSearchBranch? owned = null;
        Stopwatch elapsed = Stopwatch.StartNew();
        try
        {
            EnvSearchBranch source;
            if (parent is null)
            {
                using NativeEnv.Session seeded = NativeEnv.OpenCombat(context.Seed, context.Encounter, context.Ascension, context.ActIndex);
                // Exercise the complete import boundary in the installed game.
                // This is still an explicitly seeded root, pending the live mapper.
                using NativeEnv.Session restored = NativeEnv.ImportCombatSnapshot(seeded.ExportSnapshotJson());
                if (restored.StateHash != seeded.StateHash)
                    throw new InvalidOperationException("root_snapshot_restore_changed_state");
                NativeEnv.Session ordinary = restored;
                NativeEnv.Session? imported = null;
                if (context.LiveRoom is not null)
                {
                    imported = NativeEnv.ImportCombatSnapshot(
                        EnvLiveRootAssembler.Compose(new EnvLiveRootComponents(
                            context.LiveCards!, context.LiveMonsters!, context.LiveRun!, context.LiveCreatures!,
                            context.LiveInventory!, context.LiveCombat!, context.LiveRoom), ordinary,
                            NativeEnv.ExportBlankTemplateJson(ordinary)));
                    ordinary = imported;
                    context.MarkLiveRootImported();
                }
                try
                {
                    owned = EnvSearchBranch.Open(EnvStateSnapshot.Parse(ordinary.ExportSnapshotJson()));
                    source = owned;
                    // Full replay starts at the frozen root, not the resulting node.
                    CombatPredictionSimulator nativeRoot = startsAfterTurnSetup ? snapshot.Simulator : root.ForkSimulator();
                    RequireStructuralMatch(source, CaptureNativeEnvRoot(nativeRoot), "root");
                    EnvRngVerification.RequireMatch(source, nativeRoot.Rng, "root");
                    if (context.LiveMonsters is { } frozenMonsters)
                    {
                        frozenMonsters.RequireMatch(ordinary);
                        context.RootMonstersMatched();
                    }
                }
                finally { imported?.Dispose(); }
            }
            else
            {
                if (parent.EnvBranch is null)
                {
                    context.Reject("parent_env_branch_unavailable", snapshot.Turn, snapshot.EnvActionCount);
                    return;
                }
                source = parent.EnvBranch;
            }

            foreach (PlanAction planned in actions)
            {
                EnvSearchBranch successor = source.Predict(planned, _player.Creature.CombatId);
                owned?.Dispose();
                owned = successor;
                source = successor;
            }
            RequireStructuralMatch(source, CaptureNativeEnvRoot(snapshot.Simulator), "successor");
            EnvRngVerification.RequireMatch(source, snapshot.Simulator.Rng, "successor");
            RequireTerminalMatch(source, snapshot.Simulator.TerminalStamp);
            RequireOriginalCardFingerprints(source, snapshot.Simulator);
            RequireOriginalOrbFingerprint(source, snapshot.Simulator);
            RequireOriginalPowerFingerprints(source, snapshot.Simulator);
            // An empty replay with a parent still requires independent ownership.
            if (owned is null)
                owned = source.Fork();
            snapshot.EnvBranch = owned;
            owned = null;
            context.Checked(actions.Count, snapshot.Turn, snapshot.EnvActionCount, elapsed.Elapsed.TotalMilliseconds);
        }
        catch (EnvPredictionUnportedException error)
        {
            context.Reject($"unported:{error.Message}", snapshot.Turn, snapshot.EnvActionCount);
        }
        catch (InvalidOperationException error)
        {
            context.Reject($"unavailable:{error.Message}", snapshot.Turn, snapshot.EnvActionCount);
        }
        finally
        {
            owned?.Dispose();
        }
    }

    private static void RequireStructuralMatch(EnvSearchBranch env, LiveRoot native, string boundary)
    {
        using JsonDocument json = JsonDocument.Parse(env.InspectJson());
        if (!json.RootElement.TryGetProperty("root", out JsonElement envRoot))
            throw new InvalidOperationException($"{boundary}:decision_adapter_missing:{json.RootElement.GetProperty("decision").GetInt32()}");
        string[] fields = native.DifferenceFields(envRoot);
        if (fields.Length > 0)
            throw new InvalidOperationException($"{boundary}_mismatch:{string.Join(",", fields)}" +
                (fields.Contains("hand")
                    ? $";native_hand={string.Join("|", native.Hand)};env_hand={string.Join("|", envRoot.GetProperty("hand").EnumerateArray().Select(card => card.GetString()))}"
                    : ""));
    }

    private static void RequireTerminalMatch(EnvSearchBranch env, CombatTerminalStamp? native)
    {
        EnvCombatTerminalStamp? actual = env.CaptureSnapshot().Terminal;
        if (native is null && actual is null) return;
        if (native is { } expected && actual is { } stamp
            && expected.PlayerTurn == stamp.PlayerTurn
            && (expected.Outcome == CombatTerminalOutcome.Victory)
                == (stamp.Outcome == EnvCombatTerminalOutcome.Victory)) return;
        throw new InvalidOperationException($"terminal_stamp_mismatch:native={native}:env={actual}");
    }

    private static void RequireOriginalPowerFingerprints(EnvSearchBranch env, CombatPredictionSimulator native)
    {
        var fingerprints = env.CapturePowerFingerprints();
        List<JsonElement> remaining = env.CapturePowerQueries().Document.GetProperty("powers").EnumerateArray().Where(row => row.GetProperty("amount").GetInt32() != 0).ToList();
        var combat = (SimulatedCombatState)native.State.CombatState;
        foreach (PowerModel power in combat.EffectivePowers())
        {
            if (power.Amount == 0) continue;
            StateFingerprint expected = SimulatedCombatState.CaptureOriginalPowerFingerprintForEnvVerification(power, native);
            int index = remaining.FindIndex(row => row.GetProperty("power_id").GetString() == power.Id.Entry
                && row.GetProperty("owner_combat_id").GetUInt32() == (power.Owner.CombatId ?? uint.MaxValue)
                && fingerprints.Powers[row.GetProperty("power_ref").GetInt32()] == expected);
            if (index < 0) throw new InvalidOperationException("original_power_fingerprint_mismatch:" + power.Id.Entry);
            remaining.RemoveAt(index);
        }
        if (remaining.Count != 0) throw new InvalidOperationException("original_power_fingerprint_roster_mismatch");
        if (fingerprints.Aggregate != combat.CaptureOriginalPowerAggregateForEnvVerification(native))
            throw new InvalidOperationException("original_power_order_or_retired_slots_mismatch");
    }

    private void RequireOriginalOrbFingerprint(EnvSearchBranch env, CombatPredictionSimulator native)
    {
        StateFingerprintBuilder key = new();
        AppendOrbs(ref key, native, native.State.GetPlayerCombatState(_player).OrbQueue);
        if (env.CaptureOrbFingerprint().Fingerprint != key.Finish())
            throw new InvalidOperationException("original_orb_fingerprint_mismatch");
    }

    private void RequireOriginalCardFingerprints(EnvSearchBranch env, CombatPredictionSimulator native)
    {
        EnvSearchCardFingerprintProjection fingerprints = env.CaptureCardFingerprints();
        JsonElement piles = env.CaptureGameQueries().Document.GetProperty("piles");
        SimPlayerCombatState player = native.State.GetPlayerCombatState(_player);
        if (piles.GetArrayLength() != player.AllPiles.Count)
            throw new InvalidOperationException("original_card_fingerprint_pile_count_mismatch");
        for (int pile = 0; pile < player.AllPiles.Count; pile++)
        {
            JsonElement cards = piles[pile].GetProperty("cards");
            if (cards.GetArrayLength() != player.AllPiles[pile].Cards.Count)
                throw new InvalidOperationException($"original_card_fingerprint_card_count_mismatch:{pile}");
            for (int index = 0; index < cards.GetArrayLength(); index++)
            {
                int reference = cards[index].GetProperty("card_ref").GetInt32();
                if (fingerprints.Cards[reference] != CaptureCardStateFingerprintForTesting(player.AllPiles[pile].Cards[index]))
                    throw new InvalidOperationException($"original_card_fingerprint_mismatch:{pile}:{index}");
            }
        }
    }

    private LiveRoot CaptureNativeEnvRoot(CombatPredictionSimulator simulator)
    {
        SimulatedCombatState combat = (SimulatedCombatState)simulator.State.CombatState;
        SimPlayerCombatState player = simulator.State.GetPlayerCombatState(_player);
        SimCreatureState creature = simulator.State.GetCreature(_player.Creature);
        CardModel[] Cards(SimCardPile pile) => pile.Cards.Select(card => card.Preview).ToArray();
        string[] Ids(CardModel[] cards) => cards.Select(card => card.Id.Entry).ToArray();
        string[] States(CardModel[] cards) => cards.Select(LiveRoot.CardStateKey).ToArray();
        string[] Powers(Creature owner) => combat.EffectivePowers()
            .Where(power => ReferenceEquals(power.Owner, owner))
            .Select(power => $"{power.Id.Entry}:{power.Amount}").ToArray();
        CardModel[] hand = Cards(player.Hand), draw = Cards(player.DrawPile),
                    discard = Cards(player.DiscardPile), exhaust = Cards(player.ExhaustPile);
        return new LiveRoot(Ids(hand), Ids(draw), Ids(discard), Ids(exhaust),
            States(hand), States(draw), States(discard), States(exhaust),
            combat.RelicsOf(_player).Select(relic => $"{relic.Id.Entry}:{(relic.IsMelted ? 1 : 0)}").ToArray(),
            Enumerable.Range(0, root.PotionSlotCount)
                .Select(slot => combat.GetPotionAtSlot(_player, slot)?.Id.Entry ?? "").ToArray(),
            Powers(_player.Creature), combat.Enemies.Select(enemy =>
            {
                SimCreatureState predicted = simulator.State.GetCreature(enemy);
                return new EnemyRoot(enemy.CombatId ?? 0, enemy.Monster!.Id.Entry,
                    predicted.CurrentHp, predicted.MaxHp, predicted.Block, Powers(enemy));
            }).ToArray(), player.Energy, creature.CurrentHp, creature.MaxHp, creature.Block);
    }
}
