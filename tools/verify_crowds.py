"""Independent full-ledger crowd checks; bounded fixtures, not full crowd acceptance."""
from __future__ import annotations
import argparse
import copy
import csv
from datetime import datetime, timezone
import hashlib
import io
import json
from pathlib import Path
import subprocess
from verify_navigation import midpoint_enters_open_rect, segment_clear

CASES = {"stationary_blocker": (2,400), "stationary_chain": (6,400),
         "opposing_swap": (2,240), "moving_blocker": (2,200),
         "detour_stop_retarget": (2,None), "occupied_goal": (2,360),
         "moving_attack_target": (2,275)}
BOUNDS = (320,320,7872,5824)
RIDGES = [(3776,704,4416,2368), (3776,4032,4416,5440)]

def require(ok, message):
    if not ok:
        raise AssertionError(message)

def parse(data):
    reader = csv.DictReader(io.StringIO(data))
    require(reader.fieldnames == ['case','tick','id','x','z','hp','order','target_id','detour_count','blocked_ticks','hash'], 'unexpected CSV schema')
    return [{k: v if k == 'case' else int(v) for k,v in row.items()} for row in reader]

def verify(rows):
    require({r['case'] for r in rows} == set(CASES), 'missing/unknown scenario')
    results = {}
    for name,(count,ticks) in CASES.items():
        entries = [r for r in rows if r['case'] == name]
        require(len(entries) % (2*count) == 0, 'partial tick')
        observed = len(entries)//(2*count)
        if ticks is None:
            departure = next((r['tick'] for r in entries if r['id']==1 and r['x']!=640),None)
            require(departure is not None and 1 <= departure <= 80, 'no detour before stop')
            ticks = departure + 240
        require(observed == ticks, f'{name}: omitted/extra ticks')
        initial = {p*count+n+1: ((2 if p==0 else 29)*256+128,(9+n)*256+128)
                   for p in (0,1) for n in range(count)}
        previous = initial.copy()
        pursuit_detour = False
        for t in range(1,ticks+1):
            group = entries[(t-1)*2*count:t*2*count]
            require([r['id'] for r in group] == list(initial), 'missing/reordered/duplicate unit')
            require(all(r['tick']==t and r['hp']==100 and 0<=r['order']<=3 for r in group), 'tick/state invalid')
            require(all(0<=r['target_id']<=2*count and 0<=r['detour_count']<=2 and 0<=r['blocked_ticks']<=8 for r in group), 'avoidance state invalid')
            require(len({r['hash'] for r in group})==1 and 0<=group[0]['hash']<2**64, 'hash coverage invalid')
            current = {r['id']: (r['x'],r['z']) for r in group}
            for uid,now in current.items():
                old = previous[uid]
                require(sum((a-b)**2 for a,b in zip(old,now))<=1024, f'{name}: speed violation')
                require(segment_clear(old,now,BOUNDS,RIDGES), f'{name}: swept terrain collision')
                for other in range(uid+1,2*count+1):
                    # Relative linear motion checks every interpolation time,
                    # independently of the simulation's conservative sweep boxes.
                    a = tuple(x-y for x,y in zip(old,previous[other]))
                    b = tuple(x-y for x,y in zip(now,current[other]))
                    require(not midpoint_enters_open_rect(a,b,(-128,-128,128,128)),
                            f'{name}: units overlap during interpolation')
                if (uid>count and not (name=='moving_attack_target' and uid==3)) or (name.startswith('stationary') and uid!=1):
                    require(now==initial[uid], 'stationary participant displaced')
            if name == 'detour_stop_retarget' and departure < t <= departure+40:
                stop_at = entries[(departure-1)*2*count]
                require(current[1]==(stop_at['x'],stop_at['z']) and group[0]['order']==0, 'Stop drift/order')
            if name == 'occupied_goal' and t<=160:
                require(current[2]==initial[2] and current[1]!=initial[2] and group[0]['order']==1,
                        'occupied goal moved blocker or reported arrival')
            if name == 'moving_attack_target' and t>250:
                require(current[2]==(900,2432) and current[3]!=previous[3], 'pursuit fixture stopped its target/blocker')
                pursuit_detour |= group[0]['target_id']==3 and group[0]['detour_count']>0
            previous = current
        expected = {'stationary_blocker': {1:(640,4300)}, 'stationary_chain': {1:(640,4300)},
                    'opposing_swap': {1:(640,2688),2:(640,2432)},
                    'moving_blocker': {1:(640,3500),2:(2300,2688)},
                    'detour_stop_retarget': {1:(2101,2117)},
                    'occupied_goal': {1:(640,2688),2:(2100,2688)},
                    'moving_attack_target': {2:(900,2432)}}[name]
        for uid,goal in expected.items():
            require(previous[uid]==goal and entries[-2*count+uid-1]['order']==0, f'{name}: false/missing arrival')
        if name=='moving_attack_target': require(pursuit_detour, 'moving pursuit never detoured')
        results[name] = {'ticks':ticks, 'rows':len(entries), 'exact_arrivals':len(expected)}
    return results

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--build',type=Path,default=Path('build/windows/sim'))
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    # Legal endpoints can still overlap between ticks. The oracle must catch
    # this corner crossing, while permitting an exact tangent control.
    require(midpoint_enters_open_rect((128,112),(112,128),(-128,-128,128,128)), 'missed analytic swept crossing')
    require(not midpoint_enters_open_rect((128,112),(128,144),(-128,-128,128,128)), 'rejected analytic tangent')
    require(not segment_clear((3770,712),(3780,702),BOUNDS,RIDGES), 'missed analytic terrain crossing')
    require(segment_clear((3770,710),(3780,700),BOUNDS,RIDGES), 'rejected terrain tangent')
    args.out.mkdir(parents=True,exist_ok=False)
    summary={'started_utc':datetime.now(timezone.utc).isoformat(),'runs':[]}
    golden=None
    for config in ('Debug','Release'):
        exe=(args.build/config/'voidfront_crowd_tests.exe').resolve()
        for repeat in range(10):
            ledger=args.out/f'{config}-{repeat}.csv'
            run=subprocess.run([str(exe),str(ledger)],capture_output=True,text=True,check=True)
            (args.out/f'{config}-{repeat}.log').write_text(run.stdout+run.stderr)
            raw=ledger.read_bytes()
            if golden is None:
                golden=raw
                summary['fixtures']=verify(parse(raw.decode()))
            require(raw==golden,'cross-build/repeat trace divergence')
            summary['runs'].append({'config':config,'repeat':repeat,'ledger_sha256':hashlib.sha256(raw).hexdigest(),
                                    'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()})
    rows=parse(golden.decode())
    # Deliberate evidence corruptions must fail the independent checker.
    mutations=[]
    mutations.append(('omitted_case',[r for r in rows if r['case']!='opposing_swap']))
    mutations.append(('omitted_row',rows[1:]))
    mutations.append(('duplicate_row',[rows[0]]+rows))
    for label,field,value in [('speed','x',6000),('bad_tick','tick',0),('bad_id','id',99),
                              ('damage','hp',0),('bad_order','order',9),('hash_disagreement','hash',0)]:
        changed=copy.deepcopy(rows); changed[0][field]=value; mutations.append((label,changed))
    for label,changed in mutations:
        try: verify(changed)
        except AssertionError: continue
        raise AssertionError(f'accepted corrupted evidence: {label}')
    # Small, speed-valid corruptions must reach the semantic checks, not fail
    # merely because a schema field or endpoint count is absent.
    for label,predicate,update,expected_error in [
        ('stationary_displacement',lambda r:r['case']=='stationary_blocker' and r['tick']==1 and r['id']==2,
         {'x':641},'stationary participant displaced'),
        ('stop_drift',lambda r:r['case']=='detour_stop_retarget' and r['id']==1 and r['order']==0,
         {'x':None},'Stop drift/order'),
        ('false_arrival',lambda r:r['case']=='stationary_blocker' and r['tick']==400 and r['id']==1,
         {'order':1},'false/missing arrival')]:
        changed=copy.deepcopy(rows)
        row=next(r for r in changed if predicate(r))
        row.update({k:row[k]+1 if v is None else v for k,v in update.items()})
        try: verify(changed)
        except AssertionError as error:
            require(expected_error in str(error), f'{label}: wrong assertion: {error}')
        else: raise AssertionError(f'accepted corrupted evidence: {label}')
        mutations.append((label,None))
    summary['rejected_mutations']=[name for name,_ in mutations]
    summary['finished_utc']=datetime.now(timezone.utc).isoformat()
    summary['scope']='Seven bounded small-map fixtures; no choke/stream/128x128/performance or human-play acceptance.'
    (args.out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({'ok':True,'fixtures':summary['fixtures'],'runs':len(summary['runs']),
                      'rejected_mutations':summary['rejected_mutations']}))

if __name__=='__main__': main()
