"""ctypes and gRPC product costs, deliberately reported as different contracts."""
import argparse, gc, json, os, platform, statistics, subprocess, sys, time
from pathlib import Path

def emit(o):print(json.dumps(o),flush=True)
def measure(case,name,op,n=128,warm=256,samples=7):
    sink=0
    for _ in range(warm):sink+=op()
    times=[]
    for _ in range(samples):
        start=time.perf_counter_ns()
        for _ in range(n):sink+=op()
        times.append((time.perf_counter_ns()-start)/n)
    emit(dict(type='measurement',case=case,operation=name,iterations=n,warmup=warm,samples_ns=times,sink=sink))

def ctypes_bench(a):
    os.environ['STS2SIM_ENV_LIB']=str(a.library.resolve());sys.path.insert(0,str(a.native_root/'python'))
    from sts2sim_env import Env,API_DESCRIPTOR
    emit(dict(type='metadata',language='python_ctypes',python=platform.python_version(),descriptor=API_DESCRIPTOR))
    for char,asc,advance in [('SILENT',10,0),('SILENT',10,2),('IRONCLAD',0,0)]:
        root=Env();root.inspect_combat_named('DUAL001','CULTISTS_NORMAL',char,asc)
        end=next(x for x in root.legal_actions() if x.kind==3)  # Public Action::EndTurn ordinal.
        for _ in range(advance):root.step(end)
        case=f'DUAL001-{char}-A{asc}-CULTISTS_NORMAL-T{advance+1}';before=root.state_hash
        emit(dict(type='state',case=case,boundary='root',state=root.export_oracle_combat_state()))
        def clone():
            e=root.clone();e.__del__();return 1
        def transition():
            e=root.clone();e.step(end);e.__del__();return 1
        def predict():
            e=root.predict(end);e.__del__();return 1
        def loop():
            e=root.clone();e.step(end);o=e.observe();actions=e.legal_actions();e.__del__();return len(o)+len(actions)
        measure(case,'Clone',clone)
        measure(case,'Clone_EndTurn',transition)
        measure(case,'Predict_EndTurn',predict)
        measure(case,'Observe',lambda:len(root.observe()))
        measure(case,'LegalActions',lambda:len(root.legal_actions()))
        measure(case,'Hash',lambda:root.state_hash)
        measure(case,'Clone_Step_Observe_LegalActions',loop)
        if char=='IRONCLAD':
            for name,options in [('Preview_trace_off',dict(trace_limit=0)),('Preview_trace_on',dict()),
                 ('Preview_causal_rng',dict(include_rng=True,include_causal=True))]:
                def preview(options=options):
                    e,r=root.preview_with_results(end,**options);size=len(json.dumps(r));e.__del__();return size
                measure(case,name,preview,n=16,warm=8)
            measure(case,'ExportSnapshot_dict',lambda:len(json.dumps(root.export_snapshot())),n=16,warm=8)
            measure(case,'ExportPortableSnapshot_dict',lambda:len(json.dumps(root.export_portable_snapshot())),n=16,warm=8)
        if root.state_hash!=before:raise RuntimeError('source root changed')
        root.__del__()

def grpc_bench(a):
    sys.path.insert(0,str(a.ai_root/'python'))
    import grpc
    from sts2env.proto import env_pb2 as pb,env_pb2_grpc as service
    os.environ['PATH']=str(Path(a.dotnet).parent)+os.pathsep+os.environ['PATH']
    # Keep both endpoints on the same fixed core: actual local process/interface configuration.
    start=time.monotonic();proc=subprocess.Popen([a.dotnet,str(a.server)],stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True)
    port=int(proc.stdout.readline());channel=grpc.insecure_channel(f'127.0.0.1:{port}');grpc.channel_ready_future(channel).result(timeout=30)
    stub=service.SimEnvStub(channel);emit(dict(type='startup',language='csharp_grpc',seconds=time.monotonic()-start,port=port))
    def echo():
        r=stub.Echo(pb.EchoRequest(message='envbench'),timeout=30)
        if r.message!='envbench':raise RuntimeError('Echo mismatch')
        return len(r.message)
    measure('grpc','Echo_roundtrip',echo,n=256,warm=256)
    def reset():
        r=stub.Reset(pb.ResetRequest(seed='GRPCRESETBENCH',ascension_level=10),timeout=30)
        if r.failure_message:raise RuntimeError(r.failure_message)
        return len(r.observation.legal_action_mask)
    measure('grpc','Reset_to_first_strategy_decision',reset,n=16,warm=8)
    # Keep the same server process for the product episode sample below.
    # Product wrapper: strategy controls only non-combat decisions; server search plays combat.
    # RawSts2Env intentionally rejects a known historical candidate/mask mismatch.
    # For a transport benchmark use the protobuf contract directly and record that
    # the strict Gym decoder was not part of this measurement.
    last={}
    try:
        for i in range(a.episodes):
            seed=f'ENVPERFGRPC10{i:04d}';begin=time.perf_counter()
            response=stub.Reset(pb.ResetRequest(seed=seed),timeout=30);last['response']=response
            o=response.observation
            reset_seconds=time.perf_counter()-begin
            # RawSts2Env sends no ascension: the supported wrapper uses server default A10.
            calls=0;latencies=[];decision_types={};terminated=truncated=False
            while not(terminated or truncated) and calls<1000:
                indices=[j for j,x in enumerate(o.legal_action_mask) if x]
                if not indices:raise RuntimeError('empty legal mask')
                decision=int(o.decision_type);decision_types[str(decision)]=decision_types.get(str(decision),0)+1
                start=time.perf_counter();response=stub.Step(pb.StepRequest(action_index=indices[0]),timeout=30);last['response']=response
                o=response.observation;terminated=response.done;truncated=False;latencies.append(time.perf_counter()-start);calls+=1
            response=last['response']
            emit(dict(type='episode',language='csharp_grpc_product',seed=seed,ascension_actual=10,calls=calls,
                seconds=time.perf_counter()-begin,reset_seconds=reset_seconds,terminated=terminated,truncated=truncated,
                episode_end=int(response.episode_end),failure_message=response.failure_message,acts_cleared=response.acts_cleared,won=response.won,
                info={'won':response.won,'seed':response.seed},decision_types=decision_types,step_seconds=latencies))
    finally:
        channel.close(); proc.terminate()
        try:proc.wait(timeout=10)
        except subprocess.TimeoutExpired:proc.kill();proc.wait()

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('mode',choices=['ctypes','grpc']);p.add_argument('--native-root',type=Path)
    p.add_argument('--library',type=Path);p.add_argument('--ai-root',type=Path);p.add_argument('--server',type=Path)
    p.add_argument('--dotnet',default=str(Path.home()/'.dotnet/dotnet'));p.add_argument('--episodes',type=int,default=30)
    a=p.parse_args();ctypes_bench(a) if a.mode=='ctypes' else grpc_bench(a)
