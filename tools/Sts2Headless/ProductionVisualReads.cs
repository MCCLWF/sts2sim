// Normal-rule headless oracle: the global TestMode flag stays false.
// Only the V (visual/IO) reads listed in docs/audits/testmode.md are changed.
// G/T/P reads, mod initialization and NonInteractiveMode are never changed.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.CompilerServices;
using HarmonyLib;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.TestSupport;

internal static partial class Headless
{
    private readonly record struct VisualRead(string Type, string Method, int Count);
    private static readonly VisualRead[] ProductionVisualReads =
    {
        new("MegaCrit.Sts2.Core.Entities.Creatures.Creature", "CreateVisuals", 1), // Entities/Creatures/Creature.cs:397
        new("MegaCrit.Sts2.Core.Entities.Creatures.Creature", "GetCreatureNode", 1), // Entities/Creatures/Creature.cs:760
        new("MegaCrit.Sts2.Core.Entities.Creatures.Creature", "GetVfxContainer", 1), // Entities/Creatures/Creature.cs:792
        new("MegaCrit.Sts2.Core.Entities.Creatures.Creature", "GetBackVfxContainer", 1), // Entities/Creatures/Creature.cs:809
        new("MegaCrit.Sts2.Core.Assets.PreloadManager", "LoadMainMenuEssentials", 1), // Assets/PreloadManager.cs:50
        new("MegaCrit.Sts2.Core.Assets.PreloadManager", "LoadMainMenuAssets", 1), // Assets/PreloadManager.cs:72
        new("MegaCrit.Sts2.Core.Assets.PreloadManager", "LoadRunAssets", 1), // Assets/PreloadManager.cs:80
        new("MegaCrit.Sts2.Core.Assets.PreloadManager", "LoadActAssets", 1), // Assets/PreloadManager.cs:92
        new("MegaCrit.Sts2.Core.Assets.PreloadManager", "LoadRoomAssets", 1), // Assets/PreloadManager.cs:137
        new("MegaCrit.Sts2.Core.Assets.PreloadManager", "GetCombatAssetPaths", 1), // Assets/PreloadManager.cs:214
        new("MegaCrit.Sts2.Core.Logging.Logger", "GetIsRunningFromGodotEditor", 1), // Logging/Logger.cs:83
        new("MegaCrit.Sts2.Core.Combat.CombatState", "GodotTimerTask", 1), // Combat/CombatState.cs:605
        new("MegaCrit.Sts2.Core.Localization.Fonts.FontControlUtils", "ApplyLocaleFontSubstitution", 1), // Localization/Fonts/FontControlUtils.cs:13
        new("MegaCrit.Sts2.Core.Rewards.Reward", "CreateIcon", 1), // Rewards/Reward.cs:100
        new("MegaCrit.Sts2.Core.Rewards.PotionReward", "CreateIcon", 1), // Rewards/PotionReward.cs:65
        new("MegaCrit.Sts2.Core.Platform.Null.NullLeaderboardStrategy", "Read", 1), // Platform/Null/NullLeaderboardStrategy.cs:178
        new("MegaCrit.Sts2.Core.Platform.Null.NullLeaderboardStrategy", "Write", 1), // Platform/Null/NullLeaderboardStrategy.cs:197
        new("MegaCrit.Sts2.Core.Localization.LocManager", "SetLanguageInternal", 1), // Localization/LocManager.cs:354
        new("MegaCrit.Sts2.Core.Entities.RestSite.MendRestSiteOption", "OnSelect", 1), // Entities/RestSite/MendRestSiteOption.cs:136
        new("MegaCrit.Sts2.Core.Saves.SaveManager", "IncrementNumReloads", 1), // Saves/SaveManager.cs:301
        new("MegaCrit.Sts2.Core.Runs.RunManager", "InitializeShared", 2), // Runs/RunManager.cs:473,488
        new("MegaCrit.Sts2.Core.Runs.RunManager", "FadeIn", 1), // Runs/RunManager.cs:1033
        new("MegaCrit.Sts2.Core.Runs.RunManager", "FadeOut", 1), // Runs/RunManager.cs:1052
        new("MegaCrit.Sts2.Core.Runs.RunManager", "ClearScreens", 1), // Runs/RunManager.cs:1071
        new("MegaCrit.Sts2.Core.Runs.RunManager", "UpdateRichPresence", 1), // Runs/RunManager.cs:1400
        new("MegaCrit.Sts2.Core.Runs.RunManager", "ReturnToMainMenuWithError", 1), // Runs/RunManager.cs:1780
        new("MegaCrit.Sts2.Core.GameActions.ActionExecutor", "WaitForUnpause", 1), // GameActions/ActionExecutor.cs:235
        new("MegaCrit.Sts2.Core.GameActions.DiscardPotionGameAction", "CancelAction", 1), // GameActions/DiscardPotionGameAction.cs:77
        new("MegaCrit.Sts2.Core.GameActions.MoveToMapCoordAction", "ExecuteAction", 1), // GameActions/MoveToMapCoordAction.cs:37
        new("MegaCrit.Sts2.Core.Commands.CardCmd", "Transform", 2), // Commands/CardCmd.cs:466,488
        new("MegaCrit.Sts2.Core.Commands.CardCmd", "Preview", 1), // Commands/CardCmd.cs:737
        new("MegaCrit.Sts2.Core.Commands.CardCmd", "PreviewCardPileAdd", 2), // Commands/CardCmd.cs:749,757
        new("MegaCrit.Sts2.Core.Commands.CardCmd", "PreviewInternal", 1), // Commands/CardCmd.cs:780
        new("MegaCrit.Sts2.Core.GameActions.UsePotionAction", "CancelAction", 1), // GameActions/UsePotionAction.cs:139
        new("MegaCrit.Sts2.Core.Commands.MapCmd", "SetBossEncounter", 1), // Commands/MapCmd.cs:14
        new("MegaCrit.Sts2.Core.Commands.ForgeCmd", "PreviewSovereignBlade", 1), // Commands/ForgeCmd.cs:90
        new("MegaCrit.Sts2.Core.Commands.CreatureCmd", "Heal", 1), // Commands/CreatureCmd.cs:788
        new("MegaCrit.Sts2.Core.Commands.CreatureCmd", "TriggerAnim", 1), // Commands/CreatureCmd.cs:972
        new("MegaCrit.Sts2.Core.Commands.CardPileCmd", "RemoveFromDeck", 1), // Commands/CardPileCmd.cs:74
        new("MegaCrit.Sts2.Core.Commands.CardPileCmd", "GetTweenForCardsChangingPiles", 1), // Commands/CardPileCmd.cs:561
        new("MegaCrit.Sts2.Core.Commands.CardPileCmd", "AddDuringManualCardPlay", 1), // Commands/CardPileCmd.cs:841
        new("MegaCrit.Sts2.Core.Models.PotionModel", "OnUseWrapper", 1), // Models/PotionModel.cs:304
        new("MegaCrit.Sts2.Core.Commands.VfxCmd", "PlayFullScreenInCombat", 1), // Commands/VfxCmd.cs:116
        new("MegaCrit.Sts2.Core.Commands.VfxCmd", "GetSideCenter", 1), // Commands/VfxCmd.cs:131
        new("MegaCrit.Sts2.Core.Commands.VfxCmd", "GetSideCenterFloor", 1), // Commands/VfxCmd.cs:160
        new("MegaCrit.Sts2.Core.Commands.VfxCmd", "PlayOnSide", 1), // Commands/VfxCmd.cs:194
        new("MegaCrit.Sts2.Core.Commands.VfxCmd", "PlayOnCreatureCenter", 1), // Commands/VfxCmd.cs:214
        new("MegaCrit.Sts2.Core.Commands.VfxCmd", "PlayOnCreature", 1), // Commands/VfxCmd.cs:234
        new("MegaCrit.Sts2.Core.Commands.VfxCmd", "PlayVfx", 1), // Commands/VfxCmd.cs:246
        new("MegaCrit.Sts2.Core.Commands.VfxCmd", "PlayNonCombatVfx", 1), // Commands/VfxCmd.cs:257
        new("MegaCrit.Sts2.Core.Models.Potions.FoulPotion", "ShowPotionVfx", 1), // Models/Potions/FoulPotion.cs:128
        new("MegaCrit.Sts2.Core.Rooms.MerchantRoom", "Exit", 1), // Rooms/MerchantRoom.cs:74
        new("MegaCrit.Sts2.Core.Models.MonsterModel", "PerformMove", 1), // Models/MonsterModel.cs:444
        new("MegaCrit.Sts2.Core.Rooms.MapRoom", "EnterInternal", 1), // Rooms/MapRoom.cs:24
        new("MegaCrit.Sts2.Core.Models.EventModel", "GetAssetPaths", 1), // Models/EventModel.cs:417
        new("MegaCrit.Sts2.Core.Models.Events.DollRoom", "BeforeEventStarted", 1), // Models/Events/DollRoom.cs:96
        new("MegaCrit.Sts2.Core.Multiplayer.Game.ChecksumTracker", "CompareChecksums", 1), // Multiplayer/Game/ChecksumTracker.cs:205
        new("MegaCrit.Sts2.Core.Multiplayer.Game.ChecksumTracker", "LogStateDivergence", 1), // Multiplayer/Game/ChecksumTracker.cs:224
        new("MegaCrit.Sts2.Core.Models.Monsters.Flyconid", "VulnerableSporesMove", 1), // Models/Monsters/Flyconid.cs:54
        new("MegaCrit.Sts2.Core.Models.Monsters.Flyconid", "FrailSporesMove", 1), // Models/Monsters/Flyconid.cs:65
        new("MegaCrit.Sts2.Core.Models.Powers.SandpitPower", "AfterApplied", 1), // Models/Powers/SandpitPower.cs:60
        new("MegaCrit.Sts2.Core.Models.Powers.SandpitPower", "AfterPowerAmountChanged", 1), // Models/Powers/SandpitPower.cs:80
        new("MegaCrit.Sts2.Core.Models.Powers.SandpitPower", "AfterRemoved", 2), // Models/Powers/SandpitPower.cs:97,114
        new("MegaCrit.Sts2.Core.Models.Powers.SandpitPower", "UpdateCreaturePositions", 1), // Models/Powers/SandpitPower.cs:149
        new("MegaCrit.Sts2.Core.Models.Monsters.FuzzyWurmCrawler", "AcidGoop", 1), // Models/Monsters/FuzzyWurmCrawler.cs:64
        new("MegaCrit.Sts2.Core.Models.Monsters.LeafSlimeM", "StickyShotMove", 1), // Models/Monsters/LeafSlimeM.cs:53
        new("MegaCrit.Sts2.Core.Models.Cards.BouncingFlask", "OnPlay", 1), // Models/Cards/BouncingFlask.cs:48
        new("MegaCrit.Sts2.Core.Models.Monsters.BowlbugSilk", "SpitMove", 1), // Models/Monsters/BowlbugSilk.cs:64
        new("MegaCrit.Sts2.Core.Models.Monsters.Noisebot", "AfterAddedToRoom", 1), // Models/Monsters/Noisebot.cs:34
        new("MegaCrit.Sts2.Core.Models.Monsters.Stabbot", "AfterAddedToRoom", 1), // Models/Monsters/Stabbot.cs:32
        new("MegaCrit.Sts2.Core.Models.Monsters.Myte", "ToxicMove", 1), // Models/Monsters/Myte.cs:66
        new("MegaCrit.Sts2.Core.Models.Monsters.KinFollower", "BoomerangMove", 1), // Models/Monsters/KinFollower.cs:137
        new("MegaCrit.Sts2.Core.Models.Powers.InfestedPower", "RevealWrigglersAfterDeathAnim", 1), // Models/Powers/InfestedPower.cs:41
        new("MegaCrit.Sts2.Core.Models.Monsters.WaterfallGiant", "IncrementBuildUpAnimationTrack", 1), // Models/Monsters/WaterfallGiant.cs:318
        new("MegaCrit.Sts2.Core.Models.Monsters.TwigSlimeM", "StickyShotMove", 1), // Models/Monsters/TwigSlimeM.cs:57
        new("MegaCrit.Sts2.Core.Models.Monsters.ToughEgg", "AfterAddedToRoom", 1), // Models/Monsters/ToughEgg.cs:126
        new("MegaCrit.Sts2.Core.Models.Monsters.BygoneEffigy", "WakeMove", 1), // Models/Monsters/BygoneEffigy.cs:72
        new("MegaCrit.Sts2.Core.Models.Monsters.BygoneEffigy", "SlashMove", 1), // Models/Monsters/BygoneEffigy.cs:84
        new("MegaCrit.Sts2.Core.Models.Monsters.SnappingJaxfruit", "EnergyOrb", 1), // Models/Monsters/SnappingJaxfruit.cs:55
        new("MegaCrit.Sts2.Core.Models.Monsters.Zapbot", "AfterAddedToRoom", 1), // Models/Monsters/Zapbot.cs:32
        new("MegaCrit.Sts2.Core.Models.Monsters.Guardbot", "AfterAddedToRoom", 1), // Models/Monsters/Guardbot.cs:30
        new("MegaCrit.Sts2.Core.Models.Monsters.DecimillipedeSegment", "AnimSegmentsAttack", 1), // Models/Monsters/DecimillipedeSegment.cs:224
        new("MegaCrit.Sts2.Core.Models.Monsters.DecimillipedeSegment", "AfterDeath", 1), // Models/Monsters/DecimillipedeSegment.cs:247
        new("MegaCrit.Sts2.Core.Models.Monsters.Tunneler", "BelowMove", 1), // Models/Monsters/Tunneler.cs:108
        new("MegaCrit.Sts2.Core.Models.Monsters.TorchHeadAmalgam", "OnDieToDoom", 1), // Models/Monsters/TorchHeadAmalgam.cs:71
        new("MegaCrit.Sts2.Core.Models.Monsters.Vantom", "InkBlotMove", 1), // Models/Monsters/Vantom.cs:138
        new("MegaCrit.Sts2.Core.Models.Monsters.Vantom", "InkyLanceMove", 1), // Models/Monsters/Vantom.cs:158
        new("MegaCrit.Sts2.Core.Models.Monsters.Vantom", "DismemberMove", 1), // Models/Monsters/Vantom.cs:172
        new("MegaCrit.Sts2.Core.Models.Monsters.Vantom", "PrepareMove", 1), // Models/Monsters/Vantom.cs:196
        new("MegaCrit.Sts2.Core.Models.Monsters.SlumberingBeetle", "RolloutMove", 1), // Models/Monsters/SlumberingBeetle.cs:131
        new("MegaCrit.Sts2.Core.Models.Monsters.MagiKnight", "MagicBombMove", 1), // Models/Monsters/MagiKnight.cs:116
        new("MegaCrit.Sts2.Core.Models.Relics.LordsParasol", "PurchaseEverything", 1), // Models/Relics/LordsParasol.cs:40
        new("MegaCrit.Sts2.Core.Models.Cards.Nightmare", "OnPlay", 1), // Models/Cards/Nightmare.cs:35
        new("MegaCrit.Sts2.Core.Models.Cards.HandOfGreed", "OnPlay", 1), // Models/Cards/HandOfGreed.cs:51
    };
    private static readonly Dictionary<MethodBase, int> VisualReadCounts = new();

    public static Action InstallProductionVisualReads()
    {
        if (TestMode.IsOn) throw new InvalidOperationException("Production headless requires TestMode=false");
        var harmony = new Harmony("sts2sim.headless.production-visual-reads");
        try
        {
            foreach (VisualRead entry in ProductionVisualReads)
            {
                Type type = typeof(AbstractModel).Assembly.GetType(entry.Type, throwOnError: true)!;
                IEnumerable<MethodBase> methods = entry.Method == ".ctor"
                    ? type.GetConstructors(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static)
                    : type.GetMethods(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static | BindingFlags.DeclaredOnly)
                        .Where(m => m.Name == entry.Method);
                int found = 0;
                foreach (MethodBase method in methods)
                {
                    List<MethodBase> bodies = new() { method };
                    Type? machine = method.GetCustomAttribute<AsyncStateMachineAttribute>()?.StateMachineType
                        ?? method.GetCustomAttribute<IteratorStateMachineAttribute>()?.StateMachineType;
                    if (machine != null) bodies.Add(AccessTools.Method(machine, "MoveNext"));
                    foreach (MethodBase body in bodies)
                    {
                        int count = PatchProcessor.GetOriginalInstructions(body).Count(IsTestModeRead);
                        if (count == 0) continue;
                        if (VisualReadCounts.ContainsKey(body)) throw new InvalidOperationException("Duplicate visual body: " + body);
                        VisualReadCounts[body] = count;
                        found += count;
                        harmony.Patch(body, transpiler: new HarmonyMethod(typeof(Headless), nameof(VisualReadsOnly)));
                    }
                }
                if (found != entry.Count) throw new InvalidOperationException($"Visual-read DLL drift {entry.Type}.{entry.Method}: {found}, expected {entry.Count}");
            }
            // SelfHelpBook already applies the real enchantment before this
            // factory. It creates a card/tween/audio view only; its caller
            // accepts null. It has no gameplay callback or seeded RNG draw.
            harmony.Patch(AccessTools.Method(typeof(MegaCrit.Sts2.Core.Nodes.Vfx.NCardEnchantVfx), "Create"),
                prefix: new HarmonyMethod(typeof(Headless), nameof(NoCardEnchantVfx)));
            // Stun's move-state mutation/wrapper remains original. This node
            // only displays a stun label above the creature; the caller accepts null.
            harmony.Patch(AccessTools.Method(typeof(MegaCrit.Sts2.Core.Nodes.Vfx.NStunnedVfx), "Create"),
                prefix: new HarmonyMethod(typeof(Headless), nameof(NoStunnedVfx)));
            // ThinkCmd only formats a thought bubble and attaches its view.
            // A full hand invokes it inside Add; a missing view must not
            // interrupt Add's remaining pile events or the played card.
            harmony.Patch(AccessTools.Method(typeof(MegaCrit.Sts2.Core.Commands.ThinkCmd), "Play"),
                prefix: new HarmonyMethod(typeof(Headless), nameof(SkipVoid)));
            // Serpent Form's game effect is tracked in its own Data amounts.
            // This factory attaches only its overlay; AfterRemoved/OnPlay
            // accept a null Vfx. No damage callback lives in this factory.
            harmony.Patch(AccessTools.Method(typeof(MegaCrit.Sts2.Core.Nodes.Vfx.Forms.NSerpentFormVfx), "Create"),
                prefix: new HarmonyMethod(typeof(Headless), nameof(NoSerpentFormVfx)));
            MethodInfo notification = AccessTools.Method(typeof(MegaCrit.Sts2.Core.Combat.CombatStateTracker), "CallCombatStateChangedDeferred");
            MethodInfo notificationBody = AccessTools.Method(notification.GetCustomAttribute<AsyncStateMachineAttribute>()!.StateMachineType, "MoveNext");
            harmony.Patch(notificationBody, transpiler: new HarmonyMethod(typeof(Headless), nameof(DeferredNotificationPlayer)));
            return () => { harmony.UnpatchAll(harmony.Id); VisualReadCounts.Clear(); };
        }
        catch { harmony.UnpatchAll(harmony.Id); VisualReadCounts.Clear(); throw; }
    }

    private static bool NoCardEnchantVfx(ref MegaCrit.Sts2.Core.Nodes.Vfx.NCardEnchantVfx? __result)
    { __result = null; return false; }
    private static bool NoStunnedVfx(ref MegaCrit.Sts2.Core.Nodes.Vfx.NStunnedVfx? __result)
    { __result = null; return false; }
    private static bool NoSerpentFormVfx(ref MegaCrit.Sts2.Core.Nodes.Vfx.Forms.NSerpentFormVfx? __result)
    { __result = null; return false; }

    // With a real scene tree the notification waits for a frame, by which
    // time combat cleanup can clear its tracked state. Without a scene tree
    // it can observe only enemies during player removal. Keep original
    // RecalculateCardValues and subscribers when a local player exists;
    // an absent player has no cards to recalculate. No game effect is skipped.
    private static MegaCrit.Sts2.Core.Entities.Players.Player? NotificationPlayer(MegaCrit.Sts2.Core.Combat.ICombatState state)
        => state.Creatures.Any(c => MegaCrit.Sts2.Core.Context.LocalContext.IsMe(c.Player))
            ? MegaCrit.Sts2.Core.Context.LocalContext.GetMe(state) : null;

    private static IEnumerable<CodeInstruction> DeferredNotificationPlayer(IEnumerable<CodeInstruction> code)
    {
        int count = 0;
        foreach (CodeInstruction instruction in code)
        {
            if (instruction.operand is MethodInfo m && m.DeclaringType == typeof(MegaCrit.Sts2.Core.Context.LocalContext) && m.Name == "GetMe")
            {
                var replacement = new CodeInstruction(instruction)
                    { operand = AccessTools.Method(typeof(Headless), nameof(NotificationPlayer)) };
                yield return replacement;count++;
            }
            else yield return instruction;
        }
        if (count != 1) throw new InvalidOperationException("Deferred UI notification DLL drift");
    }

    private static bool IsTestModeRead(CodeInstruction instruction)
        => instruction.operand is MethodInfo method && method.DeclaringType == typeof(TestMode)
            && method.Name is "get_IsOn" or "get_IsOff";

    private static IEnumerable<CodeInstruction> VisualReadsOnly(IEnumerable<CodeInstruction> code, MethodBase original)
    {
        int count = 0;
        foreach (CodeInstruction instruction in code)
        {
            if (!IsTestModeRead(instruction)) { yield return instruction; continue; }
            MethodInfo getter = (MethodInfo)instruction.operand;
            // Keep labels/exception blocks. Skip just the audited visual branch;
            // effects before/after it execute normally with TestMode=false.
            var replacement = new CodeInstruction(instruction)
                { opcode = getter.Name == "get_IsOn" ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0, operand = null };
            yield return replacement;
            count++;
        }
        if (count != VisualReadCounts[original]) throw new InvalidOperationException("Visual-read transpiler drift: " + original);
    }
}
