# Shipped-DLL RNG execution observations

This standalone differential tool observes public scalar `Rng.Next*` methods in the shipped
v0.111.0 DLL using Harmony prefixes/postfixes. It never replaces a result or skips RNG code.
Generic adapters invoke the real DLL `NextItem`, `WeightedNextItem` and `Shuffle` methods;
indexed nullable items encode `default(T)` as the simulator's no-item index, -1.
Fixed seeds cover every scalar overload, Gaussian retries, empty/singleton collections,
zero/mixed weights and full shuffle permutations. TestMode is disabled.

```bash
dotnet build tools/RngExecutionChecks/RngExecutionChecks.csproj -c Release
dotnet tools/RngExecutionChecks/bin/Release/net9.0/RngExecutionChecks.dll > <external-native.jsonl>
cmake --build build --target rng_execution_checks
build/rng_execution_checks <external-native.jsonl>
```

Each NDJSON row contains the DLL SHA256, seed, typed parameter/result bits, complete before/after
RNG states, parent calls, entry/exit order and shuffle results. The C++ checker executes the
native root requests and compares all observed internal calls. Captures stay outside the repo.
The existing GameBootstrap/Headless presentation patches apply; the added patches only observe
RNG. This is a component differential probe, not live gameplay or full RF certification.
