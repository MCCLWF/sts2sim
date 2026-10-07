using System.Globalization;
using System.Text.Json;
using System.Text.Json.Nodes;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Localization.DynamicVars;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Orbs;

namespace CombatSolver;

// Active inventory only, with owned local pool references. This does not prove
// historical pool allocation or pending Hook listener snapshots of a full root.
internal sealed record EnvLiveInventory(JsonElement Component)
{
    internal static void Warmup(Player player)
    {
        foreach (RelicModel relic in player.Relics) _ = relic.DynamicVars;
        foreach (PotionModel potion in player.Potions) _ = potion.DynamicVars;
    }

    internal static EnvLiveInventory Capture(Player player)
    {
        EnvModelCatalog.RequireGameVersion();
        var pcs = player.PlayerCombatState ?? throw new InvalidOperationException("live_inventory_missing_player_combat");
        JsonArray relics = [], potions = [], orbs = [], potionSlots = [];
        HashSet<AbstractModel> seen = new(ReferenceEqualityComparer.Instance);
        void Register(AbstractModel model)
        {
            if (!model.IsMutable || !seen.Add(model)) throw new InvalidOperationException("live_inventory_immutable_or_aliased_instance");
        }
        foreach (RelicModel relic in player.Relics)
        {
            Register(relic);
            JsonElement metadata = RequireModel(relic, "relics");
            EnvModelCatalog.RequireSubclassFields(relic, typeof(RelicModel), metadata.GetProperty("fields"));
            if (!ReferenceEquals(relic._owner, player) || relic.StackCount != 1)
                throw new InvalidOperationException($"live_relic_owner_or_stack_unmapped:{relic.Id.Entry}");
            if (metadata.GetProperty("fields").GetArrayLength() != 0)
                throw new InvalidOperationException($"live_relic_subclass_fields_unmapped:{relic.Id.Entry}");
            if (!Enum.IsDefined(relic._status) || relic._floorAddedToDeck is < short.MinValue or > short.MaxValue)
                throw new InvalidOperationException("live_relic_status_or_floor_capacity");
            JsonArray variables = CaptureVariables(relic._dynamicVars, metadata, relic.Id.Entry, requireBaseline: false);
            if (variables.Count > 3) throw new InvalidOperationException("live_relic_variable_capacity");
            while (variables.Count < 3) variables.Add(Decimal(0));
            relics.Add(Slot(relics.Count + 1, new JsonObject
            {
                ["id"] = metadata.GetProperty("id").GetInt32(), ["owner"] = 1,
                ["removed"] = relic.HasBeenRemovedFromState, ["is_melted"] = relic._isMelted, ["is_wax"] = relic._isWax,
                ["status"] = (int)relic._status, ["floor_added"] = relic._floorAddedToDeck,
                ["vars"] = variables, ["extra"] = new JsonArray(0, 0, 0, 0),
            }));
        }
        foreach (PotionModel? potion in player.PotionSlots)
        {
            if (potion is null) { potionSlots.Add(0); continue; }
            Register(potion);
            JsonElement metadata = RequireModel(potion, "potions");
            EnvModelCatalog.RequireSubclassFields(potion, typeof(PotionModel), metadata.GetProperty("fields"));
            if (!ReferenceEquals(potion._owner, player) || potion.IsQueued)
                throw new InvalidOperationException($"live_potion_owner_or_queued_unmapped:{potion.Id.Entry}");
            if (metadata.GetProperty("fields").GetArrayLength() != 0)
                throw new InvalidOperationException($"live_potion_subclass_fields_unmapped:{potion.Id.Entry}");
            // C++ stores potion variables in the immutable definition table.
            // Every mutable instance variable must still equal that baseline.
            _ = CaptureVariables(potion._dynamicVars, metadata, potion.Id.Entry, requireBaseline: true);
            int reference = potions.Count + 1;
            potionSlots.Add(reference);
            potions.Add(Slot(reference, new JsonObject
            {
                ["id"] = metadata.GetProperty("id").GetInt32(), ["owner"] = 1,
                ["removed"] = potion.HasBeenRemovedFromState, ["extra"] = new JsonArray(0, 0),
            }));
        }
        foreach (OrbModel orb in pcs.OrbQueue.Orbs)
        {
            Register(orb);
            JsonElement metadata = RequireModel(orb, "orbs");
            EnvModelCatalog.RequireSubclassFields(orb, typeof(OrbModel), metadata.GetProperty("fields"));
            if (!ReferenceEquals(orb.Owner, player)) throw new InvalidOperationException("live_orb_foreign_owner");
            string[] expected = orb switch { DarkOrb => ["_evokeVal"], GlassOrb => ["_passiveVal"], _ => [] };
            if (!metadata.GetProperty("fields").EnumerateArray().Select(field => field.GetString()!).SequenceEqual(expected))
                throw new InvalidOperationException($"live_orb_subclass_fields_unmapped:{orb.Id.Entry}");
            orbs.Add(Slot(orbs.Count + 1, new JsonObject
            {
                ["id"] = metadata.GetProperty("id").GetInt32(), ["owner"] = 1, ["removed"] = orb.HasBeenRemovedFromState,
                ["dark_evoke_val"] = Decimal(orb is DarkOrb dark ? dark._evokeVal : 6m),
                ["glass_passive_val"] = Decimal(orb is GlassOrb glass ? glass._passiveVal : 4m),
            }));
        }
        if (relics.Count > 71 || potions.Count > 39 || player.PotionSlots.Count > 8
            || pcs.OrbQueue.Capacity is < 0 or > 10 || orbs.Count > pcs.OrbQueue.Capacity)
            throw new InvalidOperationException("live_inventory_capacity");
        // PlayerData uses a fixed eight-slot array. Slots outside the actual
        // belt do not exist in the game and must be null in the POD encoding.
        while (potionSlots.Count < 8) potionSlots.Add(0);
        return new(JsonSerializer.SerializeToElement(new JsonObject
        {
            ["relics"] = Pool(71, relics), ["potions"] = Pool(39, potions), ["orbs"] = Pool(255, orbs),
            ["player_fields"] = new JsonObject
            {
                ["relics"] = References(relics.Count), ["potions"] = potionSlots,
            },
            ["player_combat_fields"] = new JsonObject
            {
                ["orb_capacity"] = pcs.OrbQueue.Capacity, ["orbs"] = References(orbs.Count),
            },
        }));
    }

    private static JsonArray CaptureVariables(DynamicVarSet? variables, JsonElement metadata, string entry, bool requireBaseline)
    {
        if (variables is null) throw new InvalidOperationException("live_inventory_vars_not_materialized");
        JsonElement definitions = metadata.GetProperty("variables");
        if (!definitions.EnumerateArray().Select(definition => definition.GetProperty("name").GetString()!)
            .ToHashSet(StringComparer.Ordinal).SetEquals(variables.Keys))
            throw new InvalidOperationException($"live_inventory_variable_schema:{entry}");
        JsonArray result = [];
        foreach (JsonElement definition in definitions.EnumerateArray())
        {
            DynamicVar variable = variables[definition.GetProperty("name").GetString()!];
            EnvLiveCards.RequireVariableMetadata(variable, definition, entry);
            if (requireBaseline && !decimal.GetBits(variable.BaseValue).SequenceEqual(decimal.GetBits(decimal.Parse(definition.GetProperty("base").GetString()!, CultureInfo.InvariantCulture))))
                throw new InvalidOperationException($"live_potion_variable_modified:{entry}:{variable.Name}");
            result.Add(Decimal(variable.BaseValue));
        }
        return result;
    }
    private static JsonElement RequireModel(AbstractModel model, string category)
    {
        _ = EnvModelCatalog.Id(model, category);
        return EnvModelCatalog.Content.GetProperty(category).GetProperty(model.Id.Entry);
    }
    private static JsonArray References(int count)
        => new(Enumerable.Range(1, count).Select(index => JsonValue.Create(index)).ToArray());
    private static JsonObject Slot(int index, JsonObject value) => new() { ["index"] = index, ["value"] = value };
    private static JsonObject Pool(int capacity, JsonArray slots)
        => new() { ["capacity"] = capacity, ["next"] = slots.Count + 1, ["count"] = slots.Count, ["slots"] = slots };
    private static JsonObject Decimal(decimal value)
        => new() { ["decimal_bits"] = new JsonArray(decimal.GetBits(value).Select(bits => JsonValue.Create(unchecked((uint)bits))).ToArray()) };
}
