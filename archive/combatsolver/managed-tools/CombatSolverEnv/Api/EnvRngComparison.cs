namespace CombatSolverEnv.Api;

// Shared by the native branch adapter and isolated protocol checks. Checking
// counters alone is insufficient: a root can carry an unrelated generator state.
internal static class EnvRngComparison
{
    internal static void RequireMatch(EnvRngState actual, EnvRngState expected,
                                      EnvRunRngStream stream, string boundary)
    {
        if (actual == expected) return;
        string fields = string.Join(",", new[] {
            actual.Counter != expected.Counter ? "counter" : null,
            actual.State0 != expected.State0 ? "s0" : null,
            actual.State1 != expected.State1 ? "s1" : null,
            actual.State2 != expected.State2 ? "s2" : null,
            actual.State3 != expected.State3 ? "s3" : null }.Where(field => field is not null));
        throw new InvalidOperationException($"{boundary}_rng_mismatch:{stream}:{fields};" +
            $"native_counter={expected.Counter};env_counter={actual.Counter}");
    }
}
