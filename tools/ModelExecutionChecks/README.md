# Synchronous model/Hook execution observer

Run `dotnet build tools/ModelExecutionChecks -c Release`, capture its DLL's stdout
as NDJSON, then pass that unchanged file to `build/model_execution_checks`.
The C++ target is built by CMake. Preserve stdout, stderr, exit codes and hashes.

The observer runs the shipped DLL with TestMode=false. Harmony prefixes/postfixes
only observe three synchronous decimal AbstractModel damage methods and the two
damage Hook functions; no method body or return is replaced. It deliberately
does not use postfix completion on Task-returning methods.

32 explicit damage/flag inputs compare all decimal result bits, ordered model
IDs, actual override/default status, modifier IDs, frame parents and entry/exit
order. Both fixtures create normal starter inventory, empty pre-start combat
piles, a registered Mawler and properly initialized Strength/Vulnerable/Intangible.
This is a component fixture, not arbitrary live capture or whole-game trace
qualification. Model parameters, async completion and object lifetime identity
remain outside this probe's verified scope.
