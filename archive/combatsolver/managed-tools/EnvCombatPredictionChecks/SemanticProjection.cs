using System.Text.Json;
using CombatSolver;
using CombatSolver.Engine.Common;
using CombatSolver.Engine.InCombat.Simulation;
using CombatSolverEnv.Api;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Rngs;
using MegaCrit.Sts2.Core.Models;

// Deliberately semantic projection, not a complete native GameState serializer.
// Pool identities/capacity, replay metadata, full historical events, private card,
// power/relic/monster sidecars and noncombat random streams are not compared.
internal static class SemanticProjection
{
    private static JsonElement Catalog => EnvModelCatalog.Content;
    private static string ModelName(string kind, int id) => id == 0 ? "" : Catalog.GetProperty(kind).EnumerateObject()
        .Single(item => item.Value.GetProperty("id").GetInt32() == id).Name;

    internal static JsonElement Native(CombatPredictionSimulator simulator, ScenarioFixture fixture)
    {
        SimulatedCombatState combat = (SimulatedCombatState)simulator.State.CombatState;
        SimPlayerCombatState pcs = simulator.State.GetPlayerCombatState(fixture.Player);
        object Card(PredictedCard predicted)
        {
            CardModel card = predicted.Preview;
            return new { id = card.Id.Entry, upgrade = card.CurrentUpgradeLevel,
                enchantment = card.Enchantment?.Id.Entry ?? "", affliction = card.Affliction?.Id.Entry ?? "" };
        }
        object Creature(Creature creature)
        {
            SimCreatureState value = simulator.State.GetCreature(creature);
            return new { combat_id = (int)creature.CombatId!.Value,
                hp = value.CurrentHp, max_hp = value.MaxHp, block = value.Block, hp_display = (int)value.HpDisplay,
                attached = simulator.State.IsAttachedToCombat(creature),
                powers = combat.EffectivePowers().Where(power => ReferenceEquals(power.Owner, creature))
                    .Select(power => new { id = power.Id.Entry, amount = power.Amount }).ToArray() };
        }
        object Rng(PredictionRngState rng) => new { counter = rng.Counter, s0 = rng.State0, s1 = rng.State1, s2 = rng.State2, s3 = rng.State3 };
        object? terminal = simulator.TerminalStamp is { } stamp
            ? new { player_turn = stamp.PlayerTurn, outcome = stamp.Outcome.ToString().ToLowerInvariant() } : null;
        return JsonSerializer.SerializeToElement(new
        {
            terminal,
            in_progress = simulator.IsInProgress, pending_loss = simulator.IsAboutToLose,
            player_turn = combat.GetPlayerTurnNumber(fixture.Player),
            energy = pcs.Energy, stars = pcs.Stars, phase = (int)pcs.Phase,
            round = combat.RoundNumber, side = (int)combat.CurrentSide,
            creatures = fixture.RootCreatures.Select(Creature).ToArray(),
            enemies = simulator.State.Enemies.Select(creature => (int)creature.CombatId!.Value).ToArray(),
            piles = new[] { pcs.Hand, pcs.DrawPile, pcs.DiscardPile, pcs.ExhaustPile, pcs.PlayPile }
                .Select(pile => pile.Cards.Select(Card).ToArray()).ToArray(),
            potions = Enumerable.Range(0, fixture.Player.PotionSlots.Count)
                .Select(slot => combat.GetPotionAtSlot(fixture.Player, slot)?.Id.Entry ?? "").ToArray(),
            rng = new[] { Rng(simulator.Rng.ShuffleState), Rng(simulator.Rng.CombatCardGenerationState),
                Rng(simulator.Rng.CombatPotionGenerationState), Rng(simulator.Rng.CombatCardSelectionState),
                Rng(simulator.Rng.CombatEnergyCostsState), Rng(simulator.Rng.CombatTargetsState),
                Rng(simulator.Rng.CombatOrbGenerationState), Rng(simulator.Rng.MonsterAiState), Rng(simulator.Rng.NicheState) }
        });
    }

    internal static JsonElement Env(EnvCombatPredictionSnapshot snapshot, ScenarioFixture fixture)
    {
        JsonElement state = snapshot.Document.GetProperty("inner_snapshot").GetProperty("state");
        JsonElement combat = state.GetProperty("combat"), pcs = state.GetProperty("player_combat");
        Dictionary<int, JsonElement> creatures = Slots(state.GetProperty("creatures"));
        Dictionary<int, JsonElement> cards = Slots(state.GetProperty("cards"));
        Dictionary<int, JsonElement> powers = Slots(state.GetProperty("powers"));
        Dictionary<int, JsonElement> potions = Slots(state.GetProperty("potions"));
        object Card(int reference)
        {
            JsonElement card = cards[reference];
            return new { id = ModelName("cards", card.GetProperty("id").GetInt32()), upgrade = card.GetProperty("upgrade").GetInt32(),
                enchantment = ModelName("enchantments", card.GetProperty("enchantment").GetInt32()),
                affliction = ModelName("afflictions", card.GetProperty("affliction").GetInt32()) };
        }
        object Creature(JsonElement creature) => new {
            combat_id = creature.GetProperty("combat_id").GetInt32(), hp = creature.GetProperty("current_hp").GetInt32(),
            max_hp = creature.GetProperty("max_hp").GetInt32(), block = creature.GetProperty("block").GetInt32(),
            hp_display = creature.GetProperty("hp_display").GetInt32(), attached = creature.GetProperty("attached").GetBoolean(),
            powers = creature.GetProperty("powers").EnumerateArray().Select(reference => {
                JsonElement power = powers[reference.GetInt32()];
                return new { id = ModelName("powers", power.GetProperty("id").GetInt32()), amount = power.GetProperty("amount").GetInt32() };
            }).ToArray() };
        JsonElement rng = state.GetProperty("run").GetProperty("rng").GetProperty("rngs");
        if (state.GetProperty("run").GetProperty("player").GetProperty("potions").EnumerateArray()
            .Skip(fixture.Player.PotionSlots.Count).Any(reference => reference.GetInt32() != 0))
            throw new InvalidOperationException("env_has_potion_outside_native_slots");
        RunRngType[] streams = [RunRngType.Shuffle, RunRngType.CombatCardGeneration, RunRngType.CombatPotionGeneration,
            RunRngType.CombatCardSelection, RunRngType.CombatEnergyCosts, RunRngType.CombatTargets,
            RunRngType.CombatOrbs, RunRngType.MonsterAi, RunRngType.Niche];
        object? terminal = snapshot.Terminal is { } stamp
            ? new { player_turn = stamp.PlayerTurn, outcome = stamp.Outcome.ToString().ToLowerInvariant() } : null;
        return JsonSerializer.SerializeToElement(new {
            terminal,
            in_progress = combat.GetProperty("is_in_progress").GetBoolean(), pending_loss = combat.GetProperty("pending_loss").GetBoolean(),
            player_turn = pcs.GetProperty("turn_number").GetInt32(),
            energy = pcs.GetProperty("energy").GetInt32(), stars = pcs.GetProperty("stars").GetInt32(), phase = pcs.GetProperty("phase").GetInt32(),
            round = combat.GetProperty("round_number").GetInt32(), side = combat.GetProperty("current_side").GetInt32(),
            creatures = fixture.RootCreatures.Select(creature => Creature(creatures.Values.Single(value => value.GetProperty("combat_id").GetInt32() == creature.CombatId))).ToArray(),
            enemies = combat.GetProperty("enemies").EnumerateArray().Select(reference => creatures[reference.GetInt32()].GetProperty("combat_id").GetInt32()).ToArray(),
            piles = pcs.GetProperty("piles").EnumerateArray().Select(pile => pile.EnumerateArray().Select(reference => Card(reference.GetInt32())).ToArray()).ToArray(),
            potions = state.GetProperty("run").GetProperty("player").GetProperty("potions").EnumerateArray().Take(fixture.Player.PotionSlots.Count).Select(reference => reference.GetInt32() == 0 ? "" : ModelName("potions", potions[reference.GetInt32()].GetProperty("id").GetInt32())).ToArray(),
            rng = streams.Select(stream => rng[(int)stream].Clone()).ToArray()
        });
    }
    private static Dictionary<int, JsonElement> Slots(JsonElement pool) => pool.GetProperty("slots").EnumerateArray()
        .ToDictionary(slot => slot.GetProperty("index").GetInt32(), slot => slot.GetProperty("value"));
}
