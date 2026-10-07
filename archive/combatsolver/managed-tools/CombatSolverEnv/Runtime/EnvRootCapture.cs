using Godot;
using System.Text.Json;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Nodes;
using MegaCrit.Sts2.Core.Runs;
using Sts2EnvCompare;

namespace CombatSolver;

internal sealed partial class CombatRootSnapshot
{
    internal EnvBranchVerificationContext? EnvVerification { get; set; }
}

// Called only inside the upstream main-thread capture boundary. Background
// observers receive immutable values; they never obtain live CombatState.
internal static class EnvRootCapture
{
    private static bool Enabled => EnvBackendSettings.Mode != EnvBackendMode.Native
        && (EnvBackendSettings.Mode == EnvBackendMode.Env || EnvBackendSettings.Compare)
        && System.Environment.GetEnvironmentVariable("STS2ENV_VERIFY_BRANCHES") != "0";

    internal static void Warmup(CombatState state)
    {
        if (!NGame.IsMainThread()) throw new InvalidOperationException("Env root warmup requires main thread");
        if (Enabled)
        {
            EnvLiveCards.Warmup(state);
            EnvLiveCreatures.Warmup(state);
            foreach (Player player in state.Players) EnvLiveInventory.Warmup(player);
        }
    }

    internal static EnvBranchVerificationContext? Freeze(CombatState state, Player player)
    {
        if (!NGame.IsMainThread()) throw new InvalidOperationException("Env root capture requires main thread");
        if (!Enabled) return null;
        if (player.Character.Id.Entry != "IRONCLAD" || state.Encounter is null)
            return null;
        EnvBranchVerificationContext context = new(
            state.RunState.Rng.StringSeed, state.Encounter.Id.Entry,
            (int)state.RunState.AscensionLevel, state.RunState.CurrentActIndex,
            message => GD.Print($"[CombatSolverEnv] {message}"));
        try
        {
            Creature[] creatures = state.Creatures.Concat(state.EscapedCreatures)
                .Distinct<Creature>(ReferenceEqualityComparer.Instance).ToArray();
            Dictionary<MegaCrit.Sts2.Core.Models.CardModel, int> played = new(ReferenceEqualityComparer.Instance);
            foreach (var entry in CombatManager.Instance.History.CardPlaysFinished)
                if (entry._playerTurnNumbers.TryGetValue(player.NetId, out int turn)) played[entry.CardPlay.Card] = turn;
            context.LiveCards = EnvLiveCards.Capture(state, player, creatures, played);
            context.LiveMonsters = EnvLiveMonsters.Capture(creatures);
            context.LiveRun = EnvLiveRunContext.Capture(state, player);
            context.LiveCreatures = EnvLiveCreatures.Capture(state, player, creatures);
            context.LiveInventory = EnvLiveInventory.Capture(player);
            context.LiveCombat = EnvLiveCombatContext.Capture(state, player, creatures);
            GD.Print($"[CombatSolverEnv] LIVE_CARD_COMPONENT cards={context.LiveCards.Component.GetProperty("cards").GetProperty("count").GetInt32()} " +
                "source=live immutable=true full_root=false");
            GD.Print($"[CombatSolverEnv] LIVE_COMBAT_COMPONENT history_operands={context.LiveCombat.Component.GetProperty("history_source").GetArrayLength()} " +
                "source=live scope=idle_boundary_and_history immutable=true full_root=false");
            GD.Print($"[CombatSolverEnv] LIVE_MONSTER_COMPONENT monsters={context.LiveMonsters.Component.GetProperty("monsters").GetArrayLength()} " +
                "source=live scope=monster_rng_and_fsm immutable=true full_root=false");
            var run = context.LiveRun.Component.GetProperty("run");
            var coordinates = run.GetProperty("visited_map_coords");
            string coord = coordinates.GetArrayLength() == 0 ? "null" : coordinates[coordinates.GetArrayLength() - 1].GetRawText();
            GD.Print($"[CombatSolverEnv] LIVE_RUN_COMPONENT run_streams=12 player_streams=3 map_points={run.GetProperty("map").GetProperty("points").GetArrayLength()} " +
                $"current_coord={coord} source=live immutable=true full_root=false");
            var profiles = context.LiveRun.Component.GetProperty("profile_source");
            GD.Print($"[CombatSolverEnv] LIVE_CREATURE_COMPONENT creatures={context.LiveCreatures.Component.GetProperty("creatures").GetProperty("count").GetInt32()} " +
                $"powers={context.LiveCreatures.Component.GetProperty("powers").GetProperty("count").GetInt32()} source=live immutable=true full_root=false");
            GD.Print($"[CombatSolverEnv] LIVE_INVENTORY_COMPONENT relics={context.LiveInventory.Component.GetProperty("relics").GetProperty("count").GetInt32()} " +
                $"potions={context.LiveInventory.Component.GetProperty("potions").GetProperty("count").GetInt32()} orbs={context.LiveInventory.Component.GetProperty("orbs").GetProperty("count").GetInt32()} " +
                "source=live immutable=true full_root=false");
            foreach (string name in new[] { "run", "player" })
            {
                var profile = profiles.GetProperty(name);
                GD.Print($"[CombatSolverEnv] LIVE_PROFILE_COMPONENT source={name} runs={profile.GetProperty("number_of_runs").GetInt32()} " +
                    $"epochs={profile.GetProperty("unlocked_epochs").GetArrayLength()} encounters_seen={profile.GetProperty("encounters_seen").GetArrayLength()} " +
                    $"missing_epochs={profile.GetProperty("missing_epochs").GetArrayLength()} missing_seen_encounters={profile.GetProperty("missing_seen_encounters").GetArrayLength()} " +
                    $"full_veteran_compatible={profile.GetProperty("full_veteran_compatible").GetBoolean()} immutable=true full_root=false");
            }
            context.LiveRoom = EnvLiveRoomContext.Capture(state.RunState as RunState
                ?? throw new InvalidOperationException("live_root_run_state_missing"), player, state);
            JsonElement roomComponent = context.LiveRoom.Component;
            GD.Print($"[CombatSolverEnv] LIVE_ROOM_COMPONENT room={roomComponent.GetProperty("room").GetProperty("room_type").GetInt32()} " +
                $"rewards={roomComponent.GetProperty("rewards").GetProperty("reward_refs").GetArrayLength()} " +
                $"subscriptions={roomComponent.GetProperty("event_subscriptions").GetProperty("subscriber_count").GetInt32()} " +
                "source=live ordinary_combat_idle_boundary full_root=false");
        }
        catch (InvalidOperationException error)
        {
            context.Reject($"live_root_components:{error.Message}", player.PlayerCombatState!.TurnNumber, 0);
        }
        return context;
    }

    /// <summary>
    /// Capture and assemble the actual idle combat root for the game-facing
    /// Env comparison entry. The returned JSON owns every value needed by the
    /// native import; callers may move it to a worker thread. A failure is
    /// returned as a visible reason so the caller can keep Native active.
    /// </summary>
    internal static Sts2EnvCompare.EnvLiveSnapshotResult CaptureLiveSnapshot(
        CombatState state, Player player)
    {
        try
        {
            Warmup(state);
            EnvBranchVerificationContext? context = Freeze(state, player);
            if (context is null)
                return Sts2EnvCompare.EnvLiveSnapshotResult.Failed("live_root_capture_not_available");
            if (context.FailureReason is { } failure)
                return Sts2EnvCompare.EnvLiveSnapshotResult.Failed(failure);
            if (context.LiveCards is null || context.LiveMonsters is null
                || context.LiveRun is null || context.LiveCreatures is null
                || context.LiveInventory is null || context.LiveCombat is null
                || context.LiveRoom is null)
                return Sts2EnvCompare.EnvLiveSnapshotResult.Failed("live_root_component_missing");

            using NativeEnv.Session schema = NativeEnv.OpenCombat(
                context.Seed, context.Encounter, context.Ascension, context.ActIndex);
            string blank = NativeEnv.ExportBlankTemplateJson(schema);
            string snapshot = EnvLiveRootAssembler.Compose(
                new EnvLiveRootComponents(context.LiveCards, context.LiveMonsters, context.LiveRun,
                    context.LiveCreatures, context.LiveInventory, context.LiveCombat, context.LiveRoom),
                schema, blank);
            return Sts2EnvCompare.EnvLiveSnapshotResult.Ready(snapshot);
        }
        catch (InvalidOperationException error)
        {
            return Sts2EnvCompare.EnvLiveSnapshotResult.Failed(
                $"live_root_assembly:{error.Message}");
        }
    }

    internal static CombatRootSnapshot Attach(CombatRootSnapshot root, EnvBranchVerificationContext? context)
    {
        root.EnvVerification = context;
        // This provider is invoked only by the main-thread UI. Search writes a
        // diagnostic value; it never reads locale, scene nodes or live state.
        Sts2EnvCompare.Entry.NativeBranchWarning = () => context?.FailureReason is { } reason
            ? SolverText.Format($"Env 分支核对已停止；当前由 Native 执行动作。\n{reason}")
            : null;
        return root;
    }
}
