// Sts2Oracle: runs game logic headless and prints ground truth as JSON.
//
//   newrun --seed <SEED> --character <IRONCLAD|...> [--ascension N]
//       New singleplayer run for a fully unlocked profile: the game's own
//       save JSON after setup, plus the generated map of every act.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;

internal static class Program
{
    // Game code (Sentry bootstrap, logging) writes to stdout; results must be
    // the only thing on it.
    public static System.IO.TextWriter Out = Console.Out;

    private static int Main(string[] args)
    {
        Out = Console.Out;
        Console.SetOut(Console.Error);
        var opts = new Dictionary<string, string>();
        string gameData = GameBootstrap.DefaultGameData;
        for (int i = 1; i < args.Length; i++)
        {
            if (!args[i].StartsWith("--", StringComparison.Ordinal)) throw new ArgumentException(args[i]);
            opts[args[i].Substring(2)] = i + 1 < args.Length ? args[++i] : "";
        }
        if (opts.TryGetValue("game-data", out string? gd)) gameData = gd;
        GameBootstrap.InstallResolver(gameData);
        string cmd = args.Length > 0 ? args[0] : "";
        if (cmd == "runtime-provenance")
        {
            // Report the exact external directories this executable will use,
            // after resolving runtime settings but before touching game types.
            // The Python batch tool hashes their contents and freezes them in
            // its manifest; this command intentionally performs no game init.
            var runtime = new
            {
                schema_version = 1,
                game_data = Path.GetFullPath(gameData),
                localization = Path.GetFullPath(Path.Combine(GameBootstrap.DefaultGameSrc, "..", "..", "localization", "eng"))
            };
            Out.WriteLine(JsonSerializer.Serialize(runtime));
            Out.Flush();
            return 0;
        }
        return Oracle.Run(cmd, opts);
    }
}
