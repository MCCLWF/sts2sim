using System.Globalization;
using System.Text.Json;
using System.Text.Json.Nodes;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Localization.DynamicVars;
using MegaCrit.Sts2.Core.ValueProps;

namespace CombatSolver;

// A component of the live root, not a complete combat snapshot. Owns no game
// objects. References are local slot numbers; the creature component must use
// the same ordered creature identities supplied during capture.
internal sealed record EnvLiveCards(JsonElement Component)
{
    private static JsonElement Catalog => EnvModelCatalog.Content;

    // Warm lazy variables before the native capture's before/after stamp. Do not
    // invoke BaseStarCost or Keywords(All): they initialize state or read hooks.
    internal static void Warmup(CombatState state)
    {
        if (state.RunState is not RunState run)
            return;
        List<CardModel> cards = [];
        HashSet<CardModel> seen = new(ReferenceEqualityComparer.Instance);
        void Add(CardModel? card) { if (card is not null && seen.Add(card)) cards.Add(card); }
        foreach (CardModel card in run._allCards.Concat(state._allCards)) Add(card);
        foreach (var entry in CombatManager.Instance.History.CardPlaysFinished) Add(entry.CardPlay.Card);
        for (int i = 0; i < cards.Count; i++)
        {
            CardModel card = cards[i];
            Add(card.CloneOf);
            Add(card.DeckVersion);
            _ = card.DynamicVars;
            _ = card.EnergyCost;
        }
    }

    internal static EnvLiveCards Capture(CombatState state, Player player, IReadOnlyList<Creature> creatures,
                                         IReadOnlyDictionary<CardModel, int> lastPlayedTurns)
    {
        if (state.RunState is not RunState run || state.Players.Count != 1)
            throw new InvalidOperationException("live_cards_require_single_player_RunState");
        // Game network IDs include models registered by installed mods. Env IDs
        // belong to its pinned catalog; validate the actual game binary and each
        // active model binding instead of comparing the full modded ID table.
        EnvModelCatalog.RequireGameVersion();
        if (player.PlayerCombatState is not { } playerCombat || playerCombat.PlayPile.Cards.Count != 0)
            throw new InvalidOperationException("live_cards_require_idle_play_pile");
        if (state.Creatures.Any(creature => creature.Powers.Any(power => power.Id.Entry == "DAMPEN_POWER")))
            throw new InvalidOperationException("live_card_dampen_history_unmapped");
        List<CardModel> cards = [];
        Dictionary<CardModel, int> references = new(ReferenceEqualityComparer.Instance);
        void Register(CardModel? card)
        {
            if (card is null || references.ContainsKey(card)) return;
            if (cards.Count >= 319) throw new InvalidOperationException("live_card_pool_capacity");
            references.Add(card, cards.Count + 1);
            cards.Add(card);
        }
        foreach (CardModel card in run._allCards.Concat(state._allCards)) Register(card);
        foreach (var pair in lastPlayedTurns) Register(pair.Key);
        // Reachable removed originals and deck versions remain distinct objects.
        for (int i = 0; i < cards.Count; i++)
        {
            Register(cards[i].CloneOf);
            Register(cards[i].DeckVersion);
        }
        int Reference(CardModel? card) => card is null ? 0 : references.TryGetValue(card, out int reference) ? reference
            : throw new InvalidOperationException($"live_card_reference_not_registered:{card.Id.Entry}");
        JsonArray Pile(CardPile pile) => new(pile.Cards.Select(card => JsonValue.Create(Reference(card))).ToArray());
        JsonArray slots = [];
        for (int i = 0; i < cards.Count; i++)
        {
            CardModel card = cards[i];
            JsonObject value = CaptureCard(card, player, creatures, lastPlayedTurns);
            value["clone_of"] = Reference(card.CloneOf);
            value["deck_version"] = Reference(card.DeckVersion);
            slots.Add(new JsonObject { ["index"] = i + 1, ["value"] = value });
        }
        PlayerCombatState pcs = player.PlayerCombatState ?? throw new InvalidOperationException("missing live player combat state");
        JsonObject component = new()
        {
            ["model_id_hash"] = Catalog.GetProperty("model_id_hash").GetUInt32(),
            ["cards"] = new JsonObject { ["capacity"] = 319, ["next"] = cards.Count + 1, ["count"] = cards.Count, ["slots"] = slots },
            ["deck"] = Pile(player.Deck),
            ["piles"] = new JsonArray(Pile(pcs.Hand), Pile(pcs.DrawPile), Pile(pcs.DiscardPile), Pile(pcs.ExhaustPile), Pile(pcs.PlayPile)),
        };
        return new(JsonSerializer.SerializeToElement(component));
    }

    private static JsonObject CaptureCard(CardModel card, Player player, IReadOnlyList<Creature> creatures,
                                           IReadOnlyDictionary<CardModel, int> lastPlayedTurns)
    {
        if (card.GetType().Assembly != typeof(CardModel).Assembly
            || !Catalog.GetProperty("cards").TryGetProperty(card.Id.Entry, out JsonElement metadata))
            throw new InvalidOperationException($"live_card_unknown_model:{card.Id.Entry}");
        if (card.GetType().FullName != metadata.GetProperty("type").GetString())
            throw new InvalidOperationException($"live_card_model_type_mismatch:{card.Id.Entry}");
        EnvModelCatalog.RequireSubclassFields(card, typeof(CardModel), metadata.GetProperty("subclass_fields"));
        JsonElement subclassFields = metadata.GetProperty("subclass_fields");
        bool mappedGrowth = card is Rampage or Thrash;
        string? growthField = card switch { Rampage => "_extraDamageFromPlays", Thrash => "_extraDamage", _ => null };
        if (mappedGrowth && (subclassFields.GetArrayLength() != 1 || subclassFields[0].GetString() != growthField))
            throw new InvalidOperationException($"live_card_growth_field_schema:{card.Id.Entry}");
        if (!mappedGrowth && subclassFields.GetArrayLength() != 0)
            throw new InvalidOperationException($"live_card_subclass_fields_unmapped:{card.Id.Entry}");
        if (card.Enchantment is not null || card.Affliction is not null)
            throw new InvalidOperationException($"live_card_attachment_unmapped:{card.Id.Entry}");
        if (card.Owner is not null && !ReferenceEquals(card.Owner, player))
            throw new InvalidOperationException("live_card_unknown_owner");
        string[] expectedTags = metadata.GetProperty("tags").EnumerateArray().Select(tag => tag.GetString()!).ToArray();
        if (!expectedTags.ToHashSet(StringComparer.Ordinal).SetEquals(card.Tags.Select(tag => tag.ToString())))
            throw new InvalidOperationException($"live_card_modified_static_tags:{card.Id.Entry}");
        int target = 0;
        if (card.CurrentTarget is { } currentTarget)
        {
            for (int i = 0; i < creatures.Count; i++)
                if (ReferenceEquals(creatures[i], currentTarget)) { target = i + 1; break; }
            if (target == 0) throw new InvalidOperationException("live_card_target_not_registered");
        }
        var cost = card._energyCost ?? throw new InvalidOperationException("live_card_cost_not_materialized");
        var vars = card._dynamicVars ?? throw new InvalidOperationException("live_card_vars_not_materialized");
        string[] names = metadata.GetProperty("variables").EnumerateArray().Select(v => v.GetString()!).ToArray();
        if (!names.ToHashSet(StringComparer.Ordinal).SetEquals(vars.Keys))
            throw new InvalidOperationException($"live_card_variable_schema:{card.Id.Entry}");
        int upgrade = card.CurrentUpgradeLevel;
        JsonElement levels = metadata.GetProperty("levels");
        if (upgrade < 0 || upgrade >= levels.GetArrayLength())
            throw new InvalidOperationException($"live_card_upgrade_unmapped:{card.Id.Entry}:{upgrade}");
        JsonArray overrides = [];
        for (int index = 0; index < names.Length; index++)
        {
            RequireVariableMetadata(vars[names[index]], metadata.GetProperty("variable_metadata")[index], card.Id.Entry, card);
            decimal baseline = decimal.Parse(levels[upgrade][index].GetString()!, CultureInfo.InvariantCulture);
            decimal actual = vars[names[index]].BaseValue;
            if (!decimal.GetBits(actual).SequenceEqual(decimal.GetBits(baseline)))
                overrides.Add(new JsonObject { ["index"] = index, ["value"] = Decimal(actual) });
        }
        if (overrides.Count > 3 || cost._localModifiers.Count > 4 || card._temporaryStarCosts.Count > 3)
            throw new InvalidOperationException($"live_card_component_capacity:{card.Id.Entry}");
        ulong keywords = 0;
        foreach (CardKeyword keyword in card._keywords ?? card.CanonicalKeywords.ToHashSet())
        {
            int index = (int)keyword;
            if (index < 0 || index > 7) throw new InvalidOperationException("live_card_unknown_keyword");
            keywords |= 1UL << index;
        }
        return new JsonObject
        {
            ["id"] = metadata.GetProperty("id").GetInt32(), ["upgrade"] = upgrade,
            ["dampen_original_upgrade"] = 0, ["owner"] = card.Owner is null ? 0 : 1,
            ["pile"] = (int)(card.Pile?.Type ?? PileType.None),
            ["in_combat"] = player.Creature.CombatState!.ContainsCard(card),
            ["in_run"] = player.RunState.ContainsCard(card), ["removed"] = card.HasBeenRemovedFromState,
            ["is_dupe"] = card.IsDupe, ["exhaust_on_next_play"] = card.ExhaustOnNextPlay,
            ["single_turn_retain"] = card._hasSingleTurnRetain, ["single_turn_sly"] = card._hasSingleTurnSly,
            ["star_cost_set"] = card._starCostSet, ["was_star_cost_just_upgraded"] = card.WasStarCostJustUpgraded,
            ["cost_was_just_upgraded"] = cost.WasJustUpgraded, ["is_enchantment_preview"] = card.IsEnchantmentPreview,
            ["enchantment_status"] = 0, ["floor_added"] = card.FloorAddedToDeck ?? -1,
            ["cost_base"] = cost._base, ["captured_x"] = cost._capturedXValue,
            ["cost_modifiers"] = new JsonArray(cost._localModifiers.Select(m => (JsonNode)new JsonObject
            {
                ["amount"] = m.Amount, ["type"] = (int)m.Type, ["reduce_only"] = m.IsReduceOnly, ["expiration"] = (int)m.Expiration,
            }).ToArray()),
            // Env alloc_card stores the effective canonical star cost eagerly.
            // Reading the game's BaseStarCost getter would mutate its lazy flag.
            ["base_star_cost"] = card._starCostSet ? card._baseStarCost : card.CanonicalStarCost,
            ["last_stars_spent"] = card.LastStarsSpent,
            ["temporary_star_costs"] = new JsonArray(card._temporaryStarCosts.Select(m => (JsonNode)new JsonObject
            {
                ["cost"] = m.Cost, ["clears_when_turn_ends"] = m.ClearsWhenTurnEnds, ["clears_when_card_is_played"] = m.ClearsWhenCardIsPlayed,
            }).ToArray()),
            ["base_replay_count"] = card.BaseReplayCount, ["current_play_index"] = card.CurrentPlayIndex,
            ["last_played_turn"] = lastPlayedTurns.TryGetValue(card, out int turn) ? turn : -1,
            ["calamity_pending_amount"] = -1, ["keywords"] = keywords, ["var_overrides"] = overrides,
            ["current_target"] = target, ["enchantment"] = 0, ["affliction"] = 0,
            ["enchantment_amount"] = 0, ["affliction_amount"] = 0, ["enchantment_var"] = Decimal(0),
            ["enchantment_extra"] = new JsonArray(0, 0),
            ["growth_damage"] = Decimal(card switch {
                Rampage rampage => rampage._extraDamageFromPlays,
                Thrash thrash => thrash._extraDamage,
                _ => 0m,
            }), ["extra"] = new JsonArray(0, 0, 0, 0),
        };
    }

    private static JsonObject Decimal(decimal value)
        => new() { ["decimal_bits"] = new JsonArray(decimal.GetBits(value).Select(bits => JsonValue.Create(unchecked((uint)bits))).ToArray()) };

    internal static void RequireVariableMetadata(DynamicVar variable, JsonElement definition, string card, CardModel? owner = null)
    {
        Type type = variable.GetType();
        string actualType = type.IsGenericType
            ? type.Name.Split('`')[0] + "<" + string.Join(",", type.GetGenericArguments().Select(argument => argument.Name)) + ">"
            : type.Name;
        string declaredType = definition.GetProperty("type").GetString()!;
        // Scalar variables have no future calculation state. Calculated vars
        // below require the exact audited vanilla owner and delegate binding.
        bool supported = type.Assembly == typeof(DynamicVar).Assembly && actualType == declaredType
            && (actualType is "DynamicVar" or "DamageVar" or "BlockVar" or "CardsVar" or "EnergyVar"
                or "HealVar" or "HpLossVar" or "IntVar" or "MaxHpVar" or "RepeatVar" or "StarsVar" or "GoldVar"
                or "CalculationBaseVar" or "CalculationExtraVar" or "ExtraDamageVar"
                or "CalculatedVar" or "CalculatedDamageVar" or "CalculatedBlockVar"
                || actualType.StartsWith("PowerVar<", StringComparison.Ordinal));
        if (!supported) throw new InvalidOperationException($"live_card_variable_type_unmapped:{card}:{variable.Name}:{actualType}");
        if (variable is CalculatedVar calculated)
        {
            string? expectedName = card switch {
                "BODY_SLAM" or "PERFECTED_STRIKE" or "ASHEN_STRIKE" or "MIND_BLAST" or "BULLY" or "REND" or "GOLD_AXE" or "UNLEASH" => "CalculatedDamage",
                "DEMONIC_SHIELD" or "EXPECT_A_FIGHT" => "CalculatedBlock",
                "BARRAGE" or "TEAR_ASUNDER" => "CalculatedHits",
                "COMPILE_DRIVER" => "CalculatedCards",
                _ => null,
            };
            if (owner is null || expectedName != variable.Name || !ReferenceEquals(variable._owner, owner))
                throw new InvalidOperationException($"live_card_calculated_formula_unmapped:{card}:{variable.Name}");
            CalculatedVar canonical = (CalculatedVar)ModelDb.GetById<CardModel>(owner.Id).DynamicVars[variable.Name];
            if (calculated._multiplierCalc is not { } actualBinding || canonical._multiplierCalc is not { } expectedBinding
                || actualBinding.Method != expectedBinding.Method || !ReferenceEquals(actualBinding.Target, expectedBinding.Target))
                throw new InvalidOperationException($"live_card_calculated_binding_mismatch:{card}:{variable.Name}");
        }
        ValueProp? props = variable switch { DamageVar damage => damage.Props, BlockVar block => block.Props,
            CalculatedDamageVar damage => damage.Props, CalculatedBlockVar block => block.Props, _ => null };
        if (props is { } actual && (!definition.TryGetProperty("props", out JsonElement properties)
            || !properties.TryGetProperty("Props", out JsonElement flags)
            || actual != Enum.Parse<ValueProp>(flags.GetString()!)))
            throw new InvalidOperationException($"live_card_variable_props_unmapped:{card}:{variable.Name}");
        bool? fromOsty = variable switch { CalculatedDamageVar damage => damage.IsFromOsty,
            ExtraDamageVar damage => damage.IsFromOsty, _ => null };
        if (fromOsty is { } osty && (!definition.TryGetProperty("props", out JsonElement fields)
            || !fields.TryGetProperty("IsFromOsty", out JsonElement flag) || flag.GetBoolean() != osty))
            throw new InvalidOperationException($"live_card_variable_osty_unmapped:{card}:{variable.Name}");
    }
}
