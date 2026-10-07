"""Frozen-checkout benchmark runner. Every raw process output is retained."""
from pathlib import Path
import argparse, hashlib, itertools, json, os, platform, statistics, subprocess, time

HERE=Path(__file__).resolve().parent

def projection(s):
    player_fields=('hp','max_hp','block','energy','stars','powers','hand','draw','discard','exhaust','play','orbs','orb_capacity','pets')
    enemy_fields=('id','combat_id','hp','max_hp','block','powers','next_move','rng_counter')
    # Only combat streams are compared; run generation/room allocation has a different public surface.
    rng_fields=('Shuffle','CombatCardGeneration','CombatPotionGeneration','CombatCardSelection',
                'CombatEnergyCosts','CombatTargets','MonsterAi','Niche','CombatOrbs')
    player={k:s['player'][k] for k in player_fields}
    # Both encoders represent an uninitialized pet move; normalize wire spelling only.
    player['pets']=[dict(p,next_move=None if p['next_move']=='UNSET_MOVE' else p['next_move']) for p in player['pets']]
    return dict(round=s['round'],turn=s['turn'],player=player,
                enemies=[{k:e[k] for k in enemy_fields} for e in s['enemies']],rng={k:s['rng'][k] for k in rng_fields})

def differences(a,b,path='$'):
    if type(a)!=type(b) and not isinstance(a,(int,float)):
        return [dict(path=path,cpp=a,csharp=b)]
    if isinstance(a,dict):
        result=[]
        for k in sorted(set(a)|set(b)):
            if k not in a or k not in b:result.append(dict(path=path+'.'+k,cpp=a.get(k),csharp=b.get(k)))
            else:result+=differences(a[k],b[k],path+'.'+k)
        return result
    if isinstance(a,list):
        if len(a)!=len(b):return [dict(path=path+'.length',cpp=len(a),csharp=len(b))]
        return sum((differences(x,y,f'{path}[{i}]') for i,(x,y) in enumerate(zip(a,b))),[])
    return [] if a==b else [dict(path=path,cpp=a,csharp=b)]

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    p.add_argument('--native',type=Path,required=True);p.add_argument('--managed',type=Path,required=True)
    p.add_argument('--dotnet',default=str(Path.home()/'.dotnet/dotnet'));p.add_argument('--cpu',default='2')
    p.add_argument('--mode',choices=['verify','measure','parallel'],default='verify');p.add_argument('--repetitions',type=int,default=3)
    p.add_argument('--iterations',type=int,default=64);p.add_argument('--warmup',type=int,default=128);p.add_argument('--samples',type=int,default=7)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    manifests=a.output/'matrix.tsv'
    cases=[]
    for seed,char,asc,(enc,act),advance in itertools.product(
        ('DUAL001','ENVBENCH20261001A','ENVBENCH20261001B'),
        ('IRONCLAD','SILENT','REGENT','DEFECT','NECROBINDER'),(0,10),
        (('CULTISTS_NORMAL',0),('EXOSKELETONS_NORMAL',1),('MECHA_KNIGHT_ELITE',2)),(0,2)):
        cases.append([f'{seed}-{char}-A{asc}-{enc}-T{advance+1}',seed,char,str(asc),enc,str(act),str(advance)])
    if not manifests.exists():manifests.write_text('\n'.join('\t'.join(c) for c in cases)+'\n')
    commands=dict(cpp=[str(a.native.resolve())],csharp=[a.dotnet,str(a.managed.resolve())])
    records=[]
    def execute(lang,mode,manifest,extra,stem,cpus):
        cmd=['/usr/bin/time','-v','taskset','-c',cpus]+commands[lang]+[str(manifest.resolve()),mode]+list(map(str,extra))
        begin=time.monotonic()
        with (a.output/(stem+'.jsonl')).open('w') as stdout,(a.output/(stem+'.stderr')).open('w') as stderr:
            proc=subprocess.run(cmd,stdout=stdout,stderr=stderr,timeout=1800)
        rows=[json.loads(l) for l in (a.output/(stem+'.jsonl')).read_text().splitlines() if l.startswith('{')]
        result=dict(stem=stem,language=lang,mode=mode,command=cmd,exit_code=proc.returncode,wall_seconds=time.monotonic()-begin,
                    raw_file=stem+'.jsonl',measurements=sum(r.get('type')=='measurement' for r in rows),failures=[r for r in rows if r.get('type')=='failure'])
        records.append(result);(a.output/f'{a.mode}-processes.json').write_text(json.dumps(records,indent=2)+'\n')
        print(f'{stem}: exit={proc.returncode}, seconds={result["wall_seconds"]:.2f}, failures={len(result["failures"])}',flush=True)
        if proc.returncode not in (0,2):raise RuntimeError(f'{stem}: unexpected failure')
        return rows
    if a.mode=='verify':
        rows={lang:execute(lang,'verify',manifests,[],f'verify-{lang}',a.cpu) for lang in commands}
        indexed={lang:{(r['case'],r['boundary']):r['state'] for r in rs if r['type']=='state'} for lang,rs in rows.items()}
        actions={lang:{r['case']:dict(hand_index=r['hand_index'],target=r['target']) for r in rs if r['type']=='action'} for lang,rs in rows.items()}
        qualification=[]
        for c in cases:
            item=dict(case=c[0],metadata=c,action_equal=actions['cpp'].get(c[0])==actions['csharp'].get(c[0]),boundaries={})
            for boundary in ('root','after_endturn','after_card'):
                x=indexed['cpp'].get((c[0],boundary));y=indexed['csharp'].get((c[0],boundary))
                item['boundaries'][boundary]=dict(available=x is not None and y is not None,differences=differences(projection(x),projection(y)) if x is not None and y is not None else [])
            item['qualified']=item['action_equal'] and all(v['available'] and not v['differences'] for v in item['boundaries'].values())
            qualification.append(item)
        (a.output/'qualification.json').write_text(json.dumps(qualification,indent=2)+'\n')
        valid=[c for c,q in zip(cases,qualification) if q['qualified']]
        (a.output/'qualified.tsv').write_text('\n'.join('\t'.join(c) for c in valid)+'\n')
        print(f'qualified={len(valid)}/{len(cases)}; boundaries={sum(v["available"] and not v["differences"] for q in qualification for v in q["boundaries"].values())}/540',flush=True)
    elif a.mode=='measure':
        for rep in range(a.repetitions):
            # Rotate cases to expose position/tiering bias; pair both languages in the same order.
            offset=(rep*61)%len(cases);rotated=cases[offset:]+cases[:offset]
            manifest=a.output/f'matrix-rep{rep+1}.tsv';manifest.write_text('\n'.join('\t'.join(c) for c in rotated)+'\n')
            for lang in (('cpp','csharp') if rep%2==0 else ('csharp','cpp')):
                execute(lang,'measure',manifest,[a.iterations,a.warmup,a.samples],f'measure-{lang}-rep{rep+1}',a.cpu)
    else:
        valid=(a.output/'qualified.tsv').read_text().splitlines()
        representative=next(l for l in valid if 'DUAL001-SILENT-A10-CULTISTS_NORMAL-T3' in l)
        manifest=a.output/'parallel.tsv';manifest.write_text(representative+'\n')
        for rep in range(a.repetitions):
            for workers in (1,2,4,8):
                cpus=','.join(map(str,[2,4,6,8,10,12,14,0][:workers]))
                for lang in (('cpp','csharp') if rep%2==0 else ('csharp','cpp')):
                    execute(lang,'parallel',manifest,[workers,8192],f'parallel-{lang}-{workers}-rep{rep+1}',cpus)

if __name__=='__main__':main()
