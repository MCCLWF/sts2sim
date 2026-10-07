using System.Reflection;
using System.Text.Json;
using System.Text.Json.Nodes;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Localization.DynamicVars;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Powers;
using MegaCrit.Sts2.Core.ValueProps;

namespace CombatSolver;

// Owned scalar creature/power component, not a complete live root. Creature
// references use the exact supplied ordering shared with EnvLiveCards. Monster
// state is bound separately to the owned Env registry by EnvLiveMonsters.
internal sealed record EnvLiveCreatures(JsonElement Component)
{
    // DynamicVars is lazy. Materialize it before the capture's before stamp.
    internal static void Warmup(CombatState state)
    {
        foreach (Creature creature in state.Creatures)
            foreach (PowerModel power in creature.Powers)
                _ = power.DynamicVars;
    }

    internal static EnvLiveCreatures Capture(CombatState state, Player player, IReadOnlyList<Creature> creatures)
    {
        EnvModelCatalog.RequireGameVersion();
        if (state.GetType() != typeof(CombatState) || state.Players.Count != 1 || !ReferenceEquals(state.Players[0], player))
            throw new InvalidOperationException("live_creatures_require_single_player_CombatState");
        if (!creatures.SequenceEqual(state.Creatures, ReferenceEqualityComparer.Instance) || creatures.Count > 23)
            throw new InvalidOperationException("live_creature_roster_order_or_capacity");
        if (state._nextCreatureId > int.MaxValue)
            throw new InvalidOperationException("live_creature_next_id_overflow");
        Dictionary<Creature, int> references = new(ReferenceEqualityComparer.Instance);
        HashSet<uint> combatIds = [];
        for (int i = 0; i < creatures.Count; i++)
        {
            Creature creature = creatures[i];
            if (creature.GetType() != typeof(Creature) || !references.TryAdd(creature, i + 1))
                throw new InvalidOperationException("live_creature_type_or_duplicate");
            if (creature.CombatId is not { } id || id >= state._nextCreatureId || id > int.MaxValue || !combatIds.Add(id))
                throw new InvalidOperationException("live_creature_combat_id_invalid");
            if (!ReferenceEquals(creature.CombatState, state))
                throw new InvalidOperationException("live_creature_foreign_combat");
        }
        int Reference(Creature? creature) => creature is null ? 0 : references.TryGetValue(creature, out int index) ? index
            : throw new InvalidOperationException("live_creature_reference_outside_roster");
        int PlayerReference(Player? owner) => owner is null ? 0 : ReferenceEquals(owner, player) ? 1
            : throw new InvalidOperationException("live_creature_foreign_player");
        JsonArray Roster(IEnumerable<Creature> roster) => new(roster.Select(creature => JsonValue.Create(Reference(creature))).ToArray());
        int playerCreature = Reference(player.Creature);
        JsonElement? encounterMetadata = state.Encounter is { } encounter ? RequireModel(encounter, "encounters") : null;
        string[] slotNames = encounterMetadata is { } metadata
            ? metadata.GetProperty("slots").EnumerateArray().Select(slot => slot.GetString()!).ToArray() : [];
        if (state.Encounter is { } actualEncounter && !slotNames.SequenceEqual(actualEncounter.Slots, StringComparer.Ordinal))
            throw new InvalidOperationException("live_creature_encounter_slots_modified");
        int Slot(Creature creature)
        {
            if (creature.SlotName is null) return -1;
            int slot = Array.IndexOf(slotNames, creature.SlotName);
            if (slot < 0 || slot > sbyte.MaxValue) throw new InvalidOperationException("live_creature_slot_unmapped");
            return slot;
        }
        JsonArray powerSlots = [], creatureSlots = [], monsterModels = [];
        HashSet<PowerModel> seenPowers = new(ReferenceEqualityComparer.Instance);
        foreach (Creature creature in creatures)
        {
            if ((creature.Player is null) == (creature.Monster is null) || creature.Side is not (CombatSide.Player or CombatSide.Enemy))
                throw new InvalidOperationException("live_creature_model_or_side_invalid");
            // Hellraiser reads HpDisplay.IsInfinite() to cap automatic plays;
            // HardenedShell and WaterfallGiant set this flag. It is therefore
            // gameplay state, despite also controlling the health bar visuals.
            if (!Enum.IsDefined(creature.HpDisplay))
                throw new InvalidOperationException("live_creature_hp_display_invalid");
            if (creature.Powers.Count > 32) throw new InvalidOperationException("live_creature_power_list_capacity");
            if (creature.Monster is { } monster)
                monsterModels.Add(new JsonObject
                {
                    ["combat_id"] = (int)creature.CombatId!.Value,
                    ["id"] = RequireModel(monster, "monsters").GetProperty("id").GetInt32(),
                });
            JsonArray powers = [];
            foreach (PowerModel power in creature.Powers)
            {
                if (!seenPowers.Add(power) || !ReferenceEquals(power.Owner, creature))
                    throw new InvalidOperationException("live_power_duplicate_or_owner_mismatch");
                if (powerSlots.Count >= 191) throw new InvalidOperationException("live_power_pool_capacity");
                int index = powerSlots.Count + 1;
                powers.Add(index);
                powerSlots.Add(new JsonObject { ["index"] = index, ["value"] = CapturePower(power, Reference) });
            }
            creatureSlots.Add(new JsonObject
            {
                ["index"] = Reference(creature),
                ["value"] = new JsonObject
                {
                    ["block"] = creature.Block, ["current_hp"] = creature.CurrentHp, ["max_hp"] = creature.MaxHp, ["hp_display"] = (int)creature.HpDisplay,
                    ["monster_max_hp_before_modification"] = creature.MonsterMaxHpBeforeModification ?? -1,
                    ["combat_id"] = (int)creature.CombatId!.Value, ["side"] = (int)creature.Side, ["attached"] = true,
                    ["player"] = PlayerReference(creature.Player), ["pet_owner"] = PlayerReference(creature.PetOwner),
                    ["slot"] = Slot(creature), ["powers"] = powers,
                },
            });
        }
        var pcs = player.PlayerCombatState ?? throw new InvalidOperationException("live_creatures_missing_player_combat");
        if (pcs.Pets.Count > 8) throw new InvalidOperationException("live_creature_pet_capacity");
        JsonObject component = new()
        {
            ["creatures"] = Pool(23, creatureSlots), ["powers"] = Pool(191, powerSlots),
            ["monster_models"] = monsterModels,
            ["combat_fields"] = new JsonObject
            {
                ["allies"] = Roster(state.Allies), ["enemies"] = Roster(state.Enemies),
                // Removed/escaped references require a larger shared creature
                // closure. Refuse them rather than silently dropping identity.
                ["escaped"] = Roster(state.EscapedCreatures), ["next_creature_id"] = (int)state._nextCreatureId,
            },
            ["player_fields"] = new JsonObject { ["creature"] = playerCreature },
            ["player_combat_fields"] = new JsonObject { ["pets"] = Roster(pcs.Pets) },
        };
        return new(JsonSerializer.SerializeToElement(component));
    }

    // emptyMonster is an owned id=0 default exported from an Env player slot;
    // it has no live MonsterModel counterpart or gameplay RNG. Other values
    // must come from binding the same frozen root's symbolic monster component.
    internal JsonElement Bind(IReadOnlyDictionary<int, JsonElement> monstersByCombatId, JsonElement emptyMonster)
    {
        if (emptyMonster.GetProperty("id").GetInt32() != 0)
            throw new InvalidOperationException("live_creature_player_monster_default_invalid");
        JsonObject pool = JsonNode.Parse(Component.GetProperty("creatures").GetRawText())!.AsObject();
        Dictionary<int, int> expectedModels = Component.GetProperty("monster_models").EnumerateArray()
            .ToDictionary(model => model.GetProperty("combat_id").GetInt32(), model => model.GetProperty("id").GetInt32());
        HashSet<int> consumed = [];
        foreach (JsonNode? node in pool["slots"]!.AsArray())
        {
            JsonObject value = node!["value"]!.AsObject();
            int combatId = value["combat_id"]!.GetValue<int>();
            JsonElement monster = value["player"]!.GetValue<int>() != 0 ? emptyMonster
                : monstersByCombatId.TryGetValue(combatId, out var bound) && consumed.Add(combatId) ? bound
                : throw new InvalidOperationException("live_creature_bound_monster_missing");
            if (value["player"]!.GetValue<int>() == 0
                && (!expectedModels.TryGetValue(combatId, out int expected) || monster.GetProperty("id").GetInt32() != expected))
                throw new InvalidOperationException("live_creature_bound_monster_model_mismatch");
            value["monster"] = JsonNode.Parse(monster.GetRawText());
        }
        if (consumed.Count != monstersByCombatId.Count)
            throw new InvalidOperationException("live_creature_bound_monster_roster_mismatch");
        return JsonSerializer.SerializeToElement(pool);
    }

    // Retained, previously observed powers remain real objects after removal
    // from the owner's list. A caller must supply its audited identity map.
    internal static JsonElement CaptureRetainedPower(PowerModel power, Func<Creature?, int> reference)
    {
        _ = power.DynamicVars;
        return JsonSerializer.SerializeToElement(CapturePower(power, reference));
    }

    private static JsonObject CapturePower(PowerModel power, Func<Creature?, int> reference)
    {
        JsonElement metadata = RequireModel(power, "powers");
        HashSet<string> declaredFields = metadata.GetProperty("fields").EnumerateArray()
            .Select(field => field.GetString()!).ToHashSet(StringComparer.Ordinal);
        for (Type? type = power.GetType(); type is not null && type != typeof(PowerModel); type = type.BaseType)
        {
            foreach (FieldInfo field in type.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly))
            {
                string name = type == power.GetType() ? field.Name : type.Name + "." + field.Name;
                // The exporter omits null reference fields. This exact field
                // only stores NVoidFormVfx; all gameplay state is in Data.
                if (power is VoidFormPower && type == typeof(VoidFormPower) && name == "_vfx") continue;
                if (!declaredFields.Contains(name))
                    throw new InvalidOperationException($"live_power_hidden_subclass_field_unmapped:{power.Id.Entry}:{name}");
            }
        }
        int[] extra = new int[4];
        foreach (JsonElement field in metadata.GetProperty("fields").EnumerateArray())
        {
            string name = field.GetString()!;
            switch (power, name)
            {
                case (RitualPower ritual, "_wasJustAppliedByEnemy"): extra[0] = ritual._wasJustAppliedByEnemy ? 1 : 0; break;
                case (SlothPower sloth, "_cardsPlayedThisTurn"): extra[0] = sloth._cardsPlayedThisTurn; break;
                case (TenderPower tender, "_cardsPlayedThisTurn"): extra[0] = tender._cardsPlayedThisTurn; break;
                // VoidFormPower._vfx is only read to toggle a visual node.
                // HideTemporaryZeroCostVisual changes Data and is mapped below.
                case (VoidFormPower, "_vfx"): break;
                case (NemesisPower nemesis, "_shouldApplyIntangible"): extra[0] = nemesis._shouldApplyIntangible ? 1 : 0; break;
                case (SurroundedPower surrounded, "_facing"):
                    if (!Enum.IsDefined(surrounded._facing)) throw new InvalidOperationException("live_power_surrounded_direction_invalid");
                    extra[0] = surrounded._facing == SurroundedPower.Direction.Left ? 1 : 0; break;
                default: throw new InvalidOperationException($"live_power_subclass_field_unmapped:{power.Id.Entry}:{name}");
            }
        }
        decimal? privateDecimal = CaptureInternalData(power, extra);
        var variables = power._dynamicVars ?? throw new InvalidOperationException("live_power_vars_not_materialized");
        JsonElement definitions = metadata.GetProperty("variables");
        if (definitions.GetArrayLength() > 3 || !definitions.EnumerateArray().Select(var => var.GetProperty("name").GetString()!)
                .ToHashSet(StringComparer.Ordinal).SetEquals(variables.Keys))
            throw new InvalidOperationException($"live_power_variable_schema:{power.Id.Entry}");
        JsonArray vars = [];
        foreach (JsonElement definition in definitions.EnumerateArray())
        {
            DynamicVar variable = variables[definition.GetProperty("name").GetString()!];
            RequireVariable(variable, definition, power.Id.Entry);
            vars.Add(Decimal(variable.BaseValue));
        }
        while (vars.Count < 3) vars.Add(Decimal(0));
        if (privateDecimal is { } counter)
        {
            // HardenedShell has no canonical variables. The Env reserves its
            // first slot for the exact decimal in the shipped private Data.
            if (definitions.GetArrayLength() != 0)
                throw new InvalidOperationException("live_power_private_decimal_slot_conflict");
            vars[0] = Decimal(counter);
        }
        return new JsonObject
        {
            ["id"] = metadata.GetProperty("id").GetInt32(), ["amount"] = power._amount,
            ["amount_on_turn_start"] = power._amountOnTurnStart, ["skip_next_duration_tick"] = power._skipNextDurationTick,
            ["owner"] = reference(power._owner), ["applier"] = reference(power._applier), ["target"] = reference(power._target),
            ["vars"] = vars, ["extra"] = new JsonArray(extra.Select(value => JsonValue.Create(value)).ToArray()),
        };
    }

    private static decimal? CaptureInternalData(PowerModel power, int[] extra)
    {
        object? data = power._internalData;
        if (data is null)
        {
            if (power.GetType().GetMethod("InitInternalData", BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic)!
                    .DeclaringType != typeof(PowerModel))
                throw new InvalidOperationException($"live_power_internal_data_missing:{power.Id.Entry}");
            return null;
        }
        Type type = data.GetType();
        FieldInfo[] fields = type.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
        if (type.Assembly != typeof(PowerModel).Assembly || type.DeclaringType != power.GetType() || type.Name != "Data")
            throw new InvalidOperationException($"live_power_internal_data_unmapped:{power.Id.Entry}");
        if (power is HellraiserPower)
        {
            if (!fields.Select(f => f.Name).ToHashSet(StringComparer.Ordinal).SetEquals(
                    new[] { "autoPlayingCards", "infiniteAutoPlaysThisTurn", "showedCapReachedMessage" })
                || fields.Single(f => f.Name == "autoPlayingCards").GetValue(data) is not HashSet<CardModel> playing
                || fields.Single(f => f.Name == "infiniteAutoPlaysThisTurn").GetValue(data) is not int count
                || fields.Single(f => f.Name == "showedCapReachedMessage").GetValue(data) is not bool)
                throw new InvalidOperationException("live_power_internal_data_unmapped:HELLRAISER_POWER");
            if (playing.Count != 0)
                throw new InvalidOperationException("live_power_hellraiser_autoplay_in_progress");
            // autoPlayingCards is only read by BeforeAttack's VFX branch;
            // showedCapReachedMessage only suppresses a ThinkCmd message.
            // The idle boundary proves no automatic play is in progress.
            extra[0] = count;
            return null;
        }
        // Each pair is checked against the pinned source and its C++ slot.
        string? field = power switch
        {
            AutomationPower => "cardsLeft", DarkEmbracePower => "etherealCount", JugglingPower => "attacksPlayedThisTurn",
            VoidFormPower => "cardsPlayedThisTurn",
            PanachePower => "alreadyApplied", SkittishPower => "hasGainedBlockThisTurn",
            AdaptablePower or ReattachPower => "isReviving", HardenedShellPower => "damageReceivedThisTurn", _ => null,
        };
        if (field is null || fields.Length != 1 || fields[0].Name != field)
            throw new InvalidOperationException($"live_power_internal_data_unmapped:{power.Id.Entry}");
        object? value = fields[0].GetValue(data);
        if (power is HardenedShellPower)
            return value is decimal number ? number
                : throw new InvalidOperationException("live_power_internal_scalar_unmapped:HARDENED_SHELL_POWER");
        extra[0] = value switch
        {
            bool flag => flag ? 1 : 0, int number => number,
            _ => throw new InvalidOperationException($"live_power_internal_scalar_unmapped:{power.Id.Entry}"),
        };
        if (power is AutomationPower && extra[0] <= 0)
            throw new InvalidOperationException("live_power_automation_nonpositive_counter_unmapped");
        return null;
    }

    private static JsonElement RequireModel(AbstractModel model, string category)
    {
        if (model.GetType().Assembly != typeof(AbstractModel).Assembly
            || !EnvModelCatalog.Content.GetProperty(category).TryGetProperty(model.Id.Entry, out JsonElement metadata)
            || metadata.GetProperty("type").GetString() != model.GetType().FullName)
            throw new InvalidOperationException($"live_{category}_unknown_model:{model.Id.Entry}");
        return metadata;
    }

    private static void RequireVariable(DynamicVar variable, JsonElement definition, string entry)
    {
        Type type = variable.GetType();
        string name = type.IsGenericType ? type.Name.Split('`')[0] + "<" + string.Join(",", type.GetGenericArguments().Select(t => t.Name)) + ">" : type.Name;
        bool supported = type.Assembly == typeof(DynamicVar).Assembly && name == definition.GetProperty("type").GetString()
            && (name is "DynamicVar" or "DamageVar" or "BlockVar" or "CardsVar" or "EnergyVar" or "HealVar" or "HpLossVar"
                or "IntVar" or "MaxHpVar" or "RepeatVar" or "StarsVar" or "GoldVar" || name.StartsWith("PowerVar<", StringComparison.Ordinal));
        if (!supported) throw new InvalidOperationException($"live_power_variable_type_unmapped:{entry}:{variable.Name}:{name}");
        ValueProp? props = variable switch { DamageVar damage => damage.Props, BlockVar block => block.Props, _ => null };
        if (props is { } actual && (!definition.TryGetProperty("props", out JsonElement properties)
            || !properties.TryGetProperty("Props", out JsonElement flags) || actual != Enum.Parse<ValueProp>(flags.GetString()!)))
            throw new InvalidOperationException($"live_power_variable_props_unmapped:{entry}:{variable.Name}");
    }

    private static JsonObject Pool(int capacity, JsonArray slots)
        => new() { ["capacity"] = capacity, ["next"] = slots.Count + 1, ["count"] = slots.Count, ["slots"] = slots };
    private static JsonObject Decimal(decimal value)
        => new() { ["decimal_bits"] = new JsonArray(decimal.GetBits(value).Select(bits => JsonValue.Create(unchecked((uint)bits))).ToArray()) };
}
