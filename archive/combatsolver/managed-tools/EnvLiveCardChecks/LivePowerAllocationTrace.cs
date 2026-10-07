using System.Text.Json;
using System.Text.Json.Nodes;
using CombatSolver;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Models;

// Fixture-only reference namespace across frozen captures. Native powers have
// CLR identity, while Env powers use persistent pool refs. The ordinary capture
// assigns dense refs in creature/list order each time, which is not allocation
// order once powers are added to several owners on successive turns.
//
// Creature.ApplyPowerInternal appends to the owner's list and synchronously
// raises PowerApplied. This observer only records the native instance and event
// order; it neither writes gameplay values nor consults an Env successor. It is
// valid only for this fixed-roster fixture with observed applications/removals.
// General allocation tracing must also cover ToMutable objects never applied,
// unobserved removed-object closure and branch-specific lifetimes before using this in a
// production root mapper.
internal sealed class LivePowerAllocationTrace : IDisposable
{
    private readonly Creature[] _creatures;
    private readonly CombatState _combat;
    private readonly Dictionary<PowerModel, int> _references = new(ReferenceEqualityComparer.Instance);
    private readonly HashSet<PowerModel> _removed = new(ReferenceEqualityComparer.Instance);
    private readonly List<(Creature Creature, Action<PowerModel> Applied, Action<PowerModel> Removed)> _subscriptions = [];
    private bool _disposed;

    internal LivePowerAllocationTrace(IReadOnlyList<Creature> creatures)
    {
        _creatures = creatures.ToArray();
        if (_creatures.Length == 0 || _creatures[0].CombatState is not CombatState combat
            || _creatures.Distinct(ReferenceEqualityComparer.Instance).Count() != _creatures.Length)
            throw new InvalidOperationException("native_power_trace_roster_invalid");
        _combat = combat;
        RequireRoster();
        // Existing instances establish the same dense namespace as the first
        // root capture. All subsequent new refs come only from native events.
        foreach (Creature creature in _creatures)
            foreach (PowerModel power in creature.Powers)
                Register(creature, power);
        foreach (Creature creature in _creatures)
        {
            Action<PowerModel> handler = power => Register(creature, power);
            Action<PowerModel> removed = power => Removed(creature, power);
            creature.PowerApplied += handler;
            creature.PowerRemoved += removed;
            _subscriptions.Add((creature, handler, removed));
        }
    }

    internal EnvLiveCreatures Reindex(EnvLiveCreatures component)
    {
        if (_disposed) throw new InvalidOperationException("native_power_trace_disposed");
        RequireRoster();
        JsonObject document = JsonNode.Parse(component.Component.GetRawText())!.AsObject();
        JsonObject pool = document["powers"]!.AsObject();
        JsonArray slots = pool["slots"]!.AsArray();
        JsonArray creatureSlots = document["creatures"]!["slots"]!.AsArray();
        if (creatureSlots.Count != _creatures.Length || pool["capacity"]!.GetValue<int>() != 191
            || pool["count"]!.GetValue<int>() != slots.Count || pool["next"]!.GetValue<int>() != slots.Count + 1)
            throw new InvalidOperationException("native_power_trace_component_layout");

        Dictionary<int, int> rewritten = [];
        HashSet<PowerModel> current = new(ReferenceEqualityComparer.Instance);
        int localReference = 0;
        for (int creatureIndex = 0; creatureIndex < _creatures.Length; creatureIndex++)
        {
            Creature creature = _creatures[creatureIndex];
            JsonObject slot = creatureSlots[creatureIndex]!.AsObject();
            JsonObject value = slot["value"]!.AsObject();
            JsonArray powerRefs = value["powers"]!.AsArray();
            if (slot["index"]!.GetValue<int>() != creatureIndex + 1
                || creature.CombatId is not { } id || value["combat_id"]!.GetValue<int>() != id
                || powerRefs.Count != creature.Powers.Count)
                throw new InvalidOperationException("native_power_trace_component_roster");
            for (int powerIndex = 0; powerIndex < creature.Powers.Count; powerIndex++)
            {
                PowerModel power = creature.Powers[powerIndex];
                if (!ReferenceEquals(power.Owner, creature) || !current.Add(power)
                    || !_references.TryGetValue(power, out int persistentReference))
                    throw new InvalidOperationException("native_power_trace_unrecorded_or_aliased_power");
                int originalReference = ++localReference;
                if (powerRefs[powerIndex]!.GetValue<int>() != originalReference)
                    throw new InvalidOperationException("native_power_trace_component_power_order");
                rewritten.Add(originalReference, persistentReference);
            }
        }
        if (slots.Count != current.Count || current.Any(power => _removed.Contains(power)))
            throw new InvalidOperationException("native_power_trace_removed_or_missing_power");

        JsonObject[] reordered = new JsonObject[_references.Count];
        HashSet<int> consumed = [];
        foreach (JsonNode? node in slots)
        {
            JsonObject slot = node!.AsObject();
            int originalReference = slot["index"]!.GetValue<int>();
            if (!consumed.Add(originalReference) || !rewritten.TryGetValue(originalReference, out int persistentReference))
                throw new InvalidOperationException("native_power_trace_component_power_slot");
            // Slot values are copied verbatim from this native capture.
            JsonObject owned = slot.DeepClone().AsObject();
            owned["index"] = persistentReference;
            reordered[persistentReference - 1] = owned;
        }
        int CreatureReference(Creature? creature)
        {
            if (creature is null) return 0;
            int index = Array.FindIndex(_creatures, value => ReferenceEquals(value, creature));
            if (index < 0) throw new InvalidOperationException("native_power_trace_external_creature");
            return index + 1;
        }
        foreach (var (power, reference) in _references)
        {
            if (current.Contains(power)) continue;
            if (!_removed.Contains(power))
                throw new InvalidOperationException("native_power_trace_unobserved_removal");
            // Capture the current native object, including amount/vars after
            // removal; never reuse a previous value or inspect the Env pool.
            reordered[reference - 1] = new JsonObject {
                ["index"] = reference,
                ["value"] = JsonNode.Parse(EnvLiveCreatures.CaptureRetainedPower(power, CreatureReference).GetRawText()),
            };
        }
        foreach (JsonNode? node in creatureSlots)
        {
            JsonObject value = node!["value"]!.AsObject();
            JsonArray powerRefs = value["powers"]!.AsArray();
            for (int index = 0; index < powerRefs.Count; index++)
                powerRefs[index] = rewritten[powerRefs[index]!.GetValue<int>()];
        }
        pool["slots"] = new JsonArray(reordered.Select(slot => (JsonNode?)slot).ToArray());
        pool["count"] = _references.Count;
        pool["next"] = _references.Count + 1;
        return new EnvLiveCreatures(JsonSerializer.SerializeToElement(document));
    }

    private void Register(Creature creature, PowerModel power)
    {
        if (_disposed) throw new InvalidOperationException("native_power_trace_disposed");
        RequireRoster();
        if (!power.IsMutable || !ReferenceEquals(power.Owner, creature)
            || !creature.Powers.Contains(power, ReferenceEqualityComparer.Instance)
            || _references.ContainsKey(power))
            throw new InvalidOperationException("native_power_trace_application_identity");
        if (_references.Count >= 191) throw new InvalidOperationException("native_power_trace_capacity");
        _references.Add(power, _references.Count + 1);
    }

    private void Removed(Creature creature, PowerModel power)
    {
        if (_disposed) throw new InvalidOperationException("native_power_trace_disposed");
        RequireRoster();
        if (!ReferenceEquals(power.Owner, creature) || creature.Powers.Contains(power, ReferenceEqualityComparer.Instance)
            || !_references.ContainsKey(power) || !_removed.Add(power))
            throw new InvalidOperationException("native_power_trace_removal_identity");
    }

    private void RequireRoster()
    {
        if (!_creatures.SequenceEqual(_combat.Creatures, ReferenceEqualityComparer.Instance)
            || _creatures.Any(creature => !ReferenceEquals(creature.CombatState, _combat)))
            throw new InvalidOperationException("native_power_trace_roster_changed");
    }

    public void Dispose()
    {
        if (_disposed) return;
        foreach (var (creature, applied, removed) in _subscriptions)
        { creature.PowerApplied -= applied; creature.PowerRemoved -= removed; }
        _subscriptions.Clear();
        _disposed = true;
    }
}
