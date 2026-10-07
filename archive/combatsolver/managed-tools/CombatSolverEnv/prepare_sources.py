"""Add checked integration seams without editing the pinned submodule.

The generated sources retain the upstream search policy. The player Poison
lifecycle correction follows the game's turn-start boundary; all other seams
are verification hooks, not an Env execution backend.
"""
from pathlib import Path
import json
import sys


def replace_once(source, old, new, label):
    if source.count(old) != 1:
        raise SystemExit(f"upstream {label} changed; review Env integration")
    return source.replace(old, new)


def correct_player_poison_lifecycle(upstream, output, replay):
    replay = replace_once(replay, """                int playerPoisonHistoryStart = simulator.History.Entries.Count;
                if (!CorePowerSupport.TriggerPoison(
                        simulator,
                        simulatedCombat,
                        [_player.Creature]))
                {
                    return SearchBoundaryReason.PendingChoice;
                }
                TriggeredPowerSupport.CompensateHistorySince(
                    simulator,
                    simulatedCombat,
                    playerPoisonHistoryStart);
                if (simulatedCombat.HasPendingChoice)
                    return SearchBoundaryReason.PendingChoice;
""", """                // Player Poison belongs to the game's player-side start listener.
                // ContinuePlayerStart owns it after setup, not this enemy-end stage.
""", "player Poison removed from enemy end")

    source = (upstream / "src/Search/CombatBeamSolver.RoundTransition.cs").read_text()
    source = replace_once(source, "        simulatedCombat.AdvancePlayerTurn(_player);\n",
                          "        simulatedCombat.AdvancePlayerTurn(_player);\n"
                          "        playerState.Phase = MegaCrit.Sts2.Core.Combat.PlayerTurnPhase.None;\n",
                          "player start None phase")
    source = replace_once(source, "        if (simulatedPlayer.Block > 0)\n",
                          "        playerState.Phase = MegaCrit.Sts2.Core.Combat.PlayerTurnPhase.Start;\n"
                          "        if (simulatedPlayer.Block > 0)\n", "player setup Start phase")
    (output / "CombatBeamSolver.RoundTransition.g.cs").write_text("#nullable enable\n" + source)

    source = (upstream / "src/Search/CombatBeamSolver.TurnExecutionContinuation.cs").read_text()
    source = replace_once(source,
                          "private enum PlayerStartStage { BeforeHand, PrepareDraw, Draw, CompensateDraw, AfterPlayer, Side, Deaths, Orbs, AutoPlay, Finish }",
                          "private enum PlayerStartStage { BeforeHand, PrepareDraw, Draw, CompensateDraw, AfterPlayer, Side, Poison, Deaths, Orbs, AutoPlay, Finish }",
                          "player Poison continuation stage")
    source = replace_once(source, """            if (Suspended(PlayerStartStage.Deaths)) return SearchBoundaryReason.PendingChoice;
        }
        beforeChoice?.Dispose();
""", """            if (Suspended(PlayerStartStage.Poison)) return SearchBoundaryReason.PendingChoice;
        }
        beforeChoice?.Dispose();
        if (stage <= PlayerStartStage.Poison)
        {
            // CombatManager setup (energy, draw, AfterPlayerTurnStart) precedes
            // the Poison AfterSideTurnStart listener and the orb/start safe point.
            int playerPoisonHistoryStart = simulator.History.Entries.Count;
            using (simulator.BeginExecutionDispatch())
                CorePowerSupport.TriggerPoison(simulator, combat, [player.Creature]);
            using (simulator.BeginExecutionDispatch())
                TriggeredPowerSupport.CompensateHistorySince(simulator, combat, playerPoisonHistoryStart);
            if (Suspended(PlayerStartStage.Deaths)) return SearchBoundaryReason.PendingChoice;
        }
""", "player Poison resumed after setup")
    source = replace_once(source, """            using (simulator.BeginExecutionDispatch())
                combat.TriggerAutoPrePlayEarly(simulator, player, progress.TurnNumber, choices, progress.Deaths);
""", """            using (simulator.BeginExecutionDispatch())
            {
                if (simulator.State.GetCreature(player.Creature).IsDead)
                {
                    // StartTurn makes a dead player ready, skips AutoPrePlay and
                    // the turn loop reaches the existing phase-one loss safe point.
                    combat.SetActionChoiceTiming(PlanChoiceTiming.PlayerTurnEnd);
                    PlayerTurnEndLifecycle.RunPhaseOne(simulator, combat, player, [player.Creature]);
                }
                else
                    combat.TriggerAutoPrePlayEarly(simulator, player, progress.TurnNumber, choices, progress.Deaths);
            }
""", "dead player start skips AutoPrePlay")
    (output / "CombatBeamSolver.TurnExecutionContinuation.g.cs").write_text("#nullable enable\n" + source)
    return replay


def main():
    upstream, output = map(Path, sys.argv[1:])
    output.mkdir(parents=True, exist_ok=True)
    source = (upstream / "src/Runtime/CombatRootSnapshot.cs").read_text()
    source = replace_once(source, "internal sealed class CombatRootSnapshot",
                          "internal sealed partial class CombatRootSnapshot", "root declaration")
    source = replace_once(source, "        CardDynamicVarWarmup.EnsureMaterialized(state);",
                          "        CardDynamicVarWarmup.EnsureMaterialized(state);\n"
                          "        EnvRootCapture.Warmup(state);", "Env card warmup")
    source = replace_once(source, "        ContinuationStamp continuationAfter = ContinuationStamp.CaptureLive(state);",
                          "        EnvBranchVerificationContext? envContext = EnvRootCapture.Freeze(state, player);\n"
                          "        ContinuationStamp continuationAfter = ContinuationStamp.CaptureLive(state);",
                          "Env capture stability boundary")
    source = replace_once(source, "return new CombatRootSnapshot(",
                          "CombatRootSnapshot capturedRoot = new(", "root construction")
    source = replace_once(source, "            potionRewardOutlook);\n    }",
                          "            potionRewardOutlook);\n"
                          "        return EnvRootCapture.Attach(capturedRoot, envContext);\n    }",
                          "root capture return")
    (output / "CombatRootSnapshot.g.cs").write_text("#nullable enable\n" + source)

    source = (upstream / "src/Search/CombatPlan.cs").read_text()
    source = replace_once(source, "internal sealed class SimulationSnapshot(",
                          "internal sealed partial class SimulationSnapshot(", "snapshot declaration")
    source = replace_once(source, "        _simulator = null;\n        _releasedBy = caller;",
                          "        EnvBranch?.Dispose();\n        EnvBranch = null;\n"
                          "        _simulator = null;\n        _releasedBy = caller;", "snapshot release")
    (output / "CombatPlan.g.cs").write_text("#nullable enable\n" + source)

    source = (upstream / "src/Search/CombatBeamSolver.Expansion.Replay.cs").read_text()
    source = replace_once(source,
                          "        return Snapshot(\n            simulator,\n            _startTurnNumber,\n"
                          "            actionCount: 0,\n            shufflesCrossed: simulator.ShuffleEventCount,\n"
                          "            boundary,\n            processedEnemyDeaths);",
                          "        SimulationSnapshot setupSnapshot = Snapshot(\n            simulator,\n"
                          "            _startTurnNumber,\n            actionCount: 0,\n"
                          "            shufflesCrossed: simulator.ShuffleEventCount,\n"
                          "            boundary,\n            processedEnemyDeaths);\n"
                          "        try { VerifyEnvReplay(setupSnapshot, null, [], startsAfterTurnSetup: true); }\n"
                          "        catch { setupSnapshot.ReleaseSimulator(); throw; }\n"
                          "        return setupSnapshot;", "turn setup observation")
    source = replace_once(source,
                          "        try { cardChoiceCapture?.Receive(this, simulator, processedEnemyDeaths); }",
                          "        try\n        {\n"
                          "            VerifyEnvReplay(snapshot, parentSnapshot, actions);\n"
                          "            cardChoiceCapture?.Receive(this, simulator, processedEnemyDeaths);\n        }",
                          "replay observation")
    # Both returns belong to ReplayAction, including incremental verification.
    source = replace_once(source, "                if (!policy.VerifyIncrementalSearch)\n                    return incremental;",
                            "                if (!policy.VerifyIncrementalSearch)\n                {\n"
                            "                    VerifyEnvReplay(incremental, parent.Snapshot, [action]);\n"
                            "                    return incremental;\n                }", "incremental early return")
    source = replace_once(source, "                return incremental;\n            });",
                          "                VerifyEnvReplay(incremental, parent.Snapshot, [action]);\n"
                          "                return incremental;\n            });", "replay action observation")
    source = correct_player_poison_lifecycle(upstream, output, source)
    (output / "CombatBeamSolver.Expansion.Replay.g.cs").write_text("#nullable enable\n" + source)

    source = (upstream / "src/Search/CombatBeamSolver.Phases.cs").read_text()
    source = replace_once(source,
                          "            // 最后才扫描标签，避免诊断在候选热路径上枚举整张表。",
                          "            root.EnvVerification?.Summarize(_run.Expanded, _run.TransitionCount);\n"
                          "            // 最后才扫描标签，避免诊断在候选热路径上枚举整张表。",
                          "solver completion observation")
    (output / "CombatBeamSolver.Phases.g.cs").write_text("#nullable enable\n" + source)

    source = (upstream / "src/UI/SolverSettingsPanel.General.cs").read_text()
    source = replace_once(source, "        return CreatePageScroll(content);\n    }",
                          "        EnvBackendSettingsUi.Append(content);\n"
                          "        return CreatePageScroll(content);\n    }", "Env backend settings page")
    (output / "SolverSettingsPanel.General.g.cs").write_text("#nullable enable\n" + source)

    english = json.loads((upstream / "src/UI/English.json").read_text())
    additions = json.loads((Path(__file__).parent / "UI/English.additions.json").read_text())
    if english.keys() & additions.keys():
        raise SystemExit("upstream UI catalog now includes an Env key; review translations")
    english.update(additions)
    (output / "English.g.json").write_text(json.dumps(english, ensure_ascii=False, indent=2) + "\n")


if __name__ == "__main__":
    main()
