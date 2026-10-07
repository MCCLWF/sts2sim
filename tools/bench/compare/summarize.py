from pathlib import Path
import argparse, collections, hashlib, json, math, re, statistics

def quantile(xs,p):
    xs=sorted(xs);i=(len(xs)-1)*p;return xs[int(i)]+(xs[min(int(i)+1,len(xs)-1)]-xs[int(i)])*(i-int(i))
def stats(xs):
    return dict(n=len(xs),median=statistics.median(xs),p05=quantile(xs,.05),p95=quantile(xs,.95),
                minimum=min(xs),maximum=max(xs),mean=statistics.mean(xs))
def main():
    p=argparse.ArgumentParser();p.add_argument('results',type=Path);a=p.parse_args();out=a.results
    q=json.loads((out/'qualification.json').read_text());qualified={x['case'] for x in q if x['qualified']}
    indexed=collections.defaultdict(list);alloc=collections.defaultdict(list);processes=[]
    for path in sorted(out.glob('measure-*-rep*.jsonl')):
        lang=path.name.split('-')[1];rows=[json.loads(l) for l in path.read_text().splitlines()]
        for r in rows:
            if r['type']=='measurement':
                indexed[(lang,r['case'],r['operation'])].append(statistics.median(r['samples_ns'])/1000)
                if 'allocated_bytes' in r:alloc[(lang,r['case'],r['operation'])].append(statistics.mean(r['allocated_bytes']))
        stderr=path.with_suffix('.stderr').read_text();rss=re.search(r'Maximum resident set size \(kbytes\): (\d+)',stderr)
        faults=re.search(r'Minor \(reclaiming a frame\) page faults: (\d+)',stderr)
        processes.append(dict(file=path.name,verified=sum(r['type']=='verified' for r in rows),
            failures=[r for r in rows if r['type']=='failure'],max_rss_kib=int(rss[1]) if rss else None,
            minor_faults=int(faults[1]) if faults else None,
            gc_collections=[sum(r.get('gc_collections',[0,0,0])[i] for r in rows if r['type']=='measurement') for i in range(3)]))
    medians={k:statistics.median(v) for k,v in indexed.items() if k[1] in qualified}
    aggregate={}
    for lang,op in sorted({(l,o) for l,c,o in medians}):
        xs=[v for (l,c,o),v in medians.items() if l==lang and o==op];aggregate[f'{lang}/{op}']=stats(xs)
        av=[statistics.median(v) for (l,c,o),v in alloc.items() if l==lang and o==op];
        if av:aggregate[f'{lang}/{op}']['allocated_bytes']=stats(av)
    pairings=[('Clone','Clone'),('Clone_EndTurn','Clone_EndTurn'),('Clone_PlayCard','Clone_PlayCard'),('Clone_EngineEndTurn','Clone_EndTurn')]
    paired={}
    groups={}
    for cpp,cs in pairings:
        rows=[dict(case=c,cpp_us=medians[('cpp',c,cpp)],csharp_us=medians[('csharp',c,cs)],
             ratio_cpp_over_csharp=medians[('cpp',c,cpp)]/medians[('csharp',c,cs)]) for c in sorted(qualified)
             if ('cpp',c,cpp) in medians and ('csharp',c,cs) in medians]
        ratios=[r['ratio_cpp_over_csharp'] for r in rows]
        paired[cpp]=dict(csharp_operation=cs,ratio=stats(ratios),cpp_faster=sum(x<1 for x in ratios),
                        geometric_mean=math.exp(statistics.mean(math.log(x) for x in ratios)),rows=rows)
        for field,column in [('character',2),('act',5),('turn',6),('ascension',3)]:
            subsets=collections.defaultdict(list)
            for item in q:
                c=item['case'];key=item['metadata'][column]
                row=next((r for r in rows if r['case']==c),None)
                if row:subsets[key].append(row)
            for key,rs in subsets.items():
                groups[f'{cpp}/{field}/{key}']=dict(cpp_us=stats([r['cpp_us'] for r in rs]),csharp_us=stats([r['csharp_us'] for r in rs]),
                                                   ratio=stats([r['ratio_cpp_over_csharp'] for r in rs]))
    parallel=collections.defaultdict(list)
    for path in out.glob('parallel-*-rep*.jsonl'):
        lang=path.name.split('-')[1]
        for r in map(json.loads,path.read_text().splitlines()):
            if r['type']=='parallel':parallel[(lang,r['workers'])].append(r)
    parallel_summary={}
    for (lang,w),rs in sorted(parallel.items()):
        throughput=statistics.median(r['ops_per_second'] for r in rs)
        baseline=statistics.median(r['ops_per_second'] for r in parallel[(lang,1)])
        parallel_summary[f'{lang}/{w}']=dict(throughput=stats([r['ops_per_second'] for r in rs]),speedup=throughput/baseline)
    interfaces={}
    for path in out.glob('ctypes-rep*.jsonl'):
        for r in map(json.loads,path.read_text().splitlines()):
            if r['type']=='measurement':interfaces.setdefault(r['case']+'/'+r['operation'],[]).append(statistics.median(r['samples_ns'])/1000)
    interfaces={k:stats(v) for k,v in interfaces.items()}
    grpc=[]
    for path in out.glob('grpc-*.jsonl'):grpc += [json.loads(l) for l in path.read_text().splitlines()]
    episodes=[r for r in grpc if r['type']=='episode'];grpc_summary={}
    if episodes:
        latencies=[x*1000 for r in episodes for x in r['step_seconds']]
        grpc_summary=dict(episodes=len(episodes),outcomes=dict(collections.Counter(r['episode_end'] for r in episodes)),
            failure_messages=[r['failure_message'] for r in episodes if r['failure_message']],
            seconds_per_episode=stats([r['seconds'] for r in episodes]),total_seconds=sum(r['seconds'] for r in episodes),
            calls=sum(r['calls'] for r in episodes),step_milliseconds=stats(latencies),calls_per_second=sum(r['calls'] for r in episodes)/sum(r['seconds'] for r in episodes),
            microbenchmarks=[r for r in grpc if r['type']=='measurement'])
    summary=dict(qualified=len(qualified),total=len(q),processes=processes,aggregate_us=aggregate,paired=paired,groups=groups,
                 parallel=parallel_summary,ctypes_us=interfaces,grpc=grpc_summary,
                 case_medians=[dict(language=l,case=c,operation=o,median_us=v,repetitions=indexed[(l,c,o)]) for (l,c,o),v in sorted(medians.items())])
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    with (out/'case-medians.csv').open('w') as f:
        f.write('language,case,operation,median_us\n')
        for (l,c,o),v in sorted(medians.items()):f.write(f'{l},{c},{o},{v:.6f}\n')
    print(json.dumps({k:v for k,v in summary.items() if k in ('qualified','aggregate_us','parallel')},indent=2))
if __name__=='__main__':main()
