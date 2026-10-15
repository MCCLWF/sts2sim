using System.Reflection;
using System.Runtime.InteropServices;

namespace NoslPilot;

// Resolve the packaged library beside this mod, independent of cwd, PATH or Python.
internal static class NativeRuntime
{
    internal static string Directory => Path.GetDirectoryName(typeof(NativeRuntime).Assembly.Location)!;
    private static bool _initialized;

    internal static void Initialize()
    {
        if (_initialized) return;
        NativeLibrary.SetDllImportResolver(typeof(NativeRuntime).Assembly, Resolve);
        _initialized = true;
    }

    private static IntPtr Resolve(string name, Assembly assembly, DllImportSearchPath? searchPath) =>
        name == "libsts2nosl_pilot.so"
            ? NativeLibrary.Load(Path.Combine(Directory, name))
            : IntPtr.Zero;
}
