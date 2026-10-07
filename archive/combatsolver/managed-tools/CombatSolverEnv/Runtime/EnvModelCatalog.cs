using System.Text.Json;
using System.Reflection;
using MegaCrit.Sts2.Core.Debug;
using MegaCrit.Sts2.Core.Models;

namespace CombatSolver;

internal static class EnvModelCatalog
{
    internal static readonly JsonElement Content = Load();
    private static JsonElement Load()
    {
        using Stream stream = typeof(EnvModelCatalog).Assembly.GetManifestResourceStream("CombatSolverEnv.CardCatalog.json")
            ?? throw new InvalidOperationException("missing pinned Env model catalog");
        using JsonDocument document = JsonDocument.Parse(stream);
        return document.RootElement.Clone();
    }
    internal static void RequireGameVersion()
    {
        if (AssemblyHasher.GetMainAssemblyHash() != Content.GetProperty("main_assembly_hash").GetInt32())
            throw new InvalidOperationException("live_models_game_assembly_mismatch");
    }

    internal static int Id(AbstractModel? model, string category)
    {
        if (model is null) return 0;
        if (model.GetType().Assembly != typeof(AbstractModel).Assembly
            || !Content.GetProperty(category).TryGetProperty(model.Id.Entry, out JsonElement metadata)
            // The pinned card and monster catalogs contain only non-mock
            // generated IDs; other categories retain an explicit mock flag.
            || (category is not ("cards" or "monsters") && metadata.GetProperty("mock").GetBoolean())
            || metadata.GetProperty("type").GetString() != model.GetType().FullName)
            throw new InvalidOperationException($"live_model_binding_unmapped:{category}:{model.Id.Entry}");
        return metadata.GetProperty("id").GetInt32();
    }

    internal static void RequireSubclassFields(AbstractModel model, Type baseType, JsonElement mappedFields)
    {
        HashSet<string> known = mappedFields.EnumerateArray().Select(field => field.GetString()!).ToHashSet(StringComparer.Ordinal);
        Type leaf = model.GetType();
        for (Type? type = leaf; type is not null && type != baseType; type = type.BaseType)
            foreach (FieldInfo field in type.GetFields(BindingFlags.DeclaredOnly | BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic))
            {
                // Exporter.Fields intentionally omits reference and compiler
                // backing fields. Absence in content.json is not evidence that
                // the live subclass has no gameplay state.
                string key = type == leaf ? field.Name : type.Name + "." + field.Name;
                if (!known.Contains(key)) throw new InvalidOperationException($"live_model_hidden_subclass_field_unmapped:{model.Id.Entry}:{type.Name}.{field.Name}");
            }
    }
}
