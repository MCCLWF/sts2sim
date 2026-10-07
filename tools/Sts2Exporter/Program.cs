// Sts2Exporter: dumps every Slay the Spire 2 content model to JSON.
//
//   dotnet run -c Release -- [--out content.json] [--game-data <dir>] [--src <decompiled src/Core>]
//
// Game assemblies are resolved from the game's data directory at runtime, so no
// copy of sts2.dll is needed next to the tool.
using System;
using System.IO;

internal sealed class Options
{
    public string GameData = GameBootstrap.DefaultGameData;

    public string Out = "content.json";
    public string? Src;
    public bool Ascension = true;

    public static Options Parse(string[] args)
    {
        var o = new Options();
        for (int i = 0; i < args.Length; i++)
        {
            switch (args[i])
            {
                case "--out": o.Out = args[++i]; break;
                case "--game-data": o.GameData = args[++i]; break;
                case "--src": o.Src = args[++i]; break;
                case "--no-ascension": o.Ascension = false; break;
                default: throw new ArgumentException("unknown argument " + args[i]);
            }
        }
        return o;
    }
}

internal static class Program
{
    private static int Main(string[] args)
    {
        Options o = Options.Parse(args);
        GameBootstrap.InstallResolver(o.GameData);
        // Everything touching game types lives in Exporter so that it is only
        // JIT-compiled after the resolver above is installed.
        return Exporter.Run(o);
    }
}
