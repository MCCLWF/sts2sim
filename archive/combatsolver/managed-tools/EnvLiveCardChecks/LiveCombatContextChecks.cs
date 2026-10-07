using System.Text.Json;
using System.Text.Json.Nodes;
using CombatSolver;
using CombatSolverEnv.Api;
using Sts2EnvCompare;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Combat.History.Entries;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.GameActions.Multiplayer;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Afflictions;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Encounters;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Unlocks;
using MegaCrit.Sts2.Core.ValueProps;

internal static class LiveCombatContextChecks
{
    internal static void Run()
    {
        Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all, 1);
        RunState run = RunState.CreateForNewRun([player], ModelDb.ActsByIndex.Select(acts => acts[0].ToMutable()).ToArray(),
            [], GameMode.Standard, 0, "DUAL001");
        EncounterModel encounter = ModelDb.Encounter<CultistsNormal>().ToMutable();
        encounter.GenerateMonstersWithSlots(run);
        CombatState combat = new(encounter, run);
        combat.AddPlayer(player);
        player.PlayerCombatState = new(player) { Phase = PlayerTurnPhase.Play, Energy = 7 };
        player.PlayerCombatState.Stars = 4;
        foreach (var (monster, slot) in encounter.MonstersWithSlots)
            combat.AddCreature(combat.CreateCreature(monster, CombatSide.Enemy, slot));
        CombatRoom room = new(combat) { GoldProportion = 0.375f };
        run.PushRoom(room);
        CombatManager manager = new();
        manager._turnState = new(combat) { IsInProgress = true, IsStarting = false };
        var actions = new ActionQueueSet([player]);
        CardModel attack = combat.CreateCard<StrikeIronclad>(player), skill = combat.CreateCard<DefendIronclad>(player);
        CardPlay Play(CardModel card) => new()
        {
            Card = card, Player = player, Target = null, ResultPile = PileType.Discard,
            Resources = new() { EnergySpent = 1, EnergyValue = 1, StarsSpent = 0, StarValue = 0 },
            IsAutoPlay = false, PlayIndex = 0, PlayCount = 1,
        };
        void Started(CardModel card) => manager.History._entries.Add(new CardPlayStartedEntry(Play(card), combat.RoundNumber, combat.CurrentSide, manager.History, [player]));
        void Finished(CardModel card) => manager.History._entries.Add(new CardPlayFinishedEntry(Play(card), combat.RoundNumber, combat.CurrentSide, manager.History, [player]));
        void Exhausted(CardModel card) => manager.History._entries.Add(new CardExhaustedEntry(card, combat.RoundNumber, combat.CurrentSide, manager.History, [player]));
        void Damage(int amount) => manager.History._entries.Add(new DamageReceivedEntry(
            new(player.Creature, ValueProp.Unpowered) { UnblockedDamage = amount }, player.Creature, null, null,
            combat.RoundNumber, combat.CurrentSide, manager.History, [player]));
        Started(skill); Started(attack); Finished(attack); Exhausted(skill); Damage(2);
        // An actual player's extra turn changes TurnNumber without changing
        // RoundNumber. Old entries must fail the game's HappenedThisTurn query.
        player.PlayerCombatState.IncrementTurnNumber();
        manager._turnState.PlayersTakingExtraTurn.Add(player);
        Started(attack); Started(attack); Finished(attack); Finished(skill); Exhausted(attack); Damage(0); Damage(3);
        manager.History._entries.Add(new CardAfflictedEntry(skill, ModelDb.Affliction<Bound>().ToMutable(),
            combat.RoundNumber, combat.CurrentSide, manager.History, [player]));
        encounter._rng!.NextInt(100);
        Creature[] creatures = combat.Creatures.ToArray();
        EnvLiveCombatContext captured = EnvLiveCombatContext.Capture(combat, player, creatures, manager, actions);
        JsonElement fields = captured.Component.GetProperty("combat_fields");
        Require(fields.GetProperty("cards_played_this_combat").GetInt32() == manager.History.CardPlaysFinished.Count(), "finished history lost");
        Require(fields.GetProperty("player_attack_cards_this_turn").GetInt32() == manager.History.CardPlaysFinished.Count(e => e.HappenedThisTurn(combat) && e.CardPlay.Player == player && e.CardPlay.Card.Type == CardType.Attack), "current-turn attacks differ from native query");
        Require(fields.GetProperty("player_attack_cards_started_this_turn").GetInt32() == manager.History.CardPlaysStarted.Count(e => e.HappenedThisTurn(combat) && e.CardPlay.Player == player && e.CardPlay.Card.Type == CardType.Attack), "current-turn started attacks differ from native query");
        Require(fields.GetProperty("player_attack_cards_started_this_turn").GetInt32() != fields.GetProperty("player_attack_cards_this_turn").GetInt32(), "started and finished attacks were conflated");
        Require(fields.GetProperty("player_attack_skill_cards_started_this_turn").GetInt32() == manager.History.CardPlaysStarted.Count(e => e.HappenedThisTurn(combat) && e.CardPlay.Player == player && e.CardPlay.Card.Type is CardType.Attack or CardType.Skill), "extra-turn started count differs from native query");
        Require(!fields.GetProperty("player_played_skill_this_turn").GetBoolean(), "previous extra-turn skill leaked into current turn");
        Require(fields.GetProperty("player_damage_received_entries_this_combat").GetInt32() == manager.History.Entries.OfType<DamageReceivedEntry>().Count(e => e.Receiver == player.Creature && e.Result.UnblockedDamage > 0), "damage-entry count differs from native query");
        Require(fields.GetProperty("player_lost_hp_this_turn").GetBoolean(), "current turn HP loss lost");
        Require(fields.GetProperty("cards_exhausted_this_combat").GetInt32() == manager.History.Entries.OfType<CardExhaustedEntry>().Count(), "total exhausted count differs from native query");
        Require(fields.GetProperty("player_cards_exhausted_this_turn").GetInt32() == manager.History.Entries.OfType<CardExhaustedEntry>().Count(e => e.HappenedThisTurn(combat) && e.Actor == player.Creature), "current turn exhausted count differs from native query");
        Require(fields.GetProperty("player_bound_cards_afflicted_this_turn").GetInt32() == manager.History.Entries.OfType<CardAfflictedEntry>().Count(e => e.HappenedThisTurn(combat) && e.Actor == player.Creature && e.Affliction is Bound), "Bound history differs from native query");
        Require(fields.GetProperty("player_taking_extra_turn").GetBoolean(), "actual extra-turn membership lost");
        // Only this combat-context component is live. Card/power/monster
        // instances remain an explicit seeded fixture, not a full live root.
        using NativeEnv.Session seeded = NativeEnv.OpenCombat("DUAL001", "CULTISTS_NORMAL");
        JsonObject document = JsonNode.Parse(seeded.ExportSnapshotJson())!.AsObject();
        JsonObject state = document["state"]!.AsObject();
        foreach (var field in fields.EnumerateObject()) state["combat"]![field.Name] = JsonNode.Parse(field.Value.GetRawText());
        foreach (var field in captured.Component.GetProperty("player_combat_fields").EnumerateObject())
            state["player_combat"]![field.Name] = JsonNode.Parse(field.Value.GetRawText());
        document["state_hash"] = null;
        using NativeEnv.Session restored = NativeEnv.ImportCombatSnapshot(document.ToJsonString());
        using JsonDocument exported = JsonDocument.Parse(restored.ExportSnapshotJson());
        foreach (var field in fields.EnumerateObject()) Require(JsonElement.DeepEquals(field.Value, exported.RootElement.GetProperty("state").GetProperty("combat").GetProperty(field.Name)), "combat context import differs:" + field.Name);
        foreach (var field in captured.Component.GetProperty("player_combat_fields").EnumerateObject()) Require(JsonElement.DeepEquals(field.Value, exported.RootElement.GetProperty("state").GetProperty("player_combat").GetProperty(field.Name)), "player combat import differs:" + field.Name);
        string frozen = captured.Component.GetRawText();
        ulong hash = restored.StateHash;
        using NativeEnv.Session predicted = restored.Predict(new EnvAction(3));
        Require(restored.StateHash == hash && predicted.StateHash != hash, "combat context prediction mutated source");
        manager.History.Clear();
        encounter._rng.NextInt(100);
        player.PlayerCombatState.Energy = 1;
        Require(captured.Component.GetRawText() == frozen, "live history/RNG/player mutation changed owned context");
        manager._cardOrPotionEffectDepth[player] = 1;
        Reject("idle_player_play_boundary");
        manager._cardOrPotionEffectDepth.Clear();
        actions._actionQueues[0].isPaused = true;
        Reject("action_or_choice_pending");
        actions._actionQueues[0].isPaused = false;
        manager._turnState.PlayersReadyToEndTurn.Add(player);
        Reject("end_turn_already_queued");
        manager._turnState.PlayersReadyToEndTurn.Clear();
        manager._turnState.Cancel();
        Console.WriteLine("live combat context: native histories including extra-turn boundaries, actual encounter RNG/rosters/flags/resources, owned C++ restore/predict and async-boundary rejection passed; full_root=false");

        void Reject(string expected)
        {
            try { EnvLiveCombatContext.Capture(combat, player, creatures, manager, actions); throw new Exception("unsupported boundary accepted"); }
            catch (InvalidOperationException error) when (error.Message.Contains(expected)) { }
        }
    }

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }
}
