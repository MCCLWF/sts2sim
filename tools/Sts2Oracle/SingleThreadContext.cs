using System;
using System.Collections.Generic;
using System.Threading;

// Stand-in for Godot's main-thread synchronization context: every async
// continuation the game posts runs on the calling thread when Pump() is
// called, in FIFO order, so a headless run is single-threaded and
// deterministic.
internal sealed class SingleThreadContext : SynchronizationContext
{
    private readonly Queue<(SendOrPostCallback, object?)> _queue = new();

    public static SingleThreadContext Install()
    {
        var ctx = new SingleThreadContext();
        SetSynchronizationContext(ctx);
        return ctx;
    }

    public override void Post(SendOrPostCallback d, object? state)
    {
        lock (_queue) _queue.Enqueue((d, state));
    }

    public override void Send(SendOrPostCallback d, object? state) => d(state);

    public override SynchronizationContext CreateCopy() => this;

    // Runs queued continuations until none are left.
    public int Pump()
    {
        int n = 0;
        while (true)
        {
            (SendOrPostCallback, object?) item;
            lock (_queue)
            {
                if (_queue.Count == 0) return n;
                item = _queue.Dequeue();
            }
            item.Item1(item.Item2);
            n++;
        }
    }
}
