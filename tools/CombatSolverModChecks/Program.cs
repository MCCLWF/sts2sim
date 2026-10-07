using System.Runtime.CompilerServices;

// Two entry points over the real game assembly:
//   <character>             the fixed live-root fixture (LiveFixture.cs)
//   combat <oracle options> Sts2Oracle's `combat` command with the mod's live
//   run <oracle options>    Env capture attached to every combat decision
//                           (tests/live/live_capture_diff.py)
internal static class Program {
    // Oracle sources print their JSON lines here; game code logs to stdout.
    public static TextWriter Out = Console.Out;

    static int Main(string[] args) {
        GameBootstrap.InstallResolver(GameBootstrap.DefaultGameData);
        if (args.FirstOrDefault() is "combat" or "run") return OracleWithCapture(args);
        Run(args);
        return 0;
    }
    [MethodImpl(MethodImplOptions.NoInlining)] static void Run(string[] args) {
        GameBootstrap.Init(testMode: false);
        GameBootstrap.LoadLocalization();
        GameBootstrap.UseInMemorySaves();
        string? runSave = args.Length == 3 && args[1] == "--run-save" ? args[2] : null;
        if (args.Length > 1 && runSave is null) throw new ArgumentException("<character> [--run-save <path>]");
        LiveFixture.Run(args.FirstOrDefault() ?? "IRONCLAD", runSave);
    }
    [MethodImpl(MethodImplOptions.NoInlining)] static int OracleWithCapture(string[] args) {
        Out = Console.Out;
        Console.SetOut(Console.Error);
        var opts = new Dictionary<string, string>();
        for (int i = 1; i < args.Length; i++) {
            if (!args[i].StartsWith("--", StringComparison.Ordinal)) throw new ArgumentException(args[i]);
            opts[args[i][2..]] = i + 1 < args.Length ? args[++i] : "";
        }
        LiveObserver.Install();
        return Oracle.Run(args[0], opts);
    }
}
