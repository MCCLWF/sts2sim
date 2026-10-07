using System.Reflection;
using System.Runtime.CompilerServices;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Monsters;
using MegaCrit.Sts2.Core.MonsterMoves;
using MegaCrit.Sts2.Core.MonsterMoves.MonsterMoveStateMachine;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Unlocks;

internal static class Program {
    static void Main() { var output = Console.Out; Console.SetOut(TextWriter.Null); GameBootstrap.InstallResolver(GameBootstrap.DefaultGameData); Check(output); }
    [MethodImpl(MethodImplOptions.NoInlining)]
    static void Check(TextWriter output) {
        GameBootstrap.Init(testMode:false);
        NonInteractiveMode.AutoSlayerCheck = () => true;
        GameBootstrap.UseInMemorySaves(); GameBootstrap.LoadLocalization();
        Player player = Player.CreateForNewRun<Ironclad>(UnlockState.all,1);
        RunState run = RunState.CreateForNewRun([player],ModelDb.ActsByIndex.Select(a=>a[0].ToMutable()).ToArray(),[],GameMode.Standard,0,"MONSTERHISTORY");
        RunManager.Instance.SetUpNewSingleplayer(run,shouldSave:false);
        player.ResetCombatState(); var combat = new CombatState(runState:run); combat.AddPlayer(player);
        Creature owner = combat.CreateCreature(ModelDb.Monster<Mawler>().ToMutable(),CombatSide.Enemy,null);
        MonsterState[] states = [new MoveState("A", _=>Task.CompletedTask), new MoveState("B", _=>Task.CompletedTask), new ConditionalBranchState("C")];
        var machine = new MonsterMoveStateMachine(states,states[0]);
        typeof(MonsterModel).GetField("_moveStateMachine",BindingFlags.Instance|BindingFlags.NonPublic)!.SetValue(owner.Monster,machine);
        var nativeWeight = typeof(RandomBranchState).GetMethod("GetStateWeight",BindingFlags.Static|BindingFlags.NonPublic)!;
        var rules = new List<(MoveRepeatType repeat,int max)> { (MoveRepeatType.CanRepeatForever,0), (MoveRepeatType.UseOnlyOnce,0), (MoveRepeatType.CannotRepeat,0) };
        foreach(int max in new[]{-1,0,1,2,3,1024,1025,2048}) rules.Add((MoveRepeatType.CanRepeatXTimes,max));
        int[] cooldowns = [0,1,3,1024,1025,2048];
        Console.SetOut(output);
        Console.WriteLine("V v0.111.0 TestMode=false native-private-GetStateWeight");
        void Query() {
            Console.WriteLine($"L {machine.StateLog.Count} {(machine.StateLog.Count==0 ? -1 : Array.IndexOf(states,machine.StateLog.Last()))}");
            for(int target=0; target<states.Length; target++) foreach(var rule in rules) foreach(int cooldown in cooldowns) {
                var weight = new RandomBranchState.StateWeight {stateId=states[target].Id,repeatType=rule.repeat,maxTimes=rule.max,cooldown=cooldown,weightLambda=()=>0.4f};
                float value = (float)nativeWeight.Invoke(null,[weight,owner])!;
                Console.WriteLine($"Q {target} {(int)rule.repeat} {rule.max} {cooldown} {BitConverter.SingleToInt32Bits(value)}");
            }
        }
        for(int fixture=0;fixture<4;fixture++) {
            machine.StateLog.Clear(); Console.WriteLine($"R {fixture}"); Query();
            for(int i=0;i<4097;i++) {
                int entry = fixture switch {0=>0,1=>i%2,2=>i==0?0:1,_=>i%3};
                machine.StateLog.Add(states[entry]); Console.WriteLine($"A {entry} {(states[entry].IsMove?1:0)}");
                int count=i+1;
                if(count<=16 || count%31==0 || count is 1023 or 1024 or 1025 or 2047 or 2048 or 2049 or 4096 or 4097) Query();
            }
        }
        Console.SetOut(TextWriter.Null);
        RunManager.Instance.CleanUp();
    }
}
