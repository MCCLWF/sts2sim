"""Keep the upstream initializer intact except for the derived Mod identity."""
from pathlib import Path
import sys


def main():
    source = Path(sys.argv[1]).read_text()
    original = 'public const string ModId = "CombatSolver";'
    if source.count(original) != 1:
        raise SystemExit("upstream Mod identity changed; review the derived initializer")
    settings = "        SolverSettings.Load();"
    if source.count(settings) != 1:
        raise SystemExit("upstream settings initialization changed; review the backend wiring")
    source = source.replace(
        settings,
        "        SolverSettings.Load();\n        EnvBackendSettings.Initialize();",
        1)
    output = Path(sys.argv[2])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("#nullable enable\n" + source.replace(original, 'public const string ModId = "CombatSolverEnv";'))


if __name__ == "__main__":
    main()
