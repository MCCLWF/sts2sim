using System.Collections;
using System.Reflection;
using System.Text.Json;
using CombatSolver;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Events;
using MegaCrit.Sts2.Core.Models.Relics;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Rewards;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Unlocks;
using MegaCrit.Sts2.Core.TestSupport;

internal static class RoomContextChecks
{
    internal static void Run()
    {
        SingleThreadContext syncContext = SingleThreadContext.Install();
        NonInteractiveMode.AutoSlayerCheck = () => true;
        Action restorePresentation = Headless.InstallLiveCombatPresentation();
        GameBootstrap.UseInMemorySaves();
        GameBootstrap.LoadLocalization();
        Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all, 1);
        RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(acts => acts[0].ToMutable()).ToList(),
            [], GameMode.Standard, 0, "ROOM001");
        RunManager.Instance.SetUpNewSingleplayer(run, shouldSave: false);
        RunManager.Instance.FinalizeStartingRelics().GetAwaiter().GetResult();
        EncounterModel encounter = ModelDb.GetById<EncounterModel>(new("ENCOUNTER", "CULTISTS_NORMAL")).ToMutable();
        CombatState combat = new(encounter, run);
        combat.AddPlayer(player);
        player.PlayerCombatState = new(player);
        CombatRoom room = new(combat);
        run.PushRoom(room);
        try
        {
            EnvLiveRoomContext captured = EnvLiveRoomContext.Capture(run, player, combat);
            JsonElement component = captured.Component;
            Require(component.GetProperty("run_fields").GetProperty("phase").GetInt32() == 2,
                "ordinary combat phase was not frozen");
            Require(component.GetProperty("rewards").GetProperty("reward_refs").GetArrayLength() == 0
                && component.GetProperty("reward_pool").GetArrayLength() == 0,
                "ordinary combat accepted non-empty reward state");
            JsonElement synchronizer = component.GetProperty("synchronizer");
            Require(synchronizer.GetProperty("rewards").GetProperty("stack_count").GetInt32() == 0
                && synchronizer.GetProperty("rewards").GetProperty("buffered_count").GetInt32() == 0
                && synchronizer.GetProperty("reward_messages").GetProperty("buffered_count").GetInt32() == 0
                && synchronizer.GetProperty("events").GetProperty("event_count").GetInt32() == 0
                && synchronizer.GetProperty("rest_site").GetProperty("site_count").GetInt32() == 0
                && synchronizer.GetProperty("treasure").GetProperty("vote_count").GetInt32() == 0,
                "ordinary combat synchronizer bookkeeping was not empty");
            Require(!component.GetProperty("event_subscriptions").GetProperty("player_relic_obtained").GetBoolean(),
                "presentation-only RelicObtained subscriptions were marked gameplay-pending");

            // A normal run keeps the completed Neow instance in EventSynchronizer
            // until the next BeginEvent. Capture that real post-exit state instead
            // of treating one retained event as pending gameplay.
            RunManager.Instance.EventSynchronizer.BeginEvent(ModelDb.Event<Neow>(), isPrefinished: false);
            syncContext.Pump();
            EventModel neowInstance = RunManager.Instance.EventSynchronizer.GetLocalEvent();
            int safeNeowOption = neowInstance.CurrentOptions
                .Select((option, index) => (option, index))
                .First(pair => pair.option.Relic is CursedPearl or DowsingRod or BoomingConch
                    or FishingRod or GoldenPearl or NeowsSacrifice or NeowsTorment
                    or SilverCrucible or WingedBoots).index;
            RunManager.Instance.EventSynchronizer.ChooseLocalOption(safeNeowOption);
            Task pendingEvent = RunManager.Instance.EventSynchronizer.AwaitPendingOptionTasks();
            while (!pendingEvent.IsCompleted)
            {
                syncContext.Pump();
                Thread.Yield();
            }
            pendingEvent.GetAwaiter().GetResult();
            RunManager.Instance.EventSynchronizer.BeforeExitingRoom();
            foreach (EventModel eventModel in RunManager.Instance.EventSynchronizer.Events) eventModel.EnsureCleanup();
            EnvLiveRoomContext withFinishedEvent = EnvLiveRoomContext.Capture(run, player, combat);
            JsonElement finishedEvents = withFinishedEvent.Component.GetProperty("synchronizer").GetProperty("events");
            Require(finishedEvents.GetProperty("event_count").GetInt32() == 1
                && finishedEvents.GetProperty("event").ValueKind == JsonValueKind.Object
                && finishedEvents.GetProperty("event").GetProperty("id").GetInt32() != 0
                && finishedEvents.GetProperty("event").GetProperty("is_finished").GetBoolean()
                && finishedEvents.GetProperty("event").GetProperty("cleanup_called").GetBoolean()
                && finishedEvents.GetProperty("state").GetProperty("canonical_event").GetInt32() != 0
                && finishedEvents.GetProperty("state").GetProperty("has_player_vote_slot").GetBoolean(),
                "completed Neow event state was not captured");

            object eventSync = RunManager.Instance.EventSynchronizer;
            IList eventStates = (IList)PrivateField(eventSync, "_events");
            eventStates.Add(null);
            ExpectRefusal(() => EnvLiveRoomContext.Capture(run, player, combat), "live_event_bookkeeping_pending");
            eventStates.RemoveAt(eventStates.Count - 1);
            FieldInfo pageIndex = eventSync.GetType().GetField("_pageIndex", BindingFlags.Instance | BindingFlags.NonPublic)!;
            pageIndex.SetValue(eventSync, 1u);
            ExpectRefusal(() => EnvLiveRoomContext.Capture(run, player, combat), "live_event_bookkeeping_pending");
            pageIndex.SetValue(eventSync, 0u);

            object restSync = RunManager.Instance.RestSiteSynchronizer;
            IList restStates = (IList)PrivateField(restSync, "_restSites");
            restStates.Add(null);
            ExpectRefusal(() => EnvLiveRoomContext.Capture(run, player, combat), "live_rest_site_bookkeeping_pending");
            restStates.RemoveAt(restStates.Count - 1);

            object treasureSync = RunManager.Instance.TreasureRoomRelicSynchronizer;
            IList votes = (IList)PrivateField(treasureSync, "_votes");
            votes.Add(null);
            ExpectRefusal(() => EnvLiveRoomContext.Capture(run, player, combat), "live_treasure_bookkeeping_pending");
            votes.RemoveAt(votes.Count - 1);

            room.AddExtraReward(player, new GoldReward(1, 1, player));
            try
            {
                EnvLiveRoomContext.Capture(run, player, combat);
                throw new Exception("extra reward was silently dropped");
            }
            catch (InvalidOperationException error) when (error.Message.Contains("live_combat_extra_rewards_pending")) { }

            // CardReward is deliberately left subscribed in this isolated
            // process. Capture must identify it by target type and reject it,
            // proving that the event was not treated as an empty UI callback.
            room.ExtraRewards[player].Clear();
            _ = new CardReward(CardCreationOptions.ForRoom(player, RoomType.Monster), 3, player);
            try
            {
                EnvLiveRoomContext.Capture(run, player, combat);
                throw new Exception("CardReward subscription was silently dropped");
            }
            catch (InvalidOperationException error) when (error.Message.Contains("live_reward_card_subscription_pending")) { }

            Console.WriteLine("live room component: ordinary single combat reward/event/rest/treasure synchronizers frozen; extra reward and CardReward subscription explicitly rejected; full_root=false");
        }
        finally
        {
            RunManager.Instance.CleanUp();
            NonInteractiveMode.AutoSlayerCheck = () => false;
            restorePresentation();
            SynchronizationContext.SetSynchronizationContext(null);
        }
    }

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }

    private static object PrivateField(object owner, string name)
        => owner.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic)?.GetValue(owner)
            ?? throw new InvalidOperationException("missing private field:" + name);

    private static void ExpectRefusal(Func<object> capture, string reason)
    {
        try
        {
            capture();
            throw new Exception("expected refusal was not raised:" + reason);
        }
        catch (InvalidOperationException error) when (error.Message.Contains(reason, StringComparison.Ordinal)) { }
    }
}
