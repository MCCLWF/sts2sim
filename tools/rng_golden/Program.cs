// Writes golden vectors for sts2sim/core into <outDir>:
//   hash.txt     XxHash64 / legacy hash / snake-case stream names / seed derivation
//   rng.txt      Rng call sequences (every public Next* method, shuffles, weighted picks)
//   sort.txt     List<T>.Sort() tie placement (introsort)
//   decimal.txt  System.Decimal arithmetic, rounding and conversions
//
// Usage: dotnet run -c Release -- <outDir> [caseMultiplier]
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Entities.Rngs;
using MegaCrit.Sts2.Core.Extensions;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Random;

internal static class Program
{
    private static int Main(string[] args)
    {
        string outDir = args.Length > 0 ? args[0] : "golden";
        int mult = args.Length > 1 ? int.Parse(args[1], CultureInfo.InvariantCulture) : 1;
        Directory.CreateDirectory(outDir);
        HashGolden.Write(Path.Combine(outDir, "hash.txt"));
        RngGolden.Write(Path.Combine(outDir, "rng.txt"), mult);
        SortGolden.Write(Path.Combine(outDir, "sort.txt"), mult);
        DecimalGolden.Write(Path.Combine(outDir, "decimal.txt"), mult);
        Console.WriteLine($"golden vectors written to {outDir}");
        return 0;
    }
}

internal static class Fmt
{
    public static string Hex(string s) => Convert.ToHexString(Encoding.UTF8.GetBytes(s));
    public static string F(float f) => BitConverter.SingleToUInt32Bits(f).ToString("x8");
    public static string D(double d) => BitConverter.DoubleToUInt64Bits(d).ToString("x16");
    public static string Dec(decimal d)
    {
        int[] b = decimal.GetBits(d);
        return $"{(uint)b[0]:x8}:{(uint)b[1]:x8}:{(uint)b[2]:x8}:{(uint)b[3]:x8}";
    }
}

internal static class HashGolden
{
    public static void Write(string path)
    {
        using var w = new StreamWriter(path);
        var sys = new System.Random(1234);
        var strings = new List<string> { "", "a", "up_front", "STRIKE_IRONCLAD", "act_1_map", "spoils_map", "Ünïcödé ✓ 杀戮尖塔" };
        for (int len = 0; len <= 80; len++)
        {
            var sb = new StringBuilder();
            for (int i = 0; i < len; i++) sb.Append((char)sys.Next(32, 127));
            strings.Add(sb.ToString());
        }
        foreach (string s in strings)
        {
            w.WriteLine($"hash {Fmt.Hex(s)} {StringHelper.GetDeterministicHashCode(s)}");
            w.WriteLine($"oldhash {Fmt.Hex(s)} {StringHelper.GetDeterministicHashCodeOld(s)}");
        }

        foreach (RunRngType t in Enum.GetValues<RunRngType>())
            w.WriteLine($"runname {(int)t} {StringHelper.SnakeCase(t.ToString())}");
        foreach (PlayerRngType t in Enum.GetValues<PlayerRngType>())
            w.WriteLine($"playername {(int)t} {StringHelper.SnakeCase(t.ToString())}");

        const string alphabet = "0123456789ABCDEFGHJKLMNPQRSTUVWXYZ";
        var seeds = new List<string> { "0", "ABC", " abcio ", "old12345", "OLDSEED" };
        for (int i = 0; i < 40; i++)
        {
            var sb = new StringBuilder();
            for (int j = 0; j < 12; j++) sb.Append(alphabet[sys.Next(alphabet.Length)]);
            seeds.Add(sb.ToString());
        }
        foreach (string raw in seeds)
        {
            string canon = SeedHelper.CanonicalizeSeed(raw);
            w.WriteLine($"canon {Fmt.Hex(raw)} {Fmt.Hex(canon)}");
            // RunRngSet(string seed) constructor logic.
            ulong runSeed = canon.StartsWith("old")
                ? (uint)StringHelper.GetDeterministicHashCodeOld(canon.Substring(3))
                : StringHelper.GetDeterministicHashCode(canon);
            w.WriteLine($"runseed {Fmt.Hex(canon)} {runSeed}");
            foreach (RunRngType t in Enum.GetValues<RunRngType>())
            {
                var s = new Rng(runSeed, StringHelper.SnakeCase(t.ToString())).ToSerializable();
                w.WriteLine($"stream {runSeed} {(int)t} {s.state0} {s.state1} {s.state2} {s.state3}");
            }
            for (int slot = 0; slot < 2; slot++)
            {
                ulong playerSeed = StringHelper.GetDeterministicHashCode(canon) + (ulong)slot;
                foreach (PlayerRngType t in Enum.GetValues<PlayerRngType>())
                {
                    var s = new Rng(playerSeed, StringHelper.SnakeCase(t.ToString())).ToSerializable();
                    w.WriteLine($"pstream {Fmt.Hex(canon)} {slot} {(int)t} {s.state0} {s.state1} {s.state2} {s.state3}");
                }
            }
        }

        // Rng(Player, ModelId, mixin) content constructor.
        for (int i = 0; i < 30; i++)
        {
            var player = new Player();
            player.RunState.Rng.Seed = (ulong)sys.NextInt64() * 3;
            player.RunState.Slot = sys.Next(0, 4);
            string entry = "ENTRY_" + sys.Next(0, 1000);
            ulong mixin = (ulong)sys.Next(0, 5);
            var s = new Rng(player, new ModelId("RELIC", entry), mixin).ToSerializable();
            w.WriteLine($"content {player.RunState.Rng.Seed} {player.RunState.Slot} {Fmt.Hex(entry)} {mixin} {s.state0} {s.state1} {s.state2} {s.state3}");
        }
    }
}

internal sealed class Item : IComparable<Item>
{
    public int Key;
    public int Id;
    public int CompareTo(Item? other) => other == null ? 1 : Key.CompareTo(other.Key);
}

internal static class RngGolden
{
    public static void Write(string path, int mult)
    {
        using var w = new StreamWriter(path);
        var sys = new System.Random(99);
        int sequences = 60 * mult;
        for (int seq = 0; seq < sequences; seq++)
        {
            ulong seed = seq < 3 ? (ulong)seq : (ulong)sys.NextInt64() ^ ((ulong)sys.NextInt64() << 1);
            w.WriteLine($"seed {seed}");
            var rng = new Rng(seed);
            for (int op = 0; op < 200; op++) EmitOp(w, rng, sys);
            var st = rng.ToSerializable();
            w.WriteLine($"state {st.state0} {st.state1} {st.state2} {st.state3} {st.counter}");
        }
        // Named constructor Rng(ulong, string).
        for (int i = 0; i < 20; i++)
        {
            ulong seed = (ulong)sys.NextInt64();
            string name = "name_" + i;
            var st = new Rng(seed, name).ToSerializable();
            w.WriteLine($"named {seed} {Fmt.Hex(name)} {st.state0} {st.state1} {st.state2} {st.state3}");
        }
    }

    private static float RandFloat(System.Random sys, float lo, float hi) => (float)(lo + sys.NextDouble() * (hi - lo));

    private static void EmitOp(StreamWriter w, Rng rng, System.Random sys)
    {
        switch (sys.Next(22))
        {
            case 0:
                w.WriteLine($"ulong = {rng.NextUnsignedLong()}");
                break;
            case 1:
            {
                int max = sys.Next(4) == 0 ? int.MaxValue - sys.Next(1000) : sys.Next(1, 100);
                w.WriteLine($"int1 {max} = {rng.NextInt(max)}");
                break;
            }
            case 2:
            {
                int min = sys.Next(4) == 0 ? int.MinValue + sys.Next(1000) : sys.Next(-50, 50);
                int max = sys.Next(4) == 0 ? int.MaxValue - sys.Next(1000) : min + sys.Next(1, 100);
                w.WriteLine($"int2 {min} {max} = {rng.NextInt(min, max)}");
                break;
            }
            case 3:
                w.WriteLine($"bool = {(rng.NextBool() ? 1 : 0)}");
                break;
            case 4:
                w.WriteLine($"float0 = {Fmt.F(rng.NextFloat())}");
                break;
            case 5:
            {
                float max = RandFloat(sys, 0f, 100f);
                w.WriteLine($"float1 {Fmt.F(max)} = {Fmt.F(rng.NextFloat(max))}");
                break;
            }
            case 6:
            {
                float min = RandFloat(sys, -10f, 10f);
                float max = min + RandFloat(sys, 0f, 20f);
                w.WriteLine($"float2 {Fmt.F(min)} {Fmt.F(max)} = {Fmt.F(rng.NextFloat(min, max))}");
                break;
            }
            case 7:
                w.WriteLine($"double0 = {Fmt.D(rng.NextDouble())}");
                break;
            case 8:
            {
                double min = sys.NextDouble() * 20 - 10;
                double max = min + sys.NextDouble() * 20;
                w.WriteLine($"double2 {Fmt.D(min)} {Fmt.D(max)} = {Fmt.D(rng.NextDouble(min, max))}");
                break;
            }
            case 9:
            {
                uint max = sys.Next(3) == 0 ? uint.MaxValue - (uint)sys.Next(1000) : (uint)sys.Next(1, 1000);
                w.WriteLine($"uint1 {max} = {rng.NextUnsignedInt(max)}");
                break;
            }
            case 10:
            {
                uint min = (uint)sys.Next(0, 1000);
                uint max = min + (uint)sys.Next(1, 1000);
                w.WriteLine($"uint2 {min} {max} = {rng.NextUnsignedInt(min, max)}");
                break;
            }
            case 11:
            {
                ulong max = sys.Next(4) switch
                {
                    0 => ulong.MaxValue,
                    1 => ulong.MaxValue - (ulong)sys.Next(1000),
                    _ => (ulong)sys.NextInt64(1, long.MaxValue),
                };
                w.WriteLine($"ulong1 {max} = {rng.NextUnsignedLong(max)}");
                break;
            }
            case 12:
            {
                ulong min = (ulong)sys.NextInt64(0, 1000000);
                ulong max = min + (ulong)sys.NextInt64(1, long.MaxValue / 2);
                w.WriteLine($"ulong2 {min} {max} = {rng.NextUnsignedLong(min, max)}");
                break;
            }
            case 13:
            {
                int min = sys.Next(0, 5);
                int max = min + sys.Next(0, 10);
                int mean = sys.Next(min, max + 1);
                int std = sys.Next(0, 4);
                w.WriteLine($"gint {mean} {std} {min} {max} = {rng.NextGaussianInt(mean, std, min, max)}");
                break;
            }
            case 14:
            {
                double mean = sys.NextDouble();
                double std = sys.NextDouble() * 0.5;
                double min = sys.NextDouble() * 10 - 5;
                double max = min + sys.NextDouble() * 10;
                w.WriteLine($"gdouble {Fmt.D(mean)} {Fmt.D(std)} {Fmt.D(min)} {Fmt.D(max)} = {Fmt.D(rng.NextGaussianDouble(mean, std, min, max))}");
                break;
            }
            case 15:
            {
                float mean = (float)sys.NextDouble();
                float std = (float)(sys.NextDouble() * 0.5);
                w.WriteLine($"gfloat {Fmt.F(mean)} {Fmt.F(std)} = {Fmt.F(rng.NextGaussianFloat(mean, std))}");
                break;
            }
            case 16:
            {
                int n = sys.Next(0, 30);
                int? picked = rng.NextItem(Enumerable.Range(0, n).Select(i => (int?)i).ToArray());
                w.WriteLine($"item {n} = {picked ?? -1}");
                break;
            }
            case 17:
            {
                int n = sys.Next(0, 12);
                float[] weights = new float[n];
                for (int i = 0; i < n; i++)
                    weights[i] = sys.Next(5) == 0 ? 0f : RandFloat(sys, 0f, sys.Next(2) == 0 ? 1f : 100f);
                int?[] items = Enumerable.Range(0, n).Select(i => (int?)i).ToArray();
                int? picked = rng.WeightedNextItem(items, x => weights[x!.Value]);
                string ws = string.Join(" ", weights.Select(Fmt.F));
                w.WriteLine($"weighted {n} {ws} = {picked ?? -1}".Replace("  ", " "));
                break;
            }
            case 18:
            {
                int n = sys.Next(0, 40);
                var list = Enumerable.Range(0, n).ToList();
                rng.Shuffle(list);
                w.WriteLine($"shuffle {n} = {string.Join(" ", list)}".TrimEnd());
                break;
            }
            case 19:
            {
                int n = sys.Next(0, 40);
                var list = Enumerable.Range(0, n).ToList();
                list.UnstableShuffle(rng);
                w.WriteLine($"ushuffle {n} = {string.Join(" ", list)}".TrimEnd());
                break;
            }
            case 20:
            {
                int n = sys.Next(0, 60);
                int distinct = sys.Next(1, 8);
                var list = new List<Item>();
                for (int i = 0; i < n; i++) list.Add(new Item { Key = sys.Next(distinct), Id = i });
                string keys = string.Join(" ", list.Select(x => x.Key));
                list.StableShuffle(rng);
                w.WriteLine($"sshuffle {n} {keys} = {string.Join(" ", list.Select(x => x.Id))}".Replace("  ", " ").TrimEnd());
                break;
            }
            default:
            {
                var st = rng.ToSerializable();
                w.WriteLine($"state {st.state0} {st.state1} {st.state2} {st.state3} {st.counter}");
                break;
            }
        }
    }
}

internal static class SortGolden
{
    public static void Write(string path, int mult)
    {
        using var w = new StreamWriter(path);
        var sys = new System.Random(7);
        for (int c = 0; c < 400 * mult; c++)
        {
            int n = c < 40 ? c : sys.Next(0, sys.Next(2) == 0 ? 40 : 400);
            int distinct = sys.Next(1, sys.Next(2) == 0 ? 4 : 50);
            var list = new List<Item>();
            for (int i = 0; i < n; i++) list.Add(new Item { Key = sys.Next(distinct), Id = i });
            // Occasionally feed already-sorted / reversed input.
            if (sys.Next(6) == 0) list = list.OrderBy(x => x.Key).Select((x, i) => new Item { Key = x.Key, Id = i }).ToList();
            else if (sys.Next(6) == 0) list = list.OrderByDescending(x => x.Key).Select((x, i) => new Item { Key = x.Key, Id = i }).ToList();
            string keys = string.Join(" ", list.Select(x => x.Key));
            list.Sort();
            w.WriteLine($"sort {n} {keys} = {string.Join(" ", list.Select(x => x.Id))}".Replace("  ", " ").TrimEnd());
        }
    }
}

internal static class DecimalGolden
{
    private static System.Random _sys = new System.Random(4242);
    private static readonly List<decimal> _pool = new List<decimal>();

    private static decimal Make(ulong lo64, uint hi, bool neg, int scale)
    {
        return new decimal((int)(uint)lo64, (int)(uint)(lo64 >> 32), (int)hi, neg, (byte)scale);
    }

    private static decimal Operand()
    {
        var sys = _sys;
        switch (sys.Next(9))
        {
            case 0:
                return sys.Next(-50, 500);
            case 1:
                return Make((ulong)sys.Next(0, 100000), 0, sys.Next(4) == 0, sys.Next(0, 5));
            case 2:
                return Make((ulong)sys.NextInt64(), (uint)sys.Next(), sys.Next(2) == 0, sys.Next(0, 29));
            case 3:
                return Make((ulong)sys.NextInt64(), 0, sys.Next(2) == 0, sys.Next(0, 29));
            case 4:
            {
                decimal[] specials = { decimal.MaxValue, decimal.MinValue, 0m, 0.0m, 1m, -1m, 0.5m, 1.5m, 0.75m, 1.25m, 100m, 0.0000000000000000000000000001m, 7.9228162514264337593543950335m };
                return specials[sys.Next(specials.Length)];
            }
            default:
                if (_pool.Count > 0) return _pool[sys.Next(_pool.Count)];
                return sys.Next(1, 100);
        }
    }

    private static void Remember(decimal d)
    {
        if (_pool.Count < 4000) _pool.Add(d);
        else _pool[_sys.Next(_pool.Count)] = d;
    }

    private static string Try(Func<decimal> f)
    {
        try
        {
            decimal r = f();
            Remember(r);
            return Fmt.Dec(r);
        }
        catch (OverflowException)
        {
            return "OVF";
        }
        catch (DivideByZeroException)
        {
            return "DIV0";
        }
    }

    public static void Write(string path, int mult)
    {
        using var w = new StreamWriter(path);
        // Seed the pool with typical game values: amounts, percentages, multipliers.
        foreach (int p in new[] { 1, 3, 7, 10, 25, 30, 33, 50, 75, 100 })
        {
            Remember(p / 100m);
            Remember(1m + p / 100m);
            Remember(100m / p);
        }
        int n = 5000 * mult;
        for (int i = 0; i < n; i++)
        {
            decimal a = Operand(), b = Operand();
            string A = Fmt.Dec(a), B = Fmt.Dec(b);
            w.WriteLine($"add {A} {B} = {Try(() => a + b)}");
            w.WriteLine($"sub {A} {B} = {Try(() => a - b)}");
            w.WriteLine($"mul {A} {B} = {Try(() => a * b)}");
            w.WriteLine($"div {A} {B} = {Try(() => a / b)}");
            w.WriteLine($"cmp {A} {B} = {decimal.Compare(a, b)}");
            string toInt;
            try { toInt = ((int)a).ToString(CultureInfo.InvariantCulture); } catch (OverflowException) { toInt = "OVF"; }
            w.WriteLine($"toint {A} = {toInt}");
            w.WriteLine($"floor {A} = {Fmt.Dec(Math.Floor(a))}");
            w.WriteLine($"ceil {A} = {Fmt.Dec(Math.Ceiling(a))}");
            w.WriteLine($"trunc {A} = {Fmt.Dec(Math.Truncate(a))}");
            int decs = _sys.Next(0, 4);
            w.WriteLine($"round {A} {decs} 0 = {Fmt.Dec(Math.Round(a, decs, MidpointRounding.ToEven))}");
            w.WriteLine($"round {A} {decs} 1 = {Fmt.Dec(Math.Round(a, decs, MidpointRounding.AwayFromZero))}");
            w.WriteLine($"todouble {A} = {Fmt.D((double)a)}");
            w.WriteLine($"tofloat {A} = {Fmt.F((float)a)}");
            w.WriteLine($"max {A} {B} = {Fmt.Dec(Math.Max(a, b))}");
            w.WriteLine($"min {A} {B} = {Fmt.Dec(Math.Min(a, b))}");

            float f = _sys.Next(5) switch
            {
                0 => (float)_sys.NextDouble(),
                1 => (float)(_sys.NextDouble() * 1000),
                2 => _sys.Next(-1000, 1000) / 4f,
                3 => BitConverter.UInt32BitsToSingle((uint)_sys.Next() | ((uint)_sys.Next(2) << 31)),
                _ => (float)(_sys.NextDouble() * Math.Pow(10, _sys.Next(-30, 30))),
            };
            if (float.IsFinite(f))
            {
                string r;
                try { r = Fmt.Dec((decimal)f); } catch (OverflowException) { r = "OVF"; }
                w.WriteLine($"fromfloat {Fmt.F(f)} = {r}");
            }
        }
    }
}
