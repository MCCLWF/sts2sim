namespace Sts2Env;

public class Sts2EnvException : Exception
{
    public Sts2EnvException(string message, int? nativeStatus = null, Exception? inner = null)
        : base(message, inner) => NativeStatus = nativeStatus;

    public int? NativeStatus { get; }
}

public sealed class NativeEnvironmentException : Sts2EnvException
{
    public NativeEnvironmentException(string operation, int status, string detail)
        : base($"{operation} failed with native status {status}: {detail}", status) { }
}

public sealed class UnportedContentException : Sts2EnvException
{
    public UnportedContentException(string operation, string detail)
        : base($"{operation} reached unported content: {detail}", 1) { }
}

public sealed class TraceLimitExceededException : Sts2EnvException
{
    public TraceLimitExceededException(string operation, string detail)
        : base($"{operation} exceeded the trace capacity: {detail}", 2) { }
}

public sealed class UnsupportedOperationException : Sts2EnvException
{
    public UnsupportedOperationException(string operation, string detail)
        : base($"{operation} is not supported by the native operation contract: {detail}", 3) { }
}

public sealed class OperationCapacityExceededException : Sts2EnvException
{
    public OperationCapacityExceededException(string operation, string detail)
        : base($"{operation} exceeded a native operation capacity: {detail}", 4) { }
}

public sealed class InvalidOperationRequestException : Sts2EnvException
{
    public InvalidOperationRequestException(string operation, string detail)
        : base($"{operation} rejected the typed operation request: {detail}", 5) { }
}

public sealed class AbiVersionMismatchException : Sts2EnvException
{
    public AbiVersionMismatchException(uint expected, uint actual)
        : base($"Unsupported sts2sim C ABI version {actual}; this SDK requires {expected}.")
    {
        ExpectedVersion = expected;
        ActualVersion = actual;
    }

    public uint ExpectedVersion { get; }
    public uint ActualVersion { get; }
}

public sealed class ApiDescriptorException : Sts2EnvException
{
    public ApiDescriptorException(string message, Exception? inner = null) : base(message, null, inner) { }
}

public sealed class SnapshotSessionMismatchException : Sts2EnvException
{
    public SnapshotSessionMismatchException()
        : base("The snapshot belongs to a different loaded native library session.") { }
}
