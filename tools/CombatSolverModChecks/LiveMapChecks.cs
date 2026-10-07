using System.Text.Json;
using CombatSolver;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Map;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Models.Relics;
using MegaCrit.Sts2.Core.Random;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Saves.Runs;

// Expected graph facts come from the real map objects and their public grid
// enumeration, independently of the bridge's reflection and native point IDs.
internal static class LiveMapChecks
{
    internal static void Run(RunState run, CombatState combat, Player player)
    {
        ActMap original = run.Map;
        int actIndex = run._currentActIndex;
        try
        {
            Check("saved-standard", original);
            ActMap standard = StandardActMap.CreateFor(run, false);
            Check("standard", standard);
            ActMap golden = new GoldenPathActMap(run);
            Check("golden", golden);
            Check("saved-golden", new SavedActMap(SerializableActMap.FromActMap(golden)));
            ActMap doubleBoss = new StandardActMap(new Rng(run.Rng.Seed, "act_1_map"), run.Act,
                false, false, true);
            Check("double-boss", doubleBoss);
            Check("saved-double-boss", new SavedActMap(SerializableActMap.FromActMap(doubleBoss)));
            // This fixture explicitly supplies an Act 2 map; keep the other
            // original run fields unchanged rather than invoking act travel.
            run._currentActIndex = 1;
            ActMap spoils = new SpoilsActMap(run);
            Check("spoils-act2", spoils);
            Check("saved-spoils-act2", new SavedActMap(SerializableActMap.FromActMap(spoils)));
        }
        finally { run.Map = original; run._currentActIndex = actIndex; }

        void Check(string name, ActMap map)
        {
            run.Map = map;
            using JsonDocument snapshot = JsonDocument.Parse(EnvLiveCapture.Capture(combat, player));
            Verify(snapshot.RootElement.GetProperty("state").GetProperty("run").GetProperty("map"), map);
            Console.WriteLine($"MAP_PRESERVED {name} type={map.GetType().Name} points={map.GetAllMapPoints().Count()}");
        }
    }

    internal static void Verify(JsonElement native, ActMap map)
    {
        JsonElement[] points = native.GetProperty("points").EnumerateArray().ToArray();
        string Key(MapCoord coord) => $"{coord.col},{coord.row}";
        string NativeKey(JsonElement point) => point.GetProperty("coord").GetProperty("col").GetInt32()
            + "," + point.GetProperty("coord").GetProperty("row").GetInt32();
        string Reference(int index) => NativeKey(points[index]);
        IEnumerable<string> Set(JsonElement set) => set.GetProperty("slots").EnumerateArray()
            .Where(slot => slot.GetProperty("used").GetBoolean()).Select(slot => Reference(slot.GetProperty("value").GetInt32()));
        void Require(bool condition) { if (!condition) throw new Exception("map graph differs: " + map.GetType().Name); }
        var expected = map.GetAllMapPoints().Append(map.StartingMapPoint).Append(map.BossMapPoint)
            .Concat(map.SecondBossMapPoint is { } second ? new[] { second } : Array.Empty<MapPoint>())
            .Distinct<MapPoint>(ReferenceEqualityComparer.Instance).ToDictionary(point => Key(point.coord));
        Require(points.Length == expected.Count && native.GetProperty("cols").GetInt32() == map.GetColumnCount()
            && native.GetProperty("rows").GetInt32() == map.GetRowCount());
        foreach (JsonElement point in points)
        {
            MapPoint source = expected[NativeKey(point)];
            Require(point.GetProperty("type").GetInt32() == (int)source.PointType
                && point.GetProperty("can_be_modified").GetBoolean() == source.CanBeModified
                && point.GetProperty("fur_coat_quest").GetBoolean() == source.Quests.Any(q => q is FurCoat)
                && point.GetProperty("spoils_map_quest").GetBoolean() == source.Quests.Any(q => q is SpoilsMap)
                && Set(point.GetProperty("children")).SequenceEqual(source.Children.Select(p => Key(p.coord)))
                && Set(point.GetProperty("parents")).SequenceEqual(source.parents.Select(p => Key(p.coord))));
        }
        var gridPoints = map.GetAllMapPoints().ToDictionary(point => Key(point.coord));
        JsonElement[] columns = native.GetProperty("grid").EnumerateArray().ToArray();
        for (int col = 0; col < columns.Length; col++)
        {
            int row = 0;
            foreach (JsonElement value in columns[col].EnumerateArray())
            {
                int index = value.GetInt32();
                Require(gridPoints.ContainsKey($"{col},{row}") ? index >= 0 && Reference(index) == $"{col},{row}" : index == -1);
                row++;
            }
        }
        Require(Reference(native.GetProperty("start").GetInt32()) == Key(map.StartingMapPoint.coord)
            && Reference(native.GetProperty("boss").GetInt32()) == Key(map.BossMapPoint.coord)
            && Set(native.GetProperty("start_points")).SequenceEqual(map.startMapPoints.Select(p => Key(p.coord))));
        int secondRef = native.GetProperty("second_boss").GetInt32();
        Require(map.SecondBossMapPoint is { } boss ? secondRef >= 0 && Reference(secondRef) == Key(boss.coord) : secondRef == -1);
    }
}
