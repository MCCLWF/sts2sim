using System.Diagnostics;
using System.Text.Json;
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
using MegaCrit.Sts2.Core.Map;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Encounters;
using MegaCrit.Sts2.Core.Models.Potions;
using MegaCrit.Sts2.Core.Models.Powers;
using MegaCrit.Sts2.Core.Models.Relics;
using MegaCrit.Sts2.Core.MonsterMoves.MonsterMoveStateMachine;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Unlocks;
using Sts2EnvCompare;

// Explicit supported fixtures, composed solely from live native game objects.
// Native successors come from the pinned upstream predictor's Replay method;
// no live game recapture is used as a replacement for shadow successor state.
internal sealed class ScenarioFixture : IDisposable
{
    private readonly SingleThreadContext _sync = SingleThreadContext.Install();
    private readonly Action _restorePresentation;
    private readonly RunManager _manager = RunManager.Instance;
    private readonly CombatManager _combatManager = CombatManager.Instance;
    internal Player Player { get; }
    internal Creature Enemy { get; }
    internal Creature[] RootCreatures { get; }
    internal string RootJson { get; }
    internal EnvCombatAction[] Actions { get; }
    internal bool ExpectTerminal { get; }
    private readonly CombatBeamSolver _solver;
    private readonly EnvBranchVerificationContext? _verification;
    private readonly PlanAction[] _plans;
    private readonly CombatState _liveCombat;

    internal ScenarioFixture(string scenario)
    {
        ExpectTerminal = scenario is not ("player_poison" or "extra_turn_poison" or "rampage"
            or "rampage_then_turn" or "rampage_repeat"
            or "power_retire" or "power_retire_reacquire");
        NonInteractiveMode.AutoSlayerCheck = () => true;
        _restorePresentation = Headless.InstallLiveCombatPresentation();
        Player = Player.CreateForNewRun<Ironclad>(UnlockState.all, 1);
        RunState run = RunState.CreateForNewRun([Player], ModelDb.ActsByIndex.Select(acts => acts[0].ToMutable()).ToList(),
            [], GameMode.Standard, 0, "LIVEROOT001");
        try
        {
            _manager.SetUpNewSingleplayer(run, shouldSave: false);
            Await(_manager.FinalizeStartingRelics());
            _manager.Launch();
            run.Map = StandardActMap.CreateFor(run, false);
            _manager.CombatReplayWriter.IsEnabled = false;
            EncounterModel encounter = ModelDb.Encounter<CultistsNormal>().ToMutable();
            encounter.GenerateMonstersWithSlots(run);
            CombatState combat = new(encounter, run);
            _liveCombat = combat;
            combat.AddPlayer(Player);
            foreach (var (monster, slot) in encounter.MonstersWithSlots)
                combat.AddCreature(combat.CreateCreature(monster, CombatSide.Enemy, slot));
            run.PushRoom(new CombatRoom(combat));
            _combatManager.SetUpCombat(combat);
            foreach (Creature creature in combat.Creatures) Await(_combatManager.AfterCreatureAdded(creature));
            _combatManager._turnState!.IsInProgress = true;
            _combatManager._turnState.IsStarting = false;
            Await(Hook.BeforeCombatStart(run, combat));
            Await(_combatManager.StartTurn(_combatManager._turnState));
            Enemy = combat.Enemies[0];
            Player.Creature.CurrentHp -= 10; // Burning Blood would heal on ordinary victory teardown.
            PlayerCombatState pcs = Player.PlayerCombatState!;
            CardModel? play = null;
            switch (scenario)
            {
                case "body_slam":
                    Player.Creature.Block = 13;
                    foreach (Creature enemy in combat.Enemies) enemy.CurrentHp = 1;
                    pcs.Hand.AddInternal(combat.CreateCard<BodySlam>(Player), silent: true);
                    play = combat.CreateCard<BodySlam>(Player);
                    pcs.Hand.AddInternal(play, silent: true);
                    break;
                case "rampage":
                case "rampage_then_turn":
                case "rampage_repeat":
                    Rampage growth = combat.CreateCard<Rampage>(Player);
                    growth.UpgradeInternal(); growth.FinalizeUpgradeInternal();
                    growth._extraDamageFromPlays = -1.2500m;
                    growth.DynamicVars.Damage.BaseValue = 8.7500m;
                    growth.DynamicVars["Increase"].BaseValue = 2.500m;
                    if (scenario == "rampage_repeat") growth.BaseReplayCount = 1;
                    pcs.Hand.AddInternal(growth, silent: true);
                    play = growth;
                    break;
                case "strike":
                    foreach (Creature enemy in combat.Enemies) enemy.CurrentHp = 1;
                    play = pcs.Hand.Cards.Where(card => card is StrikeIronclad).Skip(1).First();
                    break;
                case "enemy":
                    Player.Creature.CurrentHp = 1;
                    Enemy.Monster!.SetMoveImmediate((MoveState)Enemy.Monster.MoveStateMachine!.States["DARK_STRIKE_MOVE"], true);
                    break;
                case "phase_one":
                    Player.Creature.CurrentHp = 1;
                    pcs.Hand.AddInternal(combat.CreateCard<Burn>(Player));
                    break;
                case "poison":
                    Enemy.CurrentHp = 1;
                    foreach (Creature enemy in combat.Enemies)
                    {
                        enemy.CurrentHp = 1;
                        Await(PowerCmd.Apply<PoisonPower>(new ThrowingPlayerChoiceContext(), enemy, 2, Player.Creature, null));
                    }
                    break;
                case "enemy_end":
                    foreach (Creature enemy in combat.Enemies)
                    {
                        enemy.CurrentHp = 1;
                        Await(PowerCmd.Apply<DemisePower>(new ThrowingPlayerChoiceContext(), enemy, 2, Player.Creature, null));
                    }
                    break;
                case "player_poison":
                case "extra_turn_poison":
                    Player.Creature.CurrentHp = 3;
                    Await(PowerCmd.Apply<PoisonPower>(new ThrowingPlayerChoiceContext(), Player.Creature, 2, Enemy, null));
                    if (scenario == "extra_turn_poison")
                        Await(PowerCmd.Apply<AmbergrisPower>(new ThrowingPlayerChoiceContext(), Player.Creature, 1, Player.Creature, null));
                    break;
                case "enemy_end_poison":
                    Player.Creature.CurrentHp = 1;
                    Await(PowerCmd.Apply<PoisonPower>(new ThrowingPlayerChoiceContext(), Player.Creature, 2, Enemy, null));
                    break;
                case "forced_end":
                    // Add after the first start so its first damage occurs at T+1.
                    Player.AddRelicInternal(ModelDb.Relic<MercuryHourglass>().ToMutable(), silent: true);
                    foreach (Creature enemy in combat.Enemies) enemy.CurrentHp = 1;
                    play = combat.CreateCard<VoidForm>(Player);
                    pcs.Hand.AddInternal(play);
                    break;
                case "fire_potion":
                    foreach (Creature enemy in combat.Enemies) enemy.CurrentHp = 1;
                    Player.AddPotionInternal(ModelDb.Potion<FirePotion>().ToMutable(), 0, silent: true);
                    break;
                case "power_retire":
                case "power_retire_reacquire":
                    Await(PowerCmd.Apply<StrengthPower>(new ThrowingPlayerChoiceContext(), Player.Creature, -5,
                        Player.Creature, null));
                    Await(PowerCmd.Apply<DexterityPower>(new ThrowingPlayerChoiceContext(), Player.Creature, 1,
                        Player.Creature, null));
                    Player.AddPotionInternal(ModelDb.Potion<FlexPotion>().ToMutable(), 0, silent: true);
                    Player.AddPotionInternal(ModelDb.Potion<FlexPotion>().ToMutable(), 1, silent: true);
                    break;
                default: throw new InvalidOperationException("unknown_terminal_fixture:" + scenario);
            }
            RootCreatures = combat.Creatures.ToArray();
            EnvLiveCards.Warmup(combat); EnvLiveCreatures.Warmup(combat); EnvLiveInventory.Warmup(Player);
            using NativeEnv.Session schema = NativeEnv.OpenCombat("DUAL001", "CULTISTS_NORMAL");
            EnvLiveRootComponents components = new(
                EnvLiveCards.Capture(combat, Player, RootCreatures, new Dictionary<CardModel, int>(ReferenceEqualityComparer.Instance)),
                EnvLiveMonsters.Capture(RootCreatures), EnvLiveRunContext.Capture(combat, Player),
                EnvLiveCreatures.Capture(combat, Player, RootCreatures), EnvLiveInventory.Capture(Player),
                EnvLiveCombatContext.Capture(combat, Player, RootCreatures), EnvLiveRoomContext.Capture(run, Player, combat));
            RootJson = EnvLiveRootAssembler.Compose(components, schema, NativeEnv.ExportBlankTemplateJson(schema));
            CombatRootSnapshot root = CombatRootSnapshot.Capture(combat);
            _verification = root.EnvVerification;
            SearchPolicySnapshot policy = SolverController.CaptureSearchPolicy(SolverSettings.Capture(), combat, includeTurnSetup: false, theftPolicy: null) with
            {
                FixedBudget = true, VerifyIncrementalSearch = false, DetailedDiagnostics = false,
                MeasurePhasePerformance = false, BudgetOverrideMilliseconds = null,
            };
            // Replay never renders names. Empty presentation dictionaries avoid
            // localization lookups for upstream testing-only registered powers.
            _solver = new CombatBeamSolver(root, new SolverDisplayNames([], [], [], [], [], [], [], [], false, true), BattleDamageTracker.Observe(combat), policy);
            int turn = root.StartTurnNumber;
            List<EnvCombatAction> actions = [];
            List<PlanAction> plans = [];
            int removedIndex = -1;
            if (scenario is "strike" or "fire_potion" or "body_slam" or "rampage_then_turn")
            {
                CardModel setup = pcs.Hand.Cards.First(card => scenario == "body_slam" ? card is BodySlam : card is StrikeIronclad);
                removedIndex = pcs.Hand.Cards.ToList().IndexOf(setup);
                actions.Add(new(EnvCombatActionKind.PlayCard, removedIndex, (int)Enemy.CombatId!.Value));
                plans.Add(new(PlanActionKind.PlayCard, turn, setup.Id.Entry, TargetCombatId: Enemy.CombatId));
            }
            if (play != null)
            {
                int index = pcs.Hand.Cards.ToList().FindIndex(card => ReferenceEquals(card, play));
                bool self = play is VoidForm;
                uint target = self ? Player.Creature.CombatId!.Value : combat.Enemies[1].CombatId!.Value;
                int adjustedIndex = removedIndex >= 0 && index > removedIndex ? index - 1 : index;
                actions.Add(new(EnvCombatActionKind.PlayCard, adjustedIndex, self ? -1 : (int)target));
                plans.Add(new(PlanActionKind.PlayCard, turn, play.Id.Entry, TargetCombatId: target,
                    EndsPlayerTurn: self));
            }
            else if (scenario == "fire_potion")
            {
                actions.Add(new(EnvCombatActionKind.UsePotion, 0, (int)combat.Enemies[1].CombatId!.Value));
                plans.Add(new(PlanActionKind.UsePotion, turn, TargetCombatId: combat.Enemies[1].CombatId,
                    PotionSlot: 0, PotionId: "FIRE_POTION"));
            }
            else if (scenario is "power_retire" or "power_retire_reacquire")
            {
                int potionCount = scenario == "power_retire_reacquire" ? 2 : 1;
                uint playerCombatId = Player.Creature.CombatId!.Value;
                for (int slot = 0; slot < potionCount; slot++)
                {
                    actions.Add(new(EnvCombatActionKind.UsePotion, slot, (int)playerCombatId));
                    plans.Add(new(PlanActionKind.UsePotion, turn, TargetCombatId: playerCombatId,
                        PotionSlot: slot, PotionId: "FLEX_POTION"));
                }
            }
            else
            {
                actions.Add(new(EnvCombatActionKind.EndTurn));
                plans.Add(new(PlanActionKind.EndTurn, turn));
            }
            if (scenario == "rampage_then_turn")
            {
                actions.Add(new(EnvCombatActionKind.EndTurn));
                plans.Add(new(PlanActionKind.EndTurn, turn));
            }
            Actions = actions.ToArray(); _plans = plans.ToArray();
        }
        catch { Dispose(); throw; }
    }

    internal NativeBranch Replay()
    {
        string before = ContinuationStamp.CaptureLive(_liveCombat).StateText;
        SimulationSnapshot result = _solver.Replay(_plans);
        if (ContinuationStamp.CaptureLive(_liveCombat).StateText != before)
        {
            result.ReleaseSimulator();
            throw new InvalidOperationException("native_prediction_changed_live_source");
        }
        if (result.BoundaryReason != SearchBoundaryReason.None || result.Simulator.TerminalStamp.HasValue != ExpectTerminal)
        {
            string reason = result.BoundaryReason.ToString();
            result.ReleaseSimulator();
            throw new InvalidOperationException("native_fixture_boundary_or_terminal_mismatch:" + reason);
        }
        SimPlayerCombatState playerState = result.Simulator.State.GetPlayerCombatState(Player);
        var potionInventoryMethod = typeof(CombatBeamSolver).GetMethod("BuildPotionInventoryKey",
            System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance)
            ?? throw new MissingMethodException("Original Beam potion inventory key method missing");
        StateFingerprint nativePotionInventoryKey = (StateFingerprint)(potionInventoryMethod.Invoke(_solver,
            [(SimulatedCombatState)result.Simulator.State.CombatState])
            ?? throw new InvalidOperationException("Original Beam potion inventory key unavailable"));
        var cyclePileMethod = typeof(CombatBeamSolver).GetMethod("BuildCyclePileShapeKey",
            System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static)
            ?? throw new MissingMethodException("Original Beam cycle pile shape method missing");
        StateFingerprint nativeCyclePileShape = (StateFingerprint)(cyclePileMethod.Invoke(null, [playerState])
            ?? throw new InvalidOperationException("Original Beam cycle pile shape unavailable"));
        var appendPileMethod = typeof(CombatBeamSolver).GetMethod("AppendPile",
            System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance)
            ?? throw new MissingMethodException("Original Beam AppendPile method missing");
        StateFingerprintBuilder nativePileBuilder = new();
        (SimCardPile Pile, char Marker)[] nativePiles = [
            (playerState.Hand, 'H'), (playerState.DrawPile, 'D'),
            (playerState.DiscardPile, 'C'), (playerState.ExhaustPile, 'X')];
        foreach ((SimCardPile pile, char marker) in nativePiles)
        {
            object?[] args = [nativePileBuilder, pile, marker];
            appendPileMethod.Invoke(_solver, args);
            nativePileBuilder = (StateFingerprintBuilder)args[0]!;
        }
        return new(result, nativePotionInventoryKey, nativeCyclePileShape, nativePileBuilder.Finish(), _verification?.FailureReason);
    }

    internal JsonElement ObserveVanillaPlayerPoison()
    {
        // A separate observation fixture after shadow comparison: nonlethal HP
        // avoids the game's real OnEnded/run cleanup, which cannot be skipped
        // as presentation. This is timing evidence, never a shadow successor.
        Player.Creature.CurrentHp = 3;
        using VanillaPoisonTrace trace = new(Player, _liveCombat);
        trace.Record("live_frozen_root");
        Await(_combatManager.EndPlayerTurnPhaseOneInternal());
        Await(_combatManager.EndPlayerTurnPhaseTwoInternal());
        Await(_combatManager.SwitchFromPlayerToEnemySide(_combatManager._turnState!));
        Await(_combatManager.StartTurn(_combatManager._turnState!)); // Complete actual enemy turn and switch sides.
        trace.Record("after_real_enemy_turn_before_player_setup");
        if (Player.Creature.CurrentHp != 3) throw new InvalidOperationException("vanilla_player_poison_triggered_during_enemy_turn");
        Await(_combatManager.StartTurn(_combatManager._turnState!)); // Actual T+1 draw, then AfterSideTurnStart.
        trace.Record("after_real_player_start");
        if (Player.Creature.CurrentHp != 1) throw new InvalidOperationException("vanilla_player_poison_did_not_damage_owner");
        return trace.Complete();
    }

    private void Await(Task task)
    {
        Stopwatch timeout = Stopwatch.StartNew();
        while (!task.IsCompleted && timeout.Elapsed < TimeSpan.FromSeconds(20)) { _sync.Pump(); Thread.Yield(); }
        if (!task.IsCompleted) throw new TimeoutException("combat_prediction_fixture_timeout");
        task.GetAwaiter().GetResult();
    }

    public void Dispose()
    {
        _combatManager.Reset(false); _manager.CleanUp(); _restorePresentation();
        NonInteractiveMode.AutoSlayerCheck = () => false;
        SynchronizationContext.SetSynchronizationContext(null);
    }
}

internal sealed class NativeBranch(SimulationSnapshot snapshot, StateFingerprint nativePotionInventoryKey,
    StateFingerprint nativeCyclePileShapeKey,
    StateFingerprint nativeStateKeyPileSegment, string? envFailureReason) : IDisposable
{
    internal CombatPredictionSimulator Simulator => snapshot.Simulator;
    internal bool EnvBranchAttached => snapshot.EnvBranch is not null;
    internal bool EnvVerificationObserved => snapshot.EnvVerificationObserved;
    internal string? EnvFailureReason => envFailureReason;
    internal bool EnvProjectionHasOriginalKey => snapshot.EnvBranch?.CaptureProjection().OriginalStateKeyReady == true;
    internal int EnvProjectionHistoryCount => snapshot.EnvBranch?.CaptureProjection().HistoryCount ?? -1;
    internal int EnvPolicyCardCount => snapshot.EnvBranch?.CapturePolicyProjection().Cards.Count ?? -1;
    internal int EnvThreatCreatureCount => snapshot.EnvBranch?.CaptureThreatProjection().Creatures.Count ?? -1;
    internal int EnvOriginalCardFingerprintCount => snapshot.EnvBranch?.CaptureCardFingerprints().Cards.Count ?? -1;
    internal StateFingerprint NativeUnorderedPileKey => snapshot.UnorderedPileKey;
    internal StateFingerprint NativePotionInventoryKey => nativePotionInventoryKey;
    internal StateFingerprint NativeCyclePileShapeKey => nativeCyclePileShapeKey;
    internal StateFingerprint NativeCycleShapeKey => snapshot.CycleShapeKey;
    internal StateFingerprint NativeStateKeyPileSegment => nativeStateKeyPileSegment;
    internal StateFingerprint? EnvUnorderedPileKey => snapshot.EnvBranch?.CapturePileFingerprints().UnorderedPileKey;
    internal StateFingerprint? EnvPotionInventoryKey => snapshot.EnvBranch?.CapturePotionInventoryKey();
    internal StateFingerprint? EnvCyclePileShapeKey => snapshot.EnvBranch?.CapturePileFingerprints().CyclePileShapeKey;
    internal StateFingerprint? EnvCycleShapeKey => snapshot.EnvBranch?.CaptureCycleShapeKey(SearchBoundaryReason.None);
    internal StateFingerprint? EnvStateKeyPileSegment
    {
        get
        {
            if (snapshot.EnvBranch is not { } branch) return null;
            StateFingerprintBuilder key = new();
            branch.CapturePileFingerprints().AppendStateKeyPiles(ref key, 0);
            return key.Finish();
        }
    }
    public void Dispose() => snapshot.ReleaseSimulator();
}
