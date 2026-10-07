using System.Reflection;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using CombatSolverEnv.Api;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Entities.Rngs;
using MegaCrit.Sts2.Core.Map;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Random;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Timeline;
using MegaCrit.Sts2.Core.Unlocks;

namespace CombatSolver;

// Run/player fields without instance pools. This component is combined with
// cards, creatures, relics, potions and history by the future complete mapper.
// Its owned JSON is never a seeded approximation of live values.
internal sealed record EnvLiveRunContext(JsonElement Component)
{
    internal static EnvLiveRunContext Capture(CombatState state, Player player)
    {
        EnvModelCatalog.RequireGameVersion();
        if (state.RunState is not RunState run || run.Players.Count != 1 || !ReferenceEquals(run.Players[0], player)
            || !ReferenceEquals(player.RunState, run)) throw new InvalidOperationException("live_run_requires_single_player_RunState");
        if (run.GameMode != GameMode.Standard || run.Modifiers.Count != 0)
            throw new InvalidOperationException("live_run_mode_or_modifiers_unmapped");
        JsonObject runProfile = CaptureProfile(run.UnlockState);
        JsonObject playerProfile = CaptureProfile(player.UnlockState);
        bool profileCompatible = runProfile["full_veteran_compatible"]!.GetValue<bool>()
            && playerProfile["full_veteran_compatible"]!.GetValue<bool>();
        if (run.Rng.GetType() != typeof(RunRngSet) || player.PlayerRng.GetType() != typeof(PlayerRngSet)
            || !Enum.GetNames<RunRngType>().SequenceEqual(Enum.GetNames<EnvRunRngStream>())
            || !Enum.GetNames<PlayerRngType>().SequenceEqual(new[] { "Rewards", "Shops", "Transformations" }))
            throw new InvalidOperationException("live_rng_stream_schema_unmapped");
        if (run.Rng._rngs.Count != 12 || player.PlayerRng._rngs.Count != 3)
            throw new InvalidOperationException("live_rng_stream_count_unmapped");
        HashSet<Rng> rngs = new(ReferenceEqualityComparer.Instance);
        HashSet<MegaRandom> generators = new(ReferenceEqualityComparer.Instance);
        JsonObject Freeze(Rng source)
        {
            if (source.GetType() != typeof(Rng) || source._random.GetType() != typeof(MegaRandom)
                || !rngs.Add(source) || !generators.Add(source._random))
                throw new InvalidOperationException("live_rng_type_or_alias_unmapped");
            return RngValue(source);
        }
        JsonArray runStreams = new(Enum.GetValues<RunRngType>().Select(stream => (JsonNode)Freeze(run.Rng.GetRng(stream))).ToArray());
        JsonArray playerStreams = new(Enum.GetValues<PlayerRngType>().Select(stream => (JsonNode)Freeze(player.PlayerRng.GetRng(stream))).ToArray());
        // Aliases across run/player/monster/encounter generators cannot be
        // represented by independent POD streams. Check before dropping refs.
        foreach (var creature in state.Creatures.Concat(state.EscapedCreatures).Distinct< MegaCrit.Sts2.Core.Entities.Creatures.Creature>(ReferenceEqualityComparer.Instance))
            if (creature.Monster is { } monster) _ = Freeze(monster._rng ?? throw new InvalidOperationException("live_monster_rng_missing"));
        if (state.Encounter?._rng is { } encounterRng) _ = Freeze(encounterRng);
        var odds = run.Odds.UnknownMapPoint;
        if (!ReferenceEquals(odds._rng, run.Rng.UnknownMapPoint)
            || !ReferenceEquals(player.PlayerOdds.CardRarity._rng, player.PlayerRng.Rewards)
            || !ReferenceEquals(player.PlayerOdds.PotionReward._rng, player.PlayerRng.Rewards))
            throw new InvalidOperationException("live_odds_rng_binding_unmapped");
        var baseOdds = new Dictionary<RoomType, float> { [RoomType.Monster] = 0.1f, [RoomType.Elite] = -1f, [RoomType.Treasure] = 0.02f, [RoomType.Shop] = 0.03f };
        if (odds._baseOdds.Count != baseOdds.Count || odds._nonEventOdds.Count != baseOdds.Count
            || !odds._baseOdds.Keys.SequenceEqual(baseOdds.Keys) || !odds._nonEventOdds.Keys.SequenceEqual(baseOdds.Keys)
            || baseOdds.Any(pair => !odds._baseOdds.TryGetValue(pair.Key, out float value) || BitConverter.SingleToUInt32Bits(value) != BitConverter.SingleToUInt32Bits(pair.Value))
            || baseOdds.Keys.Any(key => !odds._nonEventOdds.ContainsKey(key)))
            throw new InvalidOperationException("live_unknown_room_odds_definition_unmapped");
        byte[] seed = Encoding.UTF8.GetBytes(run.Rng.StringSeed);
        if (seed.Length >= 24 || seed.Contains((byte)0)) throw new InvalidOperationException("live_seed_capacity_or_null");
        JsonArray seedBytes = new(Enumerable.Range(0, 24).Select(i => JsonValue.Create(i < seed.Length ? seed[i] : (byte)0)).ToArray());
        JsonArray visited = Limited(run.VisitedMapCoords, 24, coord => Coord(coord));
        JsonArray rooms = Limited(run._currentRooms, 4, room =>
        {
            if (room.GetType().Assembly != typeof(AbstractRoom).Assembly || !Enum.IsDefined(room.RoomType))
                throw new InvalidOperationException("live_room_type_unmapped");
            return JsonValue.Create((int)room.RoomType)!;
        });
        int entryRooms = 0;
        foreach (var room in run.CurrentMapPointHistoryEntry?.Rooms ?? [])
        {
            if (!Enum.IsDefined(room.RoomType)) throw new InvalidOperationException("live_room_history_type_unmapped");
            entryRooms |= 1 << (int)room.RoomType;
        }
        if (run.ActFloor is < short.MinValue or > short.MaxValue || run.TotalFloor > short.MaxValue
            || run.CurrentActIndex < 0 || run.CurrentActIndex >= run.Acts.Count || run.Acts.Count > 4)
            throw new InvalidOperationException("live_run_floor_or_act_capacity");
        if (player.MaxPotionCount > 8) throw new InvalidOperationException("live_potion_slot_capacity");
        JsonObject runFields = new()
        {
            ["profile"] = runProfile["state"]!.DeepClone(),
            ["seed"] = seedBytes, ["run_seed"] = run.Rng.Seed, ["ascension"] = run.AscensionLevel,
            ["rng"] = new JsonObject { ["seed"] = run.Rng.Seed, ["rngs"] = runStreams },
            ["odds"] = new JsonObject { ["monster"] = Float(odds.MonsterOdds), ["elite"] = Float(odds.EliteOdds), ["treasure"] = Float(odds.TreasureOdds), ["shop"] = Float(odds.ShopOdds) },
            ["acts"] = Limited(run.Acts, 4, CaptureAct), ["current_act_index"] = run.CurrentActIndex,
            ["map"] = CaptureMap(run.Map), ["visited_map_coords"] = visited,
            ["act_floor"] = run.ActFloor, ["total_floor"] = run.TotalFloor,
            ["current_entry_rooms"] = entryRooms, ["rooms"] = rooms,
            ["shared_relic_grab_bag"] = CaptureBag(run.SharedRelicGrabBag),
            ["visited_event_ids"] = Limited(run._visitedEventIds, 96, id => JsonValue.Create(EnvModelCatalog.Id(ModelDb.GetById<EventModel>(id), "events"))!),
            ["started_with_neow"] = run.ExtraFields.StartedWithNeow, ["test_subject_kills"] = run.ExtraFields.TestSubjectKills,
        };
        // Visited IDs are append-only in the game. Env uses a vector; a removed
        // CLR slot would make future insertion order different.
        if ((int)typeof(HashSet<ModelId>).GetField("_freeCount", BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(run._visitedEventIds)! != 0)
            throw new InvalidOperationException("live_visited_event_removed_slots_unmapped");
        JsonObject playerFields = new()
        {
            ["profile"] = playerProfile["state"]!.DeepClone(),
            ["character"] = EnvModelCatalog.Id(player.Character, "characters"), ["net_id"] = player.NetId,
            ["max_energy"] = player.MaxEnergy, ["max_potion_slots"] = player.MaxPotionCount,
            ["base_orb_slots"] = player.BaseOrbSlotCount, ["gold"] = player.Gold,
            ["rng"] = new JsonObject { ["seed"] = player.PlayerRng.Seed, ["rngs"] = playerStreams },
            ["odds"] = new JsonObject { ["card_rarity"] = Float(player.PlayerOdds.CardRarity.CurrentValue), ["potion_reward"] = Float(player.PlayerOdds.PotionReward.CurrentValue) },
            ["relic_grab_bag"] = CaptureBag(player.RelicGrabBag), ["active_for_hooks"] = player.IsActiveForHooks,
            ["can_use_or_remove_potions"] = player.CanUseOrRemovePotions,
            ["card_shop_removals_used"] = player.ExtraFields.CardShopRemovalsUsed, ["damage_dealt"] = player.ExtraFields.DamageDealt,
        };
        return new(JsonSerializer.SerializeToElement(new JsonObject { ["run"] = runFields, ["player"] = playerFields,
            ["profile"] = new JsonObject { ["all_unlocked"] = profileCompatible },
            // Keep both actual sources with their diagnostic IDs alongside
            // the compact POD state; the summary cannot replace either one.
            ["profile_source"] = new JsonObject { ["run"] = runProfile, ["player"] = playerProfile } }));
    }

    private static JsonObject CaptureProfile(UnlockState unlocks)
    {
        if (unlocks.GetType() != typeof(UnlockState) || unlocks.NumberOfRuns < 0)
            throw new InvalidOperationException("live_unlock_state_type_or_count_unmapped");
        IReadOnlyList<string> allEpochs = EpochModel.AllEpochIds;
        if (allEpochs.Count != 57 || allEpochs.Distinct(StringComparer.Ordinal).Count() != allEpochs.Count)
            throw new InvalidOperationException("live_profile_epoch_catalog_changed");
        ulong epochBits = 0;
        foreach (string epoch in unlocks._unlockedEpochIds)
        {
            int index = -1;
            for (int i = 0; i < allEpochs.Count; i++)
                if (StringComparer.Ordinal.Equals(allEpochs[i], epoch)) { index = i; break; }
            if (index < 0) throw new InvalidOperationException("live_profile_epoch_unmapped:" + epoch);
            epochBits |= 1UL << index;
        }
        ulong seenLow = 0, seenHigh = 0;
        foreach (ModelId encounter in unlocks._encountersSeen)
        {
            JsonElement catalog = EnvModelCatalog.Content.GetProperty("encounters");
            if (!catalog.TryGetProperty(encounter.Entry, out JsonElement model)
                || model.GetProperty("type").GetString() != ModelDb.GetById<EncounterModel>(encounter).GetType().FullName)
                throw new InvalidOperationException("live_profile_encounter_unmapped:" + encounter.Entry);
            int id = model.GetProperty("id").GetInt32();
            if (id is <= 0 or > 90) throw new InvalidOperationException("live_profile_encounter_id_capacity");
            if (id < 64) seenLow |= 1UL << id;
            else seenHigh |= 1UL << (id - 64);
        }
        string[] missingEpochs = UnlockState.all._unlockedEpochIds.Except(unlocks._unlockedEpochIds).ToArray();
        ModelId[] missingEncounters = UnlockState.all._encountersSeen.Except(unlocks._encountersSeen).ToArray();
        bool fullVeteran = unlocks.NumberOfRuns != 0 && missingEpochs.Length == 0 && missingEncounters.Length == 0;
        JsonArray Epochs(IEnumerable<string> values) => Limited(values, 256, value => JsonValue.Create(value)!);
        JsonArray Encounters(IEnumerable<ModelId> values) => Limited(values, 256, value => JsonValue.Create(value.ToString())!);
        return new()
        {
            ["number_of_runs"] = unlocks.NumberOfRuns,
            ["unlocked_epochs"] = Epochs(unlocks._unlockedEpochIds), ["encounters_seen"] = Encounters(unlocks._encountersSeen),
            ["missing_epochs"] = Epochs(missingEpochs), ["missing_seen_encounters"] = Encounters(missingEncounters),
            ["full_veteran_compatible"] = fullVeteran,
            ["state"] = new JsonObject { ["number_of_runs"] = unlocks.NumberOfRuns,
                ["unlocked_epoch_bits"] = epochBits, ["encounters_seen_low"] = seenLow,
                ["encounters_seen_high"] = seenHigh, ["full_veteran_compatible"] = fullVeteran },
        };
    }

    internal static JsonObject RngValue(Rng rng)
    {
        if (rng._counter < 0) throw new InvalidOperationException("live_rng_negative_counter");
        var random = rng._random;
        return new() { ["counter"] = rng._counter, ["s0"] = random._s0, ["s1"] = random._s1, ["s2"] = random._s2, ["s3"] = random._s3 };
    }
    private static JsonObject Float(float value)
    {
        if (!float.IsFinite(value)) throw new InvalidOperationException("live_odds_nonfinite");
        return new() { ["f32_bits"] = BitConverter.SingleToUInt32Bits(value) };
    }
    private static JsonObject Coord(MapCoord coord)
    {
        if (coord.col is < sbyte.MinValue or > sbyte.MaxValue || coord.row is < sbyte.MinValue or > sbyte.MaxValue)
            throw new InvalidOperationException("live_coordinate_capacity");
        return new() { ["col"] = coord.col, ["row"] = coord.row };
    }
    private static JsonArray Limited<T>(IEnumerable<T> values, int capacity, Func<T, JsonNode> freeze)
    {
        JsonArray result = [];
        foreach (T value in values)
        {
            if (result.Count >= capacity) throw new InvalidOperationException("live_run_collection_capacity");
            result.Add(freeze(value));
        }
        return result;
    }
    private static JsonObject CaptureAct(ActModel act)
    {
        var rooms = act._rooms;
        return new()
        {
            ["id"] = EnvModelCatalog.Id(act, "acts"),
            ["rooms"] = new JsonObject
            {
                ["events"] = Limited(rooms.events, 40, model => JsonValue.Create(EnvModelCatalog.Id(model, "events"))!), ["events_visited"] = rooms.eventsVisited,
                ["normal_encounters"] = Limited(rooms.normalEncounters, 16, model => JsonValue.Create(EnvModelCatalog.Id(model, "encounters"))!), ["normal_encounters_visited"] = rooms.normalEncountersVisited,
                ["elite_encounters"] = Limited(rooms.eliteEncounters, 16, model => JsonValue.Create(EnvModelCatalog.Id(model, "encounters"))!), ["elite_encounters_visited"] = rooms.eliteEncountersVisited,
                ["boss_encounters_visited"] = rooms.bossEncountersVisited,
                ["boss"] = EnvModelCatalog.Id(rooms._boss, "encounters"), ["second_boss"] = EnvModelCatalog.Id(rooms.SecondBoss, "encounters"),
                ["ancient"] = EnvModelCatalog.Id(rooms._ancient, "events"),
            },
            ["has_shared_ancient_subset"] = act._sharedAncientSubset is not null,
            ["shared_ancient_subset"] = Limited(act._sharedAncientSubset ?? [], 4, model => JsonValue.Create(EnvModelCatalog.Id(model, "events"))!),
        };
    }
    private static JsonObject CaptureBag(RelicGrabBag bag)
    {
        int Id(RelicModel model)
        {
            if (model.IsMutable) throw new InvalidOperationException("live_relic_bag_mutable_model_unmapped");
            return EnvModelCatalog.Id(model, "relics");
        }
        return new()
        {
            ["deques"] = Limited(bag._deques, 8, pair => new JsonObject { ["rarity"] = (int)pair.Key, ["relics"] = Limited(pair.Value, 160, model => JsonValue.Create(Id(model))!) }),
            ["mp_fallback"] = Limited(bag._mpFallbackDequeue, 32, model => JsonValue.Create(Id(model))!),
            ["original"] = Limited(bag._originalRelics ?? [], 320, model => JsonValue.Create(Id(model))!),
            ["refresh_allowed"] = bag._refreshAllowed,
        };
    }
    private static JsonObject CaptureMap(ActMap map)
    {
        if (map.GetType() != typeof(StandardActMap) && map.GetType() != typeof(GoldenPathActMap))
            throw new InvalidOperationException("live_map_type_unmapped");
        var field = map.GetType().GetField("<Grid>k__BackingField", BindingFlags.Instance | BindingFlags.NonPublic)
            ?? throw new InvalidOperationException("live_map_grid_layout_unmapped");
        var grid = (MapPoint?[,])field.GetValue(map)!;
        int cols = grid.GetLength(0), rows = grid.GetLength(1);
        if (cols != 7 || rows > 18) throw new InvalidOperationException("live_map_dimensions_capacity");
        List<MapPoint> points = [];
        Dictionary<MapPoint, int> references = new(ReferenceEqualityComparer.Instance);
        void Add(MapPoint? point)
        {
            if (point is null || references.ContainsKey(point)) return;
            if (point.GetType() != typeof(MapPoint) || point.Quests.Count != 0 || points.Count >= 129)
                throw new InvalidOperationException("live_map_point_type_quests_or_capacity");
            references.Add(point, points.Count);
            points.Add(point);
        }
        Add(map.BossMapPoint); Add(map.StartingMapPoint); Add(map.SecondBossMapPoint);
        for (int col = 0; col < cols; col++)
            for (int row = 0; row < rows; row++) Add(grid[col, row]);
        foreach (MapPoint point in map.startMapPoints) Add(point);
        for (int i = 0; i < points.Count; i++)
            foreach (MapPoint neighbor in points[i].Children.Concat(points[i].parents)) Add(neighbor);
        int Ref(MapPoint? point) => point is null ? -1 : references.TryGetValue(point, out int index) ? index
            : throw new InvalidOperationException("live_map_reference_missing");
        JsonArray gridColumns = [];
        for (int col = 0; col < 7; col++)
            gridColumns.Add(new JsonArray(Enumerable.Range(0, 18).Select(row =>
            {
                int reference = row < rows ? Ref(grid[col, row]) : -1;
                if (reference > sbyte.MaxValue) throw new InvalidOperationException("live_map_grid_reference_capacity");
                return JsonValue.Create(reference);
            }).ToArray()));
        return new()
        {
            ["cols"] = cols, ["rows"] = rows,
            ["points"] = new JsonArray(points.Select(point => (JsonNode)new JsonObject
            {
                ["coord"] = Coord(point.coord), ["type"] = (int)point.PointType, ["can_be_modified"] = point.CanBeModified,
                ["children"] = EnvLiveCollections.Set(point.Children, 8, Ref), ["parents"] = EnvLiveCollections.Set(point.parents, 8, Ref),
            }).ToArray()),
            ["grid"] = gridColumns, ["start"] = Ref(map.StartingMapPoint), ["boss"] = Ref(map.BossMapPoint),
            ["second_boss"] = Ref(map.SecondBossMapPoint), ["start_points"] = EnvLiveCollections.Set(map.startMapPoints, 8, Ref),
        };
    }
}
