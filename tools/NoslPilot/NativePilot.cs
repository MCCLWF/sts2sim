using System.Runtime.InteropServices;

namespace NoslPilot;

// nosl/pilot_api.cpp. Every call happens on the game's main thread.
internal sealed class NativePilot : IDisposable
{
    private const string Library = "libsts2nosl_pilot.so";
    private IntPtr _handle;

    internal NativePilot(string seed, int ascension, int actorFilter, string weights, ulong policySeed, bool greedy)
    {
        _handle = Create(seed, ascension, actorFilter, weights, policySeed, greedy ? 1 : 0);
        if (_handle == IntPtr.Zero) throw new InvalidOperationException("nosl_pilot_create: " + LastError());
    }

    // The next action as JSON; the shadow has advanced by it.
    internal string Next() => Text(NextNative(_handle), "nosl_pilot_next");
    // The pending decision as JSON; nothing advanced.
    internal string Peek() => Text(PeekNative(_handle), "nosl_pilot_peek");

    // The shadow's own observation at the pending decision, before the actor filter (for comparison with Observer).
    internal Obs Observe()
    {
        const int maxTokens = 1024, maxActions = 1024;
        int[] global = new int[Obs.GlobalWidth], tokens = new int[maxTokens * Obs.TokenWidth], actions = new int[maxActions * Obs.ActionWidth],
            legal = new int[maxActions * 5], counts = new int[2];
        if (ObserveNative(_handle, global, tokens, maxTokens, actions, legal, maxActions, counts) != 0)
            throw new InvalidOperationException("nosl_pilot_observe: " + LastError());
        Obs o = new();
        System.Array.Copy(global, o.Global, Obs.GlobalWidth);
        for (int i = 0; i < counts[0]; i++) o.Tokens.Add(tokens.AsSpan(i * Obs.TokenWidth, Obs.TokenWidth).ToArray());
        for (int i = 0; i < counts[1]; i++)
        {
            o.Actions.Add(actions.AsSpan(i * Obs.ActionWidth, Obs.ActionWidth).ToArray());
            o.Legal.Add(legal.AsSpan(i * 5, 5).ToArray());
        }
        return o;
    }

    // A content id by entry name (table: card, relic, potion, monster, power, encounter, event, enchantment, affliction, character).
    internal static int Id(string table, string name) => IdNative(table, name);

    private static string Text(IntPtr line, string call)
    {
        if (line == IntPtr.Zero) throw new InvalidOperationException(call + ": " + LastError());
        return Marshal.PtrToStringUTF8(line)!;
    }

    private static string LastError() => Marshal.PtrToStringUTF8(LastErrorNative()) ?? "";

    public void Dispose()
    {
        if (_handle != IntPtr.Zero) Destroy(_handle);
        _handle = IntPtr.Zero;
    }

    [DllImport(Library, EntryPoint = "nosl_pilot_create")]
    private static extern IntPtr Create([MarshalAs(UnmanagedType.LPUTF8Str)] string seed, int ascension, int actorFilter,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string weights, ulong policySeed, int greedy);
    [DllImport(Library, EntryPoint = "nosl_pilot_next")] private static extern IntPtr NextNative(IntPtr pilot);
    [DllImport(Library, EntryPoint = "nosl_pilot_peek")] private static extern IntPtr PeekNative(IntPtr pilot);
    [DllImport(Library, EntryPoint = "nosl_pilot_observe")]
    private static extern int ObserveNative(IntPtr pilot, int[] global, int[] tokens, int maxTokens, int[] actions, int[] legal, int maxActions, int[] counts);
    [DllImport(Library, EntryPoint = "nosl_id")]
    private static extern int IdNative([MarshalAs(UnmanagedType.LPUTF8Str)] string table, [MarshalAs(UnmanagedType.LPUTF8Str)] string name);
    [DllImport(Library, EntryPoint = "nosl_pilot_last_error")] internal static extern IntPtr LastErrorNative();
    [DllImport(Library, EntryPoint = "nosl_pilot_destroy")] private static extern void Destroy(IntPtr pilot);
}

// nosl/actor.h through nosl/pilot_api.cpp: the actor on observation tables alone. No simulator state is created.
internal sealed class NativeActor : IDisposable
{
    private const string Library = "libsts2nosl_pilot.so";
    private IntPtr _handle;

    internal NativeActor(string weights, int actorFilter, ulong policySeed, bool greedy)
    {
        _handle = Create(weights, actorFilter, policySeed, greedy ? 1 : 0);
        if (_handle == IntPtr.Zero)
            throw new InvalidOperationException("nosl_actor_create: " + Marshal.PtrToStringUTF8(NativePilot.LastErrorNative()));
    }

    // The index of the chosen action in `o.Legal`, with how many rows the actor filter left, the probability the
    // policy gave it and the value estimate.
    internal int Decide(Obs o, out int rows, out double prob, out float value)
    {
        int[] tokens = new int[Math.Max(1, o.Tokens.Count) * Obs.TokenWidth], actions = new int[o.Actions.Count * Obs.ActionWidth],
            legal = new int[o.Legal.Count * 5];
        for (int i = 0; i < o.Tokens.Count; i++) System.Array.Copy(o.Tokens[i], 0, tokens, i * Obs.TokenWidth, Obs.TokenWidth);
        for (int i = 0; i < o.Actions.Count; i++)
        {
            System.Array.Copy(o.Actions[i], 0, actions, i * Obs.ActionWidth, Obs.ActionWidth);
            System.Array.Copy(o.Legal[i], 0, legal, i * 5, 5);
        }
        int index = DecideNative(_handle, o.Global, tokens, o.Tokens.Count, actions, legal, o.Actions.Count, out prob, out value, out rows);
        if (index < 0) throw new InvalidOperationException("nosl_actor_decide: " + Marshal.PtrToStringUTF8(NativePilot.LastErrorNative()));
        return index;
    }

    public void Dispose()
    {
        if (_handle != IntPtr.Zero) Destroy(_handle);
        _handle = IntPtr.Zero;
    }

    [DllImport(Library, EntryPoint = "nosl_actor_create")]
    private static extern IntPtr Create([MarshalAs(UnmanagedType.LPUTF8Str)] string weights, int actorFilter, ulong policySeed, int greedy);
    [DllImport(Library, EntryPoint = "nosl_actor_decide")]
    private static extern int DecideNative(IntPtr actor, int[] global, int[] tokens, int nTokens, int[] actions, int[] legal, int nActions,
        out double prob, out float value, out int rows);
    [DllImport(Library, EntryPoint = "nosl_actor_destroy")] private static extern void Destroy(IntPtr actor);
}
