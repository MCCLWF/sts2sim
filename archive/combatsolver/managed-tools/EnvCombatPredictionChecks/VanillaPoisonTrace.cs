using System.Reflection;
using System.Text.Json;
using HarmonyLib;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Entities.Rngs;
using MegaCrit.Sts2.Core.Models.Powers;
using MegaCrit.Sts2.Core.Runs;

// Observation only. This scoped prefix returns void and leaves the original
// AfterSideTurnStart Task untouched. It records when the real Poison listener
// receives its owner as a participant. HP events likewise only read scalars.
// Presentation/IO skips are exclusively the documented Headless fixture scope.
internal sealed class VanillaPoisonTrace : IDisposable
{
    private static Action<PoisonPower, CombatSide, IReadOnlyList<Creature>>? _observer;
    private readonly Harmony _harmony = new("sts2env.checks.vanilla-poison-observation");
    private readonly MethodInfo _method = typeof(PoisonPower).GetMethod(nameof(PoisonPower.AfterSideTurnStart))!;
    private readonly Player _player;
    private readonly CombatState _combat;
    private readonly List<JsonElement> _rows = [];
    private int _poisonEntries;
    internal VanillaPoisonTrace(Player player, CombatState combat)
    {
        if (_observer is not null) throw new InvalidOperationException("nested_poison_trace");
        _player = player; _combat = combat;
        _observer = (power, side, participants) =>
        {
            if (ReferenceEquals(power.Owner, player.Creature) && participants.Contains(player.Creature))
            { _poisonEntries++; Record("real_poison_after_side_turn_start_entry"); }
        };
        _harmony.Patch(_method, prefix: new HarmonyMethod(typeof(VanillaPoisonTrace), nameof(Observe)));
        player.Creature.CurrentHpChanged += OnHpChanged;
    }
    private static void Observe(PoisonPower __instance, CombatSide side, IReadOnlyList<Creature> participants)
        => _observer?.Invoke(__instance, side, participants);
    private void OnHpChanged(int oldHp, int newHp) => Record("real_current_hp_changed");
    internal void Record(string boundary)
    {
        PlayerCombatState pcs = _player.PlayerCombatState!;
        RunState run = (RunState)_combat.RunState;
        RunRngType[] streams = [RunRngType.Shuffle, RunRngType.CombatCardGeneration, RunRngType.CombatPotionGeneration,
            RunRngType.CombatCardSelection, RunRngType.CombatEnergyCosts, RunRngType.CombatTargets,
            RunRngType.CombatOrbs, RunRngType.MonsterAi, RunRngType.Niche];
        _rows.Add(JsonSerializer.SerializeToElement(new {
            boundary, player_turn = pcs.TurnNumber, round = _combat.RoundNumber, side = (int)_combat.CurrentSide,
            phase = (int)pcs.Phase, hp = _player.Creature.CurrentHp, energy = pcs.Energy,
            piles = new[] { pcs.Hand, pcs.DrawPile, pcs.DiscardPile, pcs.ExhaustPile, pcs.PlayPile }
                .Select(pile => pile.Cards.Select(card => card.Id.Entry).ToArray()).ToArray(),
            rng = streams.Select(stream => {
                var rng = run.Rng.GetRng(stream);
                return new { stream = stream.ToString(), counter = rng._counter,
                    s0 = rng._random._s0, s1 = rng._random._s1, s2 = rng._random._s2, s3 = rng._random._s3 };
            }).ToArray()
        }));
    }
    internal JsonElement Complete()
    {
        if (_poisonEntries != 1) throw new InvalidOperationException("vanilla_poison_owner_hook_entry_count:" + _poisonEntries);
        JsonElement entry = _rows.Single(row => row.GetProperty("boundary").GetString() == "real_poison_after_side_turn_start_entry");
        if (entry.GetProperty("player_turn").GetInt32() != 2 || entry.GetProperty("side").GetInt32() != (int)CombatSide.Player
            || entry.GetProperty("hp").GetInt32() != 3 || entry.GetProperty("piles")[0].GetArrayLength() != 5)
            throw new InvalidOperationException("vanilla_player_poison_did_not_trigger_after_T_plus_1_draw");
        return JsonSerializer.SerializeToElement(new {
            provenance = "real_sts2_dll_production_CombatManager_StartTurn_and_PoisonPower_AfterSideTurnStart",
            observation_fixture = "nonlethal_HP3_to_HP1_after_independent_shadow_comparison",
            test_mode = false, shadow_successor = false, full_state_comparison = false, rows = _rows
        });
    }
    public void Dispose()
    {
        _player.Creature.CurrentHpChanged -= OnHpChanged;
        _harmony.Unpatch(_method, HarmonyPatchType.All, _harmony.Id);
        _observer = null;
    }
}
