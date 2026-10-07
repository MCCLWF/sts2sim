// Minimal stand-ins for game types referenced by the verbatim-compiled sources.
// StringHelper's hash/case helpers are copied unchanged from
// src/Core/Helpers/StringHelper.cs (the rest of that file needs Godot/UI types).
using System;
using System.IO.Hashing;
using System.Text;
using System.Text.RegularExpressions;

namespace MegaCrit.Sts2.Core.Saves
{
    public class SerializableRng
    {
        public int counter;
        public ulong state0;
        public ulong state1;
        public ulong state2;
        public ulong state3;
    }
}

namespace MegaCrit.Sts2.Core.Entities.Players
{
    public class StubRunRngSet
    {
        public ulong Seed { get; set; }
    }

    public class StubRunState
    {
        public StubRunRngSet Rng { get; } = new StubRunRngSet();
        public int Slot { get; set; }
        public int GetPlayerSlotIndex(Player player) => Slot;
    }

    public class Player
    {
        public StubRunState RunState { get; } = new StubRunState();
    }
}

namespace MegaCrit.Sts2.Core.Helpers
{
    public static partial class StringHelper
    {
        [ThreadStatic]
        private static byte[]? _stringHashCache;

        [GeneratedRegex("([A-Za-z0-9]|\\G(?!^))([A-Z])")]
        private static partial Regex CamelCaseRegex();

        [GeneratedRegex("\\s+")]
        private static partial Regex WhitespaceRegex();

        [GeneratedRegex("[^A-Z0-9_]")]
        private static partial Regex SpecialCharRegex();

        public static string SnakeCase(string txt)
        {
            return CamelCaseRegex().Replace(txt.Trim(), "$1_$2").ToLowerInvariant();
        }

        public static string Slugify(string txt)
        {
            string text = CamelCaseRegex().Replace(txt.Trim(), "$1_$2");
            string input = WhitespaceRegex().Replace(text.ToUpperInvariant(), "_");
            return SpecialCharRegex().Replace(input, "");
        }

        public static ulong GetDeterministicHashCode(string str)
        {
            if (_stringHashCache == null)
            {
                _stringHashCache = new byte[1024];
            }
            int byteCount = Encoding.UTF8.GetByteCount(str);
            if (byteCount > _stringHashCache.Length)
            {
                _stringHashCache = new byte[(int)Math.Round((double)byteCount * 1.5)];
            }
            byteCount = Encoding.UTF8.GetBytes(str, _stringHashCache);
            return XxHash64.HashToUInt64(_stringHashCache.AsSpan().Slice(0, byteCount), 0L);
        }

        public static int GetDeterministicHashCodeOld(string str)
        {
            int num = 352654597;
            int num2 = num;
            for (int i = 0; i < str.Length; i += 2)
            {
                num = ((num << 5) + num) ^ str[i];
                if (i == str.Length - 1)
                {
                    break;
                }
                num2 = ((num2 << 5) + num2) ^ str[i + 1];
            }
            return num + num2 * 1566083941;
        }
    }

    public static class SeedHelper
    {
        // Copied from src/Core/Helpers/SeedHelper.cs.
        public static string CanonicalizeSeed(string seed)
        {
            seed = seed.ToUpperInvariant();
            seed = seed.Replace('O', '0');
            seed = seed.Replace('I', '1');
            seed = seed.Trim();
            return seed;
        }
    }
}
