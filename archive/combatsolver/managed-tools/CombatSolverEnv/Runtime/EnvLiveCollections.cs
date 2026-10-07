using System.Reflection;
using System.Text.Json.Nodes;

namespace CombatSolver;

// CLR buckets/hash codes are lookup acceleration; observable slot order and
// the removed-slot chain are transferred to the Env DotnetSet representation.
internal static class EnvLiveCollections
{
    internal static JsonObject Set<T>(HashSet<T> source, int capacity, Func<T, int> reference)
    {
        if (source.GetType() != typeof(HashSet<T>)) throw new InvalidOperationException("live_hashset_type_unmapped");
        if (!ReferenceEquals(source.Comparer, EqualityComparer<T>.Default) && !ReferenceEquals(source.Comparer, ReferenceEqualityComparer.Instance))
            throw new InvalidOperationException("live_hashset_comparer_unmapped");
        Type type = typeof(HashSet<T>);
        const BindingFlags flags = BindingFlags.Instance | BindingFlags.NonPublic;
        object? Read(string name) => (type.GetField(name, flags) ?? throw new InvalidOperationException("live_hashset_runtime_layout_unmapped")).GetValue(source);
        int count = (int)Read("_count")!, freeCount = (int)Read("_freeCount")!, freeList = (int)Read("_freeList")!;
        if (count < 0 || count > capacity || freeCount < 0 || source.Count != count - freeCount)
            throw new InvalidOperationException("live_hashset_capacity_or_counts");
        Array? entries = (Array?)Read("_entries");
        if (count > 0 && (entries is null || entries.Length < count)) throw new InvalidOperationException("live_hashset_entries_missing");
        JsonArray slots = [];
        List<T> active = [];
        Dictionary<int, int> free = [];
        if (count > 0)
        {
            Type entryType = entries!.GetType().GetElementType()!;
            FieldInfo nextField = entryType.GetField("Next") ?? throw new InvalidOperationException("live_hashset_entry_layout_unmapped");
            FieldInfo valueField = entryType.GetField("Value") ?? throw new InvalidOperationException("live_hashset_entry_layout_unmapped");
            for (int i = 0; i < count; i++)
            {
                object entry = entries.GetValue(i)!;
                int next = (int)nextField.GetValue(entry)!;
                bool used = next >= -1;
                JsonObject slot = new() { ["index"] = i, ["used"] = used };
                if (used)
                {
                    T value = (T)valueField.GetValue(entry)!;
                    active.Add(value);
                    slot["value"] = reference(value);
                }
                else
                {
                    // .NET 9 HashSet.StartOfFreeList == -3.
                    int nextFree = -3 - next;
                    free.Add(i, nextFree);
                    slot["next_free"] = nextFree;
                }
                slots.Add(slot);
            }
        }
        if (!active.SequenceEqual(source, source.Comparer) || free.Count != freeCount)
            throw new InvalidOperationException("live_hashset_runtime_order_unmapped");
        if (count == 0) freeList = -1; // an uninitialized empty CLR set initializes this before its first Add
        HashSet<int> visited = [];
        for (int index = freeList; index != -1;)
        {
            if (!visited.Add(index) || !free.TryGetValue(index, out index))
                throw new InvalidOperationException("live_hashset_free_chain_invalid");
        }
        if (visited.Count != freeCount) throw new InvalidOperationException("live_hashset_free_chain_incomplete");
        return new() { ["capacity"] = capacity, ["count"] = count, ["free_count"] = freeCount, ["free_list"] = freeList, ["slots"] = slots };
    }
}
