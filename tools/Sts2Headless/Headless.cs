using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection.Emit;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using HarmonyLib;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Nodes.Cards;
using MegaCrit.Sts2.Core.Audio.Debug;
using MegaCrit.Sts2.Core.Models.Events;
using MegaCrit.Sts2.Core.Nodes;
using MegaCrit.Sts2.Core.Nodes.Vfx.Utilities;
using MegaCrit.Sts2.Core.Logging;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Entities.Merchant;
using MegaCrit.Sts2.Core.Nodes.Vfx;
using MegaCrit.Sts2.Core.Nodes.Vfx.Ui;
using MegaCrit.Sts2.Core.Models.Relics;
using MegaCrit.Sts2.Core.Saves;
using MegaCrit.Sts2.Core.TestSupport;
using MegaCrit.Sts2.Core.CardSelection;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Models;
using System.Threading.Tasks;

// Lets sts2.dll run in a plain .NET process. GodotSharp's native bindings are
// unbound without the engine, so any call into them would jump through a null
// function pointer. Install() binds every one of them to a native stub that
// returns zero, which turns engine calls (nodes, resources, GD.Print, ...)
// into no-ops, and redirects the game's log output to stderr. It must run
// before any game or Godot type is used.
internal static partial class Headless
{
    private static Func<Player, long?>? _handSelectionMarker;
    private static Action<Player, CardSelectorPrefs, IReadOnlyList<CardModel>, long>? _handSelectionObserver;
    private static FieldInfo? _handSelectionPlayer, _handSelectionPrefs, _handSelectionBuilder;

    // Optional offline route bookkeeping, not a gameplay bypass. FromHand
    // executes unchanged and keeps its original Task/result. Observe a
    // completed result before the caller discards/exhausts any selected card.
    // The synchronous oracle selector lets us distinguish a real callback
    // (its consumption marker advances) from the native automatic-all branch.
    // No filter is evaluated twice, and no choice, parameter or RNG is changed.
    public static Action InstallHandSelectionObservation(Func<Player, long?> marker,
        Action<Player, CardSelectorPrefs, IReadOnlyList<CardModel>, long> observer)
    {
        if (_handSelectionObserver != null) throw new InvalidOperationException("Hand selection observer already installed");
        var harmony = new Harmony("sts2sim.headless.hand-selection-observation");
        _handSelectionMarker = marker;
        _handSelectionObserver = observer;
        try
        {
            MethodInfo entry = AccessTools.Method(typeof(CardSelectCmd), nameof(CardSelectCmd.FromHand),
                    new[] { typeof(MegaCrit.Sts2.Core.GameActions.Multiplayer.PlayerChoiceContext), typeof(Player),
                            typeof(CardSelectorPrefs), typeof(Func<CardModel, bool>), typeof(AbstractModel) });
            Type machine = entry.GetCustomAttribute<AsyncStateMachineAttribute>()!.StateMachineType;
            _handSelectionPlayer = AccessTools.Field(machine, "player");
            _handSelectionPrefs = AccessTools.Field(machine, "prefs");
            _handSelectionBuilder = AccessTools.Field(machine, "<>t__builder");
            if (_handSelectionPlayer?.FieldType != typeof(Player)
                || _handSelectionPrefs?.FieldType != typeof(CardSelectorPrefs)
                || _handSelectionBuilder?.FieldType != typeof(AsyncTaskMethodBuilder<IEnumerable<CardModel>>))
                throw new InvalidOperationException("Unsupported original FromHand state machine fields");
            // Keep the public entry frame: OracleRun's selection-source trace
            // reads it. Wrapping that entry silently changes FromHand into
            // FromHandForDiscard in otherwise identical manual-choice traces.
            harmony.Patch(AccessTools.Method(machine, "MoveNext"),
                prefix: new HarmonyMethod(typeof(Headless), nameof(MarkHandSelection)),
                postfix: new HarmonyMethod(typeof(Headless), nameof(ObserveHandSelection)));
        }
        catch
        {
            harmony.UnpatchAll(harmony.Id);
            _handSelectionMarker = null;
            _handSelectionObserver = null;
            throw;
        }
        return () =>
        {
            harmony.UnpatchAll(harmony.Id);
            _handSelectionMarker = null;
            _handSelectionObserver = null;
        };
    }

    private static void MarkHandSelection(object __instance, out long? __state)
        => __state = _handSelectionMarker?.Invoke((Player)_handSelectionPlayer!.GetValue(__instance)!);

    private static void ObserveHandSelection(object __instance, long? __state)
    {
        if (__state == null) return;
        var builder = (AsyncTaskMethodBuilder<IEnumerable<CardModel>>)_handSelectionBuilder!.GetValue(__instance)!;
        Task<IEnumerable<CardModel>> result = builder.Task;
        if (result.IsFaulted || result.IsCanceled) return;
        if (!result.IsCompletedSuccessfully)
            throw new InvalidOperationException("Offline hand selection observation requires a completed original Task");
        _handSelectionObserver?.Invoke((Player)_handSelectionPlayer!.GetValue(__instance)!,
            (CardSelectorPrefs)_handSelectionPrefs!.GetValue(__instance)!, result.Result.ToList(), __state.Value);
    }

    // Trial abandonment with TestMode=false: remove only screen closing,
    // popup disposal, music access/playback and the game-over view call.
    // Keep AbandonInternal's IsAbandoned write, GuaranteeKillAllPlayers,
    // CreatureCmd.Kill(force:true), all death hooks and RunManager.OnEnded.
    // Exact call counts reject DLL drift; labels/exception blocks are retained.
    public static Action InstallAbandonPresentation()
    {
        var harmony = new Harmony("sts2sim.headless.abandon-presentation");
        MethodInfo AsyncBody(MethodInfo method) => AccessTools.Method(
            method.GetCustomAttribute<AsyncStateMachineAttribute>()!.StateMachineType, "MoveNext");
        harmony.Patch(AsyncBody(AccessTools.Method(typeof(MegaCrit.Sts2.Core.Runs.RunManager), "AbandonInternal")),
            transpiler: new HarmonyMethod(typeof(Headless), nameof(AbandonPresentationCalls)));
        harmony.Patch(AsyncBody(AccessTools.Method(typeof(CreatureCmd), nameof(CreatureCmd.Kill),
            new[] { typeof(IReadOnlyCollection<Creature>), typeof(bool) })),
            transpiler: new HarmonyMethod(typeof(Headless), nameof(AbandonPresentationCalls)));
        foreach (string name in new[] { "OnYesButtonPressed", "OnNoButtonPressed" })
            harmony.Patch(AccessTools.Method(typeof(MegaCrit.Sts2.Core.Nodes.CommonUi.NAbandonRunConfirmPopup), name),
                transpiler: new HarmonyMethod(typeof(Headless), nameof(AbandonPresentationCalls)));
        return () => harmony.UnpatchAll(harmony.Id);
    }

    private static IEnumerable<CodeInstruction> AbandonPresentationCalls(IEnumerable<CodeInstruction> code, MethodBase original)
    {
        int count = 0;
        foreach (CodeInstruction ins in code)
        {
            bool skip = ins.operand is MethodInfo method && (ins.opcode == OpCodes.Call || ins.opcode == OpCodes.Callvirt)
                && (method.DeclaringType?.FullName, method.Name) is
                    ("MegaCrit.Sts2.Core.Nodes.Screens.Capstones.NCapstoneContainer", "Close") or
                    ("MegaCrit.Sts2.Core.Nodes.Screens.Map.NMapScreen", "Close") or
                    ("MegaCrit.Sts2.Core.Nodes.NRun", "get_RunMusicController") or
                    ("MegaCrit.Sts2.Core.Nodes.Audio.NRunMusicController", "StopMusic") or
                    ("MegaCrit.Sts2.Core.Nodes.Audio.NAudioManager", "PlayMusic") or
                    ("MegaCrit.Sts2.Core.Nodes.NRun", "ShowGameOverScreen") or
                    ("MegaCrit.Sts2.Core.Helpers.GodotTreeExtensions", "QueueFreeSafely");
            if (!skip) { yield return ins; continue; }
            var call = (MethodInfo)ins.operand;
            int pops = call.GetParameters().Length + (call.IsStatic ? 0 : 1);
            if (pops == 0 || (call.ReturnType != typeof(void) && call.ReturnType.IsValueType))
                throw new InvalidOperationException("Unsupported abandonment presentation signature: " + call);
            var first = new CodeInstruction(ins) { opcode = OpCodes.Pop, operand = null };
            yield return first;
            for (int i = 1; i < pops; i++) yield return new CodeInstruction(OpCodes.Pop);
            if (call.ReturnType != typeof(void)) yield return new CodeInstruction(OpCodes.Ldnull);
            count++;
        }
        int expected = original.Name == "MoveNext"
            ? original.DeclaringType!.Name.Contains("AbandonInternal", StringComparison.Ordinal) ? 2 : 4 : 1;
        if (count != expected) throw new InvalidOperationException($"Abandon presentation calls in {original}: {count}, expected {expected}");
    }

    private static bool _installed;

    [DllImport("libc", SetLastError = true)]
    private static extern IntPtr mmap(IntPtr addr, UIntPtr length, int prot, int flags, int fd, IntPtr offset);

    [DllImport("libc", SetLastError = true)]
    private static extern int mprotect(IntPtr addr, UIntPtr length, int prot);

    public static void Install()
    {
        if (_installed) return;
        _installed = true;
        InstallNativeStubs();
        SeedEngineSingletons();
        var harmony = new Harmony("sts2sim.headless");

        // Logger's static initializer asks the engine for its command line.
        harmony.Patch(
            AccessTools.Method(typeof(Logger), "GetIsRunningFromGodotEditor"),
            prefix: new HarmonyMethod(typeof(Headless), nameof(NotInEditor)));

        // Log output goes through GD.Print.
        harmony.Patch(
            AccessTools.Method(typeof(ConsoleLogPrinter), nameof(ConsoleLogPrinter.Print)),
            prefix: new HarmonyMethod(typeof(Headless), nameof(PrintToStderr)));

        // TalkCmd.Play only formats localized text, reads the fast-mode preference,
        // creates a speech-bubble VFX node, and attaches it to the speaker. It does
        // not mutate combat state or consume RNG, so headless oracle runs can skip
        // this presentation-only command and return the same completed result.
        harmony.Patch(
            AccessTools.Method(typeof(TalkCmd), nameof(TalkCmd.Play),
                new[] { typeof(MegaCrit.Sts2.Core.Localization.LocString), typeof(MegaCrit.Sts2.Core.Entities.Creatures.Creature),
                        typeof(VfxColor), typeof(VfxDuration) }),
            prefix: new HarmonyMethod(typeof(Headless), nameof(SkipTalkPlay)));

        // PandorasBox.AfterObtained has already completed the original bulk
        // CardCmd.Transform, all pile/obtain hooks and Niche RNG before calling
        // this inspection-only view. ShowScreen only instantiates card nodes,
        // opens the capstone and plays audio; it has no selection or rule result.
        // Return the same null view accepted by its sole caller, without enabling
        // TestMode or replacing any transformation/relic/event implementation.
        harmony.Patch(
            AccessTools.Method(typeof(MegaCrit.Sts2.Core.Nodes.Screens.NSimpleCardsViewScreen), "ShowScreen",
                new[] { typeof(List<CardPileAddResult>), typeof(MegaCrit.Sts2.Core.Localization.LocString) }),
            prefix: new HarmonyMethod(typeof(Headless), nameof(NoSimpleCardsView)));

        // These event handlers call audio/rumble singletons without null guards.
        // Replace only the presentation calls inside their async state machines;
        // their arguments, waits, rewards, RNG draws and event transitions remain
        // the original game code. Play's pitch RNG is Chaotic (presentation only).
        foreach (var handler in new[] {
            AccessTools.Method(typeof(JungleMazeAdventure), "SafetyInNumbers"),
            AccessTools.Method(typeof(DenseVegetation), "Rest"),
            AccessTools.Method(typeof(PunchOff), "Nab") })
        {
            Type machine = handler.GetCustomAttribute<AsyncStateMachineAttribute>()!.StateMachineType;
            harmony.Patch(AccessTools.Method(machine, "MoveNext"),
                transpiler: new HarmonyMethod(typeof(Headless), nameof(EventPresentationCalls)));
        }

        // The game's file layer (Godot FileAccess) would read garbage out of the
        // stubs: present an empty, read-only file system instead.
        foreach (MethodInfo m in typeof(GodotFileIo).GetMethods(BindingFlags.Instance | BindingFlags.Public | BindingFlags.DeclaredOnly))
        {
            string? prefix = m.ReturnType switch
            {
                Type t when t == typeof(void) => nameof(SkipVoid),
                Type t when t == typeof(bool) => nameof(ReturnFalse),
                Type t when t == typeof(string) => nameof(ReturnNullString),
                Type t when t == typeof(string[]) => nameof(ReturnNoStrings),
                Type t when t == typeof(int) => nameof(ReturnZero),
                Type t when t == typeof(DateTimeOffset) => nameof(ReturnEpoch),
                Type t when t == typeof(System.Threading.Tasks.Task) => nameof(ReturnCompletedTask),
                Type t when t == typeof(System.Threading.Tasks.Task<string?>) => nameof(ReturnNullStringTask),
                _ => null,
            };
            if (prefix == null)
                throw new InvalidOperationException("Headless: unhandled GodotFileIo." + m.Name + " returning " + m.ReturnType);
            if (m.Name == nameof(GodotFileIo.GetFullPath)) continue;  // pure string logic
            harmony.Patch(m, prefix: new HarmonyMethod(typeof(Headless), prefix));
        }
    }

    // Scoped presentation support for live-root differential fixtures with
    // TestMode off. No gameplay method is replaced. The caller restores the
    // patches when its native combat ends, leaving other oracle tools unchanged.
    public static Action InstallLiveCombatPresentation()
    {
        var harmony = new Harmony("sts2sim.headless.live-combat-presentation");
        var patches = new List<(MethodInfo Original, MethodInfo Patch)>();
        void Patch(MethodInfo original, string name, bool postfix = false)
        {
            MethodInfo replacement = AccessTools.Method(typeof(Headless), name);
            harmony.Patch(original, prefix: postfix ? null : new HarmonyMethod(replacement),
                postfix: postfix ? new HarmonyMethod(replacement) : null);
            patches.Add((original, replacement));
        }
        void Restore()
        {
            foreach (var (original, replacement) in patches) harmony.Unpatch(original, replacement);
            patches.Clear();
        }
        try
        {
            // Only creates/moves an NCard node and updates its visuals. Original
            // Add/Draw/AddDuringManualCardPlay pile mutations/hooks/events run.
            Patch(AccessTools.Method(typeof(CardPileCmd), "CreateCardNodeAndUpdateVisuals"), nameof(NoCardNode));
            // Only creates shuffle visuals/audio. StableShuffle, Add and
            // AfterShuffle still run in CardPileCmd.Shuffle. Visual RNG is Chaotic.
            Patch(AccessTools.Method(typeof(NCardFlyShuffleVfx), nameof(NCardFlyShuffleVfx.Create)), nameof(NoShuffleVfx));
            // Only resolves screen coordinates and creates damage-number VFX.
            // Damage, history and hit hooks run in CreatureCmd.Damage. Random
            // visual offsets use Rng.Chaotic, not combat/run/player RNG streams.
            Patch(AccessTools.Method(typeof(NDamageNumVfx), nameof(NDamageNumVfx.Create),
                new[] { typeof(Creature), typeof(int), typeof(bool) }), nameof(NoDamageVfx));
            // Create() only instantiates the low-HP border scene; its caller
            // accepts null. Damage, HP events, Poison and death/run-end logic
            // remain original. Used by TestMode=false player-poison timing
            // observations; no gameplay RNG is read by this factory.
            Patch(AccessTools.Method(typeof(NLowHpBorderVfx), nameof(NLowHpBorderVfx.Create)), nameof(NoLowHpBorder));
            // Run the original helper, including fromSilentAdd pile events.
            // With no nodes, clear only its animation-wait result so Shuffle
            // does not query the nonexistent SceneTree viewport for frame time.
            Patch(AccessTools.Method(typeof(CardPileCmd), nameof(CardPileCmd.GetTweenForCardsChangingPiles),
                new[] { typeof(IEnumerable<CardPileAddResult>), typeof(bool) }), nameof(NoPileAnimationWait), postfix: true);
            return Restore;
        }
        catch { Restore(); throw; }
    }

    private static bool NoCardNode(ref NCard? __result) { __result = null; return false; }
    private static bool NoSimpleCardsView(ref MegaCrit.Sts2.Core.Nodes.Screens.NCardsViewScreen? __result)
        { __result = null; return false; }
    private static bool NoShuffleVfx(ref NCardFlyShuffleVfx? __result) { __result = null; return false; }
    private static bool NoDamageVfx(ref NDamageNumVfx? __result) { __result = null; return false; }
    private static bool NoLowHpBorder(ref NLowHpBorderVfx? __result) { __result = null; return false; }
    private static void NoPileAnimationWait(ref (Godot.Tween?, bool) __result)
    {
        if (__result.Item1 is not null) throw new InvalidOperationException("headless_live_combat_unexpected_tween");
        __result = (null, false);
    }

    // Only used by the explicit Env-operation TestMode=false oracle. RunManager
    // FadeIn/Out access NGame.Transition; ClearScreens closes overlay/map nodes.
    // Preload's two methods update asset caches and await Godot resources. None
    // mutates a run/player/room model or reads deterministic gameplay RNG.
    // Room entry, map travel, purchases, selectors, hooks and saves run original.
    public static Action InstallEnvOperationPresentation()
    {
        var harmony = new Harmony("sts2sim.headless.env-operation-presentation");
        var patches = new List<(MethodInfo Original, MethodInfo Patch)>();
        void Patch(MethodInfo original, string name)
        {
            MethodInfo replacement = AccessTools.Method(typeof(Headless), name);
            harmony.Patch(original, prefix: new HarmonyMethod(replacement));
            patches.Add((original, replacement));
        }
        void Restore()
        {
            foreach (var (original, replacement) in patches) harmony.Unpatch(original, replacement);
            patches.Clear();
        }
        try
        {
            var manager = typeof(MegaCrit.Sts2.Core.Runs.RunManager);
            var preload = typeof(MegaCrit.Sts2.Core.Assets.PreloadManager);
            Patch(AccessTools.Method(manager, "FadeIn"), nameof(ReturnCompletedTask));
            Patch(AccessTools.Method(manager, "FadeOut"), nameof(ReturnCompletedTask));
            Patch(AccessTools.Method(manager, "ClearScreens"), nameof(SkipVoid));
            Patch(AccessTools.Method(preload, "LoadActAssets"), nameof(ReturnCompletedTask));
            Patch(AccessTools.Method(preload, "LoadRoomAssets"), nameof(ReturnCompletedTask));
            // Event asset loads and NEventRoom.Create only create/cache the
            // scene. Preserve EventRoom.EnterInternal, BeginEvent, hooks and
            // original event callbacks; this is used by Env Crystal fixtures.
            Patch(AccessTools.Method(preload, "LoadRoomEventAssets"), nameof(ReturnCompletedTask));
            Patch(AccessTools.Method(typeof(MegaCrit.Sts2.Core.Nodes.Rooms.NEventRoom), "Create"), nameof(NoEventRoomNode));
            // MapRoom.EnterInternal contains only its reconstruction guard and
            // NMapRoom.Create/NRun.SetCurrentRoom. Keep the guard exactly;
            // suppress the view creation, not RunManager's room/act hooks.
            Patch(AccessTools.Method(typeof(MegaCrit.Sts2.Core.Rooms.MapRoom), "EnterInternal"), nameof(NoMapRoomView));
            Patch(AccessTools.Method(typeof(MegaCrit.Sts2.Core.Nodes.Cards.NCard), "Create"), nameof(NoCardNode));
            Patch(AccessTools.Method(typeof(MegaCrit.Sts2.Core.Multiplayer.Replay.CombatReplayWriter), "WriteReplay"), nameof(SkipVoid));
            return Restore;
        }
        catch { Restore(); throw; }
    }

    private static bool NoEventRoomNode(ref MegaCrit.Sts2.Core.Nodes.Rooms.NEventRoom __result)
    {
        __result = null!;
        return false;
    }

    // TestMode=false reward screen input adapter. ShowScreen creates a Godot
    // overlay and UI buttons only; RewardsSet.Offer has already generated and
    // begun the native synchronizer task. Drive the supplied selector through
    // that same synchronizer rather than replacing Offer or reward logic.
    public static Action InstallEnvRewardsPresentation()
    {
        var harmony = new Harmony("sts2sim.headless.env-reward-input");
        var original = AccessTools.Method(typeof(MegaCrit.Sts2.Core.Nodes.Screens.NRewardsScreen), "ShowScreen");
        var replacement = AccessTools.Method(typeof(Headless), nameof(DriveEnvRewardScreen));
        harmony.Patch(original, prefix: new HarmonyMethod(replacement));
        // A null card-reward screen routes original CardReward.OnSelect to
        // its ICardSelector input branch. Selection IDs, synchronization,
        // OnSelect effects and reward completion still execute in sts2.dll.
        var cardScreen = AccessTools.Method(typeof(MegaCrit.Sts2.Core.Nodes.Screens.CardSelection.NCardRewardSelectionScreen), "ShowScreen");
        var cardInput = AccessTools.Method(typeof(Headless), nameof(NoCardRewardScreen));
        try { harmony.Patch(cardScreen, prefix: new HarmonyMethod(cardInput)); }
        catch { harmony.Unpatch(original, replacement); throw; }
        return () => { harmony.Unpatch(cardScreen, cardInput); harmony.Unpatch(original, replacement); };
    }

    private static bool NoCardRewardScreen(ref MegaCrit.Sts2.Core.Nodes.Screens.CardSelection.NCardRewardSelectionScreen? __result)
    {
        __result = null;
        return false;
    }

    private static bool DriveEnvRewardScreen(MegaCrit.Sts2.Core.Rewards.RewardsSet set,
        ref MegaCrit.Sts2.Core.Nodes.Screens.NRewardsScreen __result)
    {
        var selector = MegaCrit.Sts2.Core.Rewards.RewardsSet.testSelector
            ?? throw new InvalidOperationException("Env reward input requires an explicit selector");
        MegaCrit.Sts2.Core.Helpers.TaskHelper.RunSafely(selector(set));
        __result = null!;
        return false;
    }

    private static bool NoMapRoomView(bool isRestoringRoomStackBase, ref System.Threading.Tasks.Task __result)
    {
        if (isRestoringRoomStackBase) throw new InvalidOperationException("MapRoom does not support room stack reconstruction.");
        __result = System.Threading.Tasks.Task.CompletedTask;
        return false;
    }

    // The oracle runs the game the way the game's own test runner does:
    // TestMode on, which skips every node, tween, VFX, audio and asset path.
    // A few TestMode reads change gameplay rather than presentation; those
    // methods get their production branch back. The full audit of every
    // TestMode read is docs/audits/testmode.md.
    public static void EnableTestMode()
    {
        TestMode.IsOn = true;
        var harmony = new Harmony("sts2sim.headless.testmode");
        foreach (MethodBase m in ProductionBranches)
            harmony.Patch(m, transpiler: new HarmonyMethod(typeof(Headless), nameof(TestModeOff)));
    }

    private static MethodBase[] ProductionBranches => new MethodBase[]
    {
        // Price jitter draws from the Shops rng; TestMode skips the draw.
        AccessTools.Method(typeof(MerchantPotionEntry), nameof(MerchantPotionEntry.CalcCost)),
        // TestMode offers fixed relics / potions instead of rolling rewards.
        AccessTools.Method(typeof(CallingBell), "GenerateRewards"),
        AccessTools.Method(typeof(Cauldron), "GenerateRewards"),
    };

    // Replaces TestMode.IsOn / IsOff reads with the constants of a normal run.
    private static IEnumerable<CodeInstruction> TestModeOff(IEnumerable<CodeInstruction> code, MethodBase original)
    {
        MethodInfo isOn = AccessTools.PropertyGetter(typeof(TestMode), nameof(TestMode.IsOn));
        MethodInfo isOff = AccessTools.PropertyGetter(typeof(TestMode), nameof(TestMode.IsOff));
        var result = new List<CodeInstruction>();
        int replaced = 0;
        foreach (CodeInstruction ins in code)
        {
            bool on = ins.Calls(isOn);
            if (on || ins.Calls(isOff))
            {
                result.Add(new CodeInstruction(on ? OpCodes.Ldc_I4_0 : OpCodes.Ldc_I4_1).MoveLabelsFrom(ins));
                replaced++;
            }
            else result.Add(ins);
        }
        if (replaced == 0)
            throw new InvalidOperationException("Headless: no TestMode read in " + original.DeclaringType?.Name + "." + original.Name);
        return result;
    }

    private static IEnumerable<CodeInstruction> EventPresentationCalls(IEnumerable<CodeInstruction> code)
    {
        MethodInfo play = AccessTools.Method(typeof(NDebugAudioManager), "Play");
        MethodInfo stop = AccessTools.Method(typeof(NDebugAudioManager), "Stop");
        MethodInfo rumble = AccessTools.Method(typeof(NGame), "ScreenRumble");
        MethodInfo shake = AccessTools.Method(typeof(NGame), "ScreenShakeTrauma");
        foreach (CodeInstruction ins in code)
        {
            string? replacement = ins.Calls(play) ? nameof(SilentDebugAudioPlay)
                : ins.Calls(stop) ? nameof(SilentDebugAudioStop)
                : ins.Calls(rumble) ? nameof(SilentScreenRumble)
                : ins.Calls(shake) ? nameof(SilentScreenShakeTrauma) : null;
            if (replacement == null) { yield return ins; continue; }
            // Static helpers take the original receiver as their first argument,
            // preserving the evaluation stack even when the receiver is null.
            ins.opcode = OpCodes.Call;
            ins.operand = AccessTools.Method(typeof(Headless), replacement);
            yield return ins;
        }
    }

    private static int SilentDebugAudioPlay(NDebugAudioManager? receiver, string streamName,
                                             float volume, PitchVariance variance) => 0;
    private static void SilentDebugAudioStop(NDebugAudioManager? receiver, int id, float fadeTime) { }
    private static void SilentScreenRumble(NGame? receiver, ShakeStrength strength,
                                            ShakeDuration duration, RumbleStyle style) { }
    private static void SilentScreenShakeTrauma(NGame? receiver, ShakeStrength strength) { }

    private static bool SkipVoid() => false;

    private static bool ReturnFalse(ref bool __result)
    {
        __result = false;
        return false;
    }

    private static bool ReturnNullString(ref string? __result)
    {
        __result = null;
        return false;
    }

    private static bool ReturnNoStrings(ref string[] __result)
    {
        __result = Array.Empty<string>();
        return false;
    }

    private static bool ReturnZero(ref int __result)
    {
        __result = 0;
        return false;
    }

    private static bool ReturnEpoch(ref DateTimeOffset __result)
    {
        __result = DateTimeOffset.UnixEpoch;
        return false;
    }

    private static bool ReturnCompletedTask(ref System.Threading.Tasks.Task __result)
    {
        __result = System.Threading.Tasks.Task.CompletedTask;
        return false;
    }

    private static bool ReturnNullStringTask(ref System.Threading.Tasks.Task<string?> __result)
    {
        __result = System.Threading.Tasks.Task.FromResult<string?>(null);
        return false;
    }

    private static bool SkipTalkPlay(ref NSpeechBubbleVfx? __result)
    {
        __result = null;
        return false;
    }

    // Fills GodotSharp's NativeFuncs.UnmanagedCallbacks table (normally handed
    // over by the engine at startup) with stubs. Almost every entry clears the
    // integer and SSE return registers and returns; out-parameters are left
    // untouched. Lookups that must not fail (method binds, class constructors,
    // engine singletons) return pointers to a zeroed dummy block instead, and
    // "constructed" engine objects get that block as their native pointer.
    private static unsafe void InstallNativeStubs()
    {
        if (Environment.GetEnvironmentVariable("STS2_NO_NATIVE_STUBS") == "1") return;
        Type nativeFuncs = typeof(Godot.GodotObject).Assembly.GetType("Godot.NativeInterop.NativeFuncs")!;
        Type callbacks = nativeFuncs.GetNestedType("UnmanagedCallbacks", BindingFlags.NonPublic | BindingFlags.Public)!;
        int size = (int)typeof(Unsafe).GetMethod(nameof(Unsafe.SizeOf))!.MakeGenericMethod(callbacks).Invoke(null, null)!;

        const int protRw = 3, protRx = 5, mapPrivateAnon = 0x22;
        var page = (UIntPtr)4096;
        IntPtr code = mmap(IntPtr.Zero, page, protRw, mapPrivateAnon, -1, IntPtr.Zero);
        IntPtr dummy = mmap(IntPtr.Zero, page, protRw, mapPrivateAnon, -1, IntPtr.Zero);
        if (code == new IntPtr(-1) || dummy == new IntPtr(-1))
            throw new InvalidOperationException("mmap failed: " + Marshal.GetLastWin32Error());

        // zero:  xor eax,eax; xor edx,edx; xorps xmm0,xmm0; xorps xmm1,xmm1; ret
        byte[] zero = { 0x31, 0xC0, 0x31, 0xD2, 0x0F, 0x57, 0xC0, 0x0F, 0x57, 0xC9, 0xC3 };
        Marshal.Copy(zero, 0, code, zero.Length);
        // const: movabs rax, imm64; xor edx,edx; ret
        IntPtr EmitConst(int offset, IntPtr value)
        {
            IntPtr at = code + offset;
            Marshal.WriteByte(at, 0x48);
            Marshal.WriteByte(at, 1, 0xB8);
            Marshal.WriteInt64(at, 2, value.ToInt64());
            Marshal.WriteByte(at, 10, 0x31);
            Marshal.WriteByte(at, 11, 0xD2);
            Marshal.WriteByte(at, 12, 0xC3);
            return at;
        }
        IntPtr returnsDummy = EmitConst(64, dummy);
        IntPtr returnsCtor = EmitConst(128, returnsDummy);
        if (mprotect(code, page, protRx) != 0) throw new InvalidOperationException("mprotect failed");

        IntPtr table = Marshal.AllocHGlobal(size);  // intentionally never freed
        for (int off = 0; off + IntPtr.Size <= size; off += IntPtr.Size) Marshal.WriteIntPtr(table, off, code);
        void Bind(string name, IntPtr fn) => Marshal.WriteIntPtr(table, (int)Marshal.OffsetOf(callbacks, name), fn);
        Bind("godotsharp_method_bind_get_method", returnsDummy);
        Bind("godotsharp_method_bind_get_method_with_compatibility", returnsDummy);
        Bind("godotsharp_get_class_constructor", returnsCtor);
        Bind("godotsharp_engine_get_singleton", returnsDummy);
        nativeFuncs.GetMethod("Initialize", BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.Public)!
            .Invoke(null, new object[] { table, size });
    }

    private static IntPtr _dummyObject;

    // Engine singletons (OS, Engine, ProjectSettings, ...) are fetched lazily into
    // a static `singleton` field; without the engine they would come back null.
    // Pre-seed each with an uninitialized wrapper whose native pointer is the
    // dummy block, so instance calls reach the no-op stubs.
    private static void SeedEngineSingletons()
    {
        if (Environment.GetEnvironmentVariable("STS2_NO_NATIVE_STUBS") == "1") return;
        FieldInfo nativePtr = typeof(Godot.GodotObject).GetField("NativePtr", BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public)!;
        foreach (Type t in typeof(Godot.GodotObject).Assembly.GetTypes())
        {
            FieldInfo? f = t.GetField("singleton", BindingFlags.Static | BindingFlags.NonPublic);
            if (f == null || f.IsInitOnly || !typeof(Godot.GodotObject).IsAssignableFrom(f.FieldType)) continue;
            Type instanceType = f.FieldType.IsAbstract ? typeof(Godot.GodotObject) : f.FieldType;
            object obj = RuntimeHelpers.GetUninitializedObject(instanceType);
            nativePtr.SetValue(obj, DummyObject());
            f.SetValue(null, obj);
        }
    }

    private static IntPtr DummyObject()
    {
        if (_dummyObject == IntPtr.Zero) _dummyObject = Marshal.AllocHGlobal(4096);  // never freed
        return _dummyObject;
    }

    private static bool NotInEditor(ref bool __result)
    {
        __result = false;
        return false;
    }

    private static bool PrintToStderr(LogLevel logLevel, string text)
    {
        if (logLevel >= LogLevel.Warn || Environment.GetEnvironmentVariable("STS2EXP_TRACE") == "1")
            Console.Error.WriteLine("[game " + logLevel.ToString().ToUpperInvariant() + "] " + text);
        return false;
    }
}
