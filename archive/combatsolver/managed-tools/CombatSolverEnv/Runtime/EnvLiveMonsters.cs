using System.Reflection;
using System.Text.Json;
using System.Text.Json.Nodes;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Monsters;
using MegaCrit.Sts2.Core.MonsterMoves.Intents;
using MegaCrit.Sts2.Core.MonsterMoves.MonsterMoveStateMachine;
using Sts2EnvCompare;

namespace CombatSolver;

// Symbolic frozen runtime state. Bind resolves names against an existing owned
// Env branch's registry; it never guesses registry indices or invokes callbacks.
internal sealed record EnvLiveMonsters(JsonElement Component)
{
    internal static EnvLiveMonsters Capture(IReadOnlyList<Creature> creatures)
    {
        EnvModelCatalog.RequireGameVersion();
        JsonArray monsters = [];
        foreach (Creature creature in creatures)
        {
            if (creature.Monster is not { } monster) continue;
            if (creature.CombatId is not { } combatId || combatId > int.MaxValue)
                throw new InvalidOperationException("live_monster_combat_id_missing_or_overflow");
            if (monster.GetType().Assembly != typeof(MonsterModel).Assembly
                || !EnvModelCatalog.Content.GetProperty("monsters").TryGetProperty(monster.Id.Entry, out JsonElement metadata)
                || metadata.GetProperty("type").GetString() != monster.GetType().FullName)
                throw new InvalidOperationException($"live_monster_unknown_model:{monster.Id.Entry}");
            EnvModelCatalog.RequireSubclassFields(monster, typeof(MonsterModel), metadata.GetProperty("fields"));
            int[] extra = new int[4];
            foreach (var field in metadata.GetProperty("fields").EnumerateArray())
            {
                string name = field.GetString()!;
                // These two fields only drive the sound in PlayAttackSfx.
                if (name == "_attackSfxStrength" && monster is CalcifiedCultist or DampCultist) continue;
                if (name == "_isFront" && monster is Toadpole toadpole) { extra[0] = toadpole._isFront ? 1 : 0; continue; }
                throw new InvalidOperationException($"live_monster_internal_field_unmapped:{monster.Id.Entry}:{name}");
            }
            if (metadata.GetProperty("variables").GetArrayLength() != 0)
                throw new InvalidOperationException($"live_monster_variables_unmapped:{monster.Id.Entry}");
            var rng = monster._rng ?? throw new InvalidOperationException("live_monster_rng_missing");
            var random = rng._random;
            JsonObject frozen = new()
            {
                ["combat_id"] = (int)combatId, ["monster"] = monster.Id.Entry,
                ["id"] = metadata.GetProperty("id").GetInt32(),
                ["rng"] = new JsonObject { ["counter"] = rng._counter, ["s0"] = random._s0, ["s1"] = random._s1, ["s2"] = random._s2, ["s3"] = random._s3 },
                ["is_performing_move"] = monster._isPerformingMove, ["spawned_this_turn"] = monster._spawnedThisTurn,
                ["extra"] = new JsonArray(extra.Select(value => JsonValue.Create(value)).ToArray()),
                ["next_move"] = monster.NextMove.Id,
                ["states"] = new JsonArray(), ["initial"] = null, ["current"] = null,
                ["performed_first_move"] = false, ["log"] = new JsonArray(),
            };
            if (monster._isPerformingMove) throw new InvalidOperationException("live_monster_capture_during_move");
            if (monster.MoveStateMachine is { } machine)
            {
                if (machine.GetType() != typeof(MonsterMoveStateMachine) || machine.States.Count > 24 || machine.StateLog.Count > 160)
                    throw new InvalidOperationException("live_monster_machine_type_or_capacity");
                frozen["initial"] = machine._initialState.Id;
                frozen["current"] = machine._currentState.Id;
                frozen["performed_first_move"] = machine._performedFirstMove;
                frozen["log"] = new JsonArray(machine.StateLog.Select(state => JsonValue.Create(state.Id)).ToArray());
                JsonArray states = frozen["states"]!.AsArray();
                foreach (var pair in machine.States)
                {
                    if (pair.Key != pair.Value.Id) throw new InvalidOperationException("live_monster_state_identity_mismatch");
                    states.Add(CaptureState(pair.Value, monster));
                }
            }
            monsters.Add(frozen);
        }
        return new(JsonSerializer.SerializeToElement(new JsonObject { ["monsters"] = monsters }));
    }

    private static JsonObject CaptureState(MonsterState state, MonsterModel owner)
    {
        JsonObject result = new() { ["id"] = state.Id, ["weights"] = new JsonArray(), ["branches"] = new JsonArray(), ["intents"] = new JsonArray() };
        if (state.GetType() == typeof(MoveState))
        {
            MoveState move = (MoveState)state;
            var callback = move._onPerform;
            if (callback.GetInvocationList().Length != 1 || !ReferenceEquals(callback.Target, owner)
                || callback.Method.Module.Assembly != typeof(MonsterModel).Assembly
                || (callback.Method.DeclaringType != owner.GetType() && callback.Method.DeclaringType != typeof(MonsterModel)))
                throw new InvalidOperationException($"live_monster_move_callback_unmapped:{owner.Id.Entry}:{move.Id}");
            result["kind"] = 0;
            result["on_perform"] = callback.Method.DeclaringType!.Name + "." + callback.Method.Name;
            result["follow_up"] = move.FollowUpState?.Id ?? move.FollowUpStateId;
            result["must_perform_once"] = move.MustPerformOnceBeforeTransitioning;
            result["performed"] = move._performedAtLeastOnce;
            foreach (AbstractIntent intent in move.Intents) result["intents"]!.AsArray().Add(CaptureIntent(intent, owner));
        }
        else if (state.GetType() == typeof(RandomBranchState))
        {
            result["kind"] = 1;
            foreach (var weight in ((RandomBranchState)state).States)
            {
                bool constant = TryReadConstructorConstant<float>(weight.weightLambda, typeof(RandomBranchState), "weight", out float value);
                if (!constant) throw new InvalidOperationException($"live_monster_dynamic_weight_unmapped:{owner.Id.Entry}:{state.Id}:{weight.stateId}");
                result["weights"]!.AsArray().Add(new JsonObject
                {
                    ["state"] = weight.stateId, ["repeat_type"] = (int)weight.repeatType,
                    ["max_times"] = weight.maxTimes, ["cooldown"] = weight.cooldown, ["dynamic"] = !constant,
                    ["weight_bits"] = constant ? BitConverter.SingleToUInt32Bits(value) : 0u,
                });
            }
        }
        else if (state.GetType() == typeof(ConditionalBranchState))
        {
            result["kind"] = 2;
            int index = 0;
            foreach (var branch in ((ConditionalBranchState)state).States)
            {
                if (branch._conditionalLambda is { } callback) RequireMappedCondition(callback, owner, state.Id, branch.id, index);
                result["branches"]!.AsArray().Add(new JsonObject { ["state"] = branch.id, ["conditional"] = branch._conditionalLambda is not null });
                index++;
            }
        }
        else throw new InvalidOperationException($"live_monster_state_type_unmapped:{state.GetType().FullName}");
        return result;
    }

    private static JsonObject CaptureIntent(AbstractIntent intent, MonsterModel owner)
    {
        if (intent.GetType().Assembly != typeof(AbstractIntent).Assembly)
            throw new InvalidOperationException("live_monster_intent_type_unmapped");
        JsonObject result = new()
        {
            ["type"] = (int)intent.IntentType, ["damage"] = 0, ["dynamic_damage"] = false, ["repeat"] = 1,
            ["dynamic_repeat"] = false, ["multi"] = intent is MultiAttackIntent,
            ["strong"] = intent is DebuffIntent debuff && debuff._strong, ["count"] = intent is StatusIntent status ? status.CardCount : 0,
        };
        if (intent is AttackIntent attack)
        {
            var callback = attack.DamageCalc ?? throw new InvalidOperationException("live_monster_attack_calculator_missing");
            bool constant = TryReadConstructorConstant<int>(callback, intent.GetType(), "damage", out int damage);
            if (!constant) throw new InvalidOperationException($"live_monster_dynamic_damage_unmapped:{owner.Id.Entry}");
            result["damage"] = constant ? damage : 0;
            result["dynamic_damage"] = !constant;
            if (intent is MultiAttackIntent multi)
            {
                if (multi._repeatCalc is not null) throw new InvalidOperationException($"live_monster_dynamic_repeat_unmapped:{owner.Id.Entry}");
                result["repeat"] = multi._repeatCalc is null ? multi._repeat : 0;
                result["dynamic_repeat"] = multi._repeatCalc is not null;
            }
        }
        return result;
    }

    // Read the exact scalar closure created by a vanilla constructor/AddBranch.
    // Do not execute even a seemingly harmless delegate during root capture.
    private static bool TryReadConstructorConstant<T>(Delegate callback, Type origin, string fieldName, out T value) where T : struct
    {
        value = default;
        Type? declaring = callback.Method.DeclaringType;
        byte[]? il = callback.Method.GetMethodBody()?.GetILAsByteArray();
        if (callback.GetInvocationList().Length != 1 || declaring?.DeclaringType != origin
            || callback.Target?.GetType() != declaring || il is null) return false;
        if (typeof(T) == typeof(float) && il.Length == 6 && il[0] == 0x22 && il[5] == 0x2a)
        {
            value = (T)(object)BitConverter.ToSingle(il, 1);
            return true;
        }
        FieldInfo? field = declaring.GetField(fieldName, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance);
        bool plain = il.Length == 7 && il[6] == 0x2a;
        bool decimalConversion = false;
        if (typeof(T) == typeof(int) && il.Length == 12 && il[11] == 0x2a && (il[6] == 0x73 || il[6] == 0x28))
        {
            MethodBase? conversion = callback.Method.Module.ResolveMethod(BitConverter.ToInt32(il, 7));
            decimalConversion = conversion?.DeclaringType == typeof(decimal)
                && conversion.GetParameters() is [{ ParameterType: var input }] && input == typeof(int)
                && ((il[6] == 0x73 && conversion is ConstructorInfo)
                    || (il[6] == 0x28 && conversion is MethodInfo { IsStatic: true, Name: "op_Implicit", ReturnType: var output } && output == typeof(decimal)));
        }
        if (field?.FieldType != typeof(T) || (!plain && !decimalConversion) || il[0] != 0x02 || il[1] != 0x7b
            || callback.Method.Module.ResolveField(BitConverter.ToInt32(il, 2)) != field) return false;
        value = (T)field.GetValue(callback.Target)!;
        return true;
    }

    private static void RequireMappedCondition(Delegate callback, MonsterModel owner, string state, string target, int index)
    {
        // v0.111.0 GenerateMoveStateMachine: first branch is !IsFront,
        // second is IsFront. Match their identities as well as destinations;
        // a swapped vanilla delegate must not be treated as the same definition.
        string? expected = owner is Toadpole && state == "INIT_MOVE"
            ? (index, target) switch
            {
                (0, "WHIRL_MOVE") => "<GenerateMoveStateMachine>b__24_0",
                (1, "SPIKEN_MOVE") => "<GenerateMoveStateMachine>b__24_1",
                _ => null,
            } : null;
        if (callback.GetInvocationList().Length != 1 || callback.Method.Module.Assembly != typeof(MonsterModel).Assembly
            || callback.Method.DeclaringType != owner.GetType() || !ReferenceEquals(callback.Target, owner)
            || expected is null || callback.Method.Name != expected)
            throw new InvalidOperationException($"live_monster_state_lambda_unmapped:{owner.Id.Entry}:{callback.Method.Name}");
    }

    internal Dictionary<int, JsonElement> Bind(JsonElement layout)
    {
        if (layout.GetProperty("format").GetString() != "sts2env.monster_machines" || layout.GetProperty("version").GetInt32() != 1)
            throw new InvalidOperationException("monster_layout_format_mismatch");
        var definitions = layout.GetProperty("machines").EnumerateArray().ToDictionary(machine => machine.GetProperty("combat_id").GetInt32());
        Dictionary<int, JsonElement> result = [];
        foreach (var frozen in Component.GetProperty("monsters").EnumerateArray())
        {
            int id = frozen.GetProperty("combat_id").GetInt32();
            if (!definitions.TryGetValue(id, out var definition) || definition.GetProperty("monster_id").GetInt32() != frozen.GetProperty("id").GetInt32())
                throw new InvalidOperationException($"monster_layout_binding_missing:{id}");
            result.Add(id, BindMonster(frozen, definition));
        }
        if (result.Count != definitions.Count) throw new InvalidOperationException("monster_layout_roster_mismatch");
        return result;
    }

    private static JsonElement BindMonster(JsonElement frozen, JsonElement definition)
    {
        JsonElement sourceStates = frozen.GetProperty("states"), targetStates = definition.GetProperty("states");
        if (sourceStates.GetArrayLength() != targetStates.GetArrayLength() || definition.GetProperty("exists").GetBoolean() != (frozen.GetProperty("current").ValueKind != JsonValueKind.Null))
            throw new InvalidOperationException("monster_layout_state_count_mismatch");
        Dictionary<string, int> indices = targetStates.EnumerateArray().Select((state, i) => (state, i)).ToDictionary(pair => pair.state.GetProperty("id").GetString()!, pair => pair.i, StringComparer.Ordinal);
        int Index(string? name) => name is null ? -1 : indices.TryGetValue(name, out int index) ? index
            : throw new InvalidOperationException($"monster_layout_unknown_state:{name}");
        string? Name(int index) => index < 0 ? null : targetStates[index].GetProperty("id").GetString();
        uint performed = 0;
        int[] follows = Enumerable.Repeat(definition.GetProperty("exists").GetBoolean() ? -1 : 0, 24).ToArray();
        for (int i = 0; i < sourceStates.GetArrayLength(); i++)
        {
            JsonElement source = sourceStates[i], target = targetStates[i];
            if (source.GetProperty("id").GetString() != target.GetProperty("id").GetString() || source.GetProperty("kind").GetInt32() != target.GetProperty("kind").GetInt32())
                throw new InvalidOperationException("monster_layout_state_order_or_kind_mismatch");
            if (source.GetProperty("kind").GetInt32() == 0)
            {
                if (source.GetProperty("on_perform").GetString() != target.GetProperty("on_perform").GetString()
                    || source.GetProperty("must_perform_once").GetBoolean() != target.GetProperty("must_perform_once").GetBoolean()
                    || !JsonElement.DeepEquals(source.GetProperty("intents"), target.GetProperty("intents")))
                    throw new InvalidOperationException($"monster_layout_move_definition_mismatch:{source.GetProperty("id").GetString()}");
                if (source.GetProperty("performed").GetBoolean()) performed |= 1u << i;
                string? follow = source.GetProperty("follow_up").GetString();
                string? baseline = Name(target.GetProperty("follow_up").GetInt32())
                    ?? (target.GetProperty("follow_up_id").GetString() is { Length: > 0 } outside ? outside : null);
                if (follow != baseline)
                {
                    if (follow is null) throw new InvalidOperationException("monster_layout_null_follow_up_override_unmapped");
                    follows[i] = Index(follow);
                }
            }
            CompareLinks(source.GetProperty("weights"), target.GetProperty("weights"));
            CompareLinks(source.GetProperty("branches"), target.GetProperty("branches"));
        }
        if (Index(frozen.GetProperty("initial").GetString()) != definition.GetProperty("initial").GetInt32())
            throw new InvalidOperationException("monster_layout_initial_state_mismatch");
        string? next = frozen.GetProperty("next_move").GetString();
        JsonObject state = new()
        {
            ["id"] = frozen.GetProperty("id").GetInt32(), ["rng"] = JsonNode.Parse(frozen.GetProperty("rng").GetRawText()),
            ["is_performing_move"] = frozen.GetProperty("is_performing_move").GetBoolean(), ["spawned_this_turn"] = frozen.GetProperty("spawned_this_turn").GetBoolean(),
            ["machine"] = new JsonObject
            {
                ["exists"] = definition.GetProperty("exists").GetBoolean(), ["definition"] = definition.GetProperty("definition").GetInt32(),
                ["current"] = Index(frozen.GetProperty("current").GetString()), ["performed_first_move"] = frozen.GetProperty("performed_first_move").GetBoolean(),
                ["performed_at_least_once"] = performed, ["follow_up"] = new JsonArray(follows.Select(value => JsonValue.Create(value)).ToArray()),
                ["log"] = new JsonArray(frozen.GetProperty("log").EnumerateArray().Select(value => JsonValue.Create(Index(value.GetString()))).ToArray()),
            },
            ["next_move"] = new JsonObject { ["state"] = next == "UNSET_MOVE" ? -1 : Index(next), ["stunned"] = false, ["stun_move"] = 0, ["stun_follow_up"] = -1, ["stun_performed"] = false },
            ["vars"] = new JsonArray(Enumerable.Range(0, 3).Select(_ => (JsonNode)new JsonObject { ["decimal_bits"] = new JsonArray(0, 0, 0, 0) }).ToArray()),
            ["extra"] = JsonNode.Parse(frozen.GetProperty("extra").GetRawText()),
        };
        return JsonSerializer.SerializeToElement(state);

        void CompareLinks(JsonElement source, JsonElement target)
        {
            if (source.GetArrayLength() != target.GetArrayLength()) throw new InvalidOperationException("monster_layout_branch_count_mismatch");
            for (int j = 0; j < source.GetArrayLength(); j++)
            {
                JsonObject mapped = JsonNode.Parse(source[j].GetRawText())!.AsObject();
                mapped["state"] = Index(source[j].GetProperty("state").GetString());
                if (!JsonElement.DeepEquals(JsonSerializer.SerializeToElement(mapped), target[j]))
                    throw new InvalidOperationException("monster_layout_branch_definition_mismatch");
            }
        }
    }

    internal void RequireMatch(NativeEnv.Session env)
    {
        using JsonDocument state = JsonDocument.Parse(env.ExportSnapshotJson());
        using JsonDocument layout = JsonDocument.Parse(env.ExportMonsterMachinesJson());
        if (state.RootElement.GetProperty("runtime_id").GetString() != layout.RootElement.GetProperty("runtime_id").GetString()
            || state.RootElement.GetProperty("state_hash").GetUInt64() != layout.RootElement.GetProperty("state_hash").GetUInt64())
            throw new InvalidOperationException("monster_layout_source_changed");
        Dictionary<int, JsonElement> expected = Bind(layout.RootElement);
        foreach (var slot in state.RootElement.GetProperty("state").GetProperty("creatures").GetProperty("slots").EnumerateArray())
        {
            JsonElement creature = slot.GetProperty("value"), monster = creature.GetProperty("monster");
            if (monster.GetProperty("id").GetInt32() == 0) continue;
            int id = creature.GetProperty("combat_id").GetInt32();
            if (!expected.Remove(id, out var mapped))
                throw new InvalidOperationException($"root_monster_runtime_roster_mismatch:{id}");
            if (Difference(mapped, monster, "monster") is { } difference)
                throw new InvalidOperationException($"root_monster_runtime_mismatch:{id}:{difference}");
        }
        if (expected.Count != 0) throw new InvalidOperationException("root_monster_runtime_roster_mismatch");
    }

    private static string? Difference(JsonElement live, JsonElement env, string path)
    {
        if (JsonElement.DeepEquals(live, env)) return null;
        if (live.ValueKind == JsonValueKind.Object && env.ValueKind == JsonValueKind.Object)
        {
            foreach (var field in live.EnumerateObject())
            {
                if (!env.TryGetProperty(field.Name, out var other)) return path + "." + field.Name + ":missing";
                if (Difference(field.Value, other, path + "." + field.Name) is { } difference) return difference;
            }
        }
        if (live.ValueKind == JsonValueKind.Array && env.ValueKind == JsonValueKind.Array && live.GetArrayLength() == env.GetArrayLength())
        {
            for (int i = 0; i < live.GetArrayLength(); i++)
                if (Difference(live[i], env[i], path + "[" + i + "]") is { } difference) return difference;
        }
        return path + ":live=" + live.GetRawText() + ",env=" + env.GetRawText();
    }
}
