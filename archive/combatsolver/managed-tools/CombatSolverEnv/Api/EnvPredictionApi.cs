using System.Collections.ObjectModel;
using Sts2EnvCompare;
using System.Text.Json;

namespace CombatSolverEnv.Api;

// Values follow the versioned Env combat action ABI. Targets are combat IDs;
// card indices refer to the inspected branch's current hand, not the live UI.
public enum EnvCombatActionKind { PlayCard = 2, EndTurn = 3, UsePotion = 10, Answer = 12 }

public readonly record struct EnvCombatAction(EnvCombatActionKind Kind, int Index = -1, int TargetCombatId = -1);

/// <summary>
/// An owned copy of the choice request that suspended a prediction. Answers are
/// integer indices into this request; they are not native model references.
/// The same value is used by ordinary snapshots and the combat-prediction
/// envelope so a future search adapter can carry a checkpoint across clones.
/// </summary>
public sealed class EnvChoiceRequest
{
    private EnvChoiceRequest(int kind, int optionCount, int minSelect, int maxSelect,
        string prompt, IReadOnlyList<int> cards, bool subset, IReadOnlyList<int> selected,
        bool canSkip, string source)
    {
        Kind = kind; OptionCount = optionCount; MinSelect = minSelect; MaxSelect = maxSelect;
        Prompt = prompt; Cards = cards; Subset = subset; Selected = selected;
        CanSkip = canSkip; Source = source;
    }

    public int Kind { get; }
    public int OptionCount { get; }
    public int MinSelect { get; }
    public int MaxSelect { get; }
    public string Prompt { get; }
    public IReadOnlyList<int> Cards { get; }
    public bool Subset { get; }
    public IReadOnlyList<int> Selected { get; }
    public bool CanSkip { get; }
    public string Source { get; }

    internal static EnvChoiceRequest Parse(JsonElement value, string errorPrefix)
    {
        if (value.ValueKind != JsonValueKind.Object)
            throw new InvalidOperationException($"{errorPrefix}:choice is not an object");
        string[] expected = ["kind", "option_count", "min_select", "max_select", "prompt",
            "cards", "subset", "selected", "can_skip", "source"];
        string[] fields = value.EnumerateObject().Select(field => field.Name).ToArray();
        if (fields.Length != expected.Length || fields.Distinct(StringComparer.Ordinal).Count() != expected.Length
            || expected.Any(field => !fields.Contains(field, StringComparer.Ordinal)))
            throw new InvalidOperationException($"{errorPrefix}:choice fields mismatch");
        int kind = value.GetProperty("kind").GetInt32();
        int optionCount = value.GetProperty("option_count").GetInt32();
        int minSelect = value.GetProperty("min_select").GetInt32();
        int maxSelect = value.GetProperty("max_select").GetInt32();
        if (kind is < 0 or > 2 || optionCount is < 0 or > 512
            || minSelect < 0 || maxSelect < minSelect || maxSelect > optionCount)
            throw new InvalidOperationException($"{errorPrefix}:choice range mismatch");
        string prompt = value.GetProperty("prompt").GetString()
            ?? throw new InvalidOperationException($"{errorPrefix}:choice prompt missing");
        string source = value.GetProperty("source").GetString()
            ?? throw new InvalidOperationException($"{errorPrefix}:choice source missing");
        static ReadOnlyCollection<int> ReadIndices(JsonElement array, int optionCount, string name, string prefix)
        {
            if (array.ValueKind != JsonValueKind.Array || array.GetArrayLength() > 512)
                throw new InvalidOperationException($"{prefix}:choice {name} shape mismatch");
            int[] values = array.EnumerateArray().Select(item => item.GetInt32()).ToArray();
            if (values.Any(index => index < 0 || index >= optionCount))
                throw new InvalidOperationException($"{prefix}:choice {name} range mismatch");
            return Array.AsReadOnly(values);
        }
        JsonElement cards = value.GetProperty("cards");
        if (cards.ValueKind != JsonValueKind.Array || cards.GetArrayLength() > 512
            || cards.EnumerateArray().Any(item => item.GetInt32() < 0))
            throw new InvalidOperationException($"{errorPrefix}:choice cards shape mismatch");
        ReadOnlyCollection<int> cardValues = Array.AsReadOnly(
            cards.EnumerateArray().Select(item => item.GetInt32()).ToArray());
        ReadOnlyCollection<int> selected = ReadIndices(
            value.GetProperty("selected"), optionCount, "selected", errorPrefix);
        return new(kind, optionCount, minSelect, maxSelect, prompt, cardValues,
            value.GetProperty("subset").GetBoolean(), selected,
            value.GetProperty("can_skip").GetBoolean(), source);
    }
}

public enum EnvPredictionStatus { Ready, Unported, Error }

public sealed record EnvSeededCombatOptions(string Seed, string Encounter, int Ascension = 0, int ActIndex = 0);

/// <summary>
/// Public owned-branch API for callers of the installed Mod. Seeded combat is
/// explicitly a benchmark/hypothetical root; it does not import a live battle.
/// The Mod initializer must have loaded the native library before calling this API.
/// </summary>
public static class EnvPredictionApi
{
    public const int ApiVersion = 1;

    public static EnvPredictionSession OpenSeededIroncladCombat(EnvSeededCombatOptions options)
    {
        ArgumentNullException.ThrowIfNull(options);
        ArgumentException.ThrowIfNullOrWhiteSpace(options.Seed);
        ArgumentException.ThrowIfNullOrWhiteSpace(options.Encounter);
        return new EnvPredictionSession(NativeEnv.OpenCombat(
            options.Seed, options.Encounter, options.Ascension, options.ActIndex));
    }

    /// <summary>
    /// Restore a complete versioned combat or pending-choice snapshot from this
    /// loaded native runtime. The result is owned independently. This restores
    /// explicit state; it does not infer a live battle from a seed or replace
    /// the still-required live-game field mapper.
    /// </summary>
    public static EnvPredictionSession RestoreCombatSnapshot(EnvStateSnapshot snapshot)
    {
        ArgumentNullException.ThrowIfNull(snapshot);
        return new EnvPredictionSession(NativeEnv.ImportCombatSnapshot(snapshot.Document.GetRawText()));
    }

    /// <summary>
    /// Start a combat search branch from an explicit active ordinary snapshot.
    /// Win/loss is recorded at safe combat boundaries before post-combat effects.
    /// This engine scope does not execute the still-incomplete Beam backend.
    /// </summary>
    public static EnvCombatPredictionSession OpenCombatPrediction(EnvStateSnapshot root)
    {
        ArgumentNullException.ThrowIfNull(root);
        return new(NativeCombatPrediction.FromGameSnapshot(root.Document.GetRawText()));
    }

    public static EnvCombatPredictionSession RestoreCombatPrediction(EnvCombatPredictionSnapshot snapshot)
    {
        ArgumentNullException.ThrowIfNull(snapshot);
        return new(NativeCombatPrediction.Restore(snapshot.Document.GetRawText()));
    }
}

/// <summary>
/// Every Predict/Clone result is a new owned branch. Dispose it independently.
/// Inspection and failed predictions preserve the source state. Shared-source
/// access is serialized; independent successors can be used concurrently.
/// </summary>
public sealed class EnvPredictionSession : IDisposable
{
    private readonly NativeEnv.Session _session;
    internal EnvPredictionSession(NativeEnv.Session session) => _session = session;

    public ulong StateHash => _session.StateHash;
    public string InspectJson() => _session.InspectJson();
    /// <summary>Complete privileged state, including hidden RNG; not an agent observation.</summary>
    public EnvStateSnapshot CaptureSnapshot() => EnvStateSnapshot.Parse(_session.ExportSnapshotJson());
    /// <summary>
    /// Owned immutable monster definition metadata for this branch's runtime.
    /// Includes symbolic states and callbacks; does not invoke monster actions.
    /// Registry indices are scoped to the returned runtime_id, not portable IDs.
    /// </summary>
    public JsonElement InspectMonsterMachines()
    {
        using JsonDocument document = JsonDocument.Parse(_session.ExportMonsterMachinesJson());
        return document.RootElement.Clone();
    }
    public EnvPredictionSession Clone() => new(_session.Clone());

    public EnvPredictionSession Predict(EnvCombatAction action)
    {
        if (!Enum.IsDefined(action.Kind))
            throw new ArgumentOutOfRangeException(nameof(action), "Unknown Env combat action kind");
        return new EnvPredictionSession(_session.Predict(
            new EnvAction((int)action.Kind, action.Index, action.TargetCombatId)));
    }

    /// <summary>
    /// A failure has no successor. Unported is separate from a transition error;
    /// callers must stop using Env on unported content rather than score a failed branch.
    /// Use Predict when exceptions should propagate with their original stack.
    /// </summary>
    public EnvPredictionStatus TryPredict(EnvCombatAction action,
                                          out EnvPredictionSession? successor, out string? reason)
    {
        successor = null;
        reason = null;
        try
        {
            successor = Predict(action);
            return EnvPredictionStatus.Ready;
        }
        catch (EnvPredictionUnportedException error)
        {
            reason = error.Message;
            return EnvPredictionStatus.Unported;
        }
        catch (InvalidOperationException error) when (error is not ObjectDisposedException)
        {
            reason = error.Message;
            return EnvPredictionStatus.Error;
        }
        catch (ArgumentOutOfRangeException error)
        {
            reason = error.Message;
            return EnvPredictionStatus.Error;
        }
    }

    public void Dispose() => _session.Dispose();
}
