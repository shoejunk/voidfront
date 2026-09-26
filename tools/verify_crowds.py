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
         "moving_attack_target": (2,275), "short_disjoint": (2,240),
         "firing_then_target_leaves": (1,251),
         "opposed_stop_retarget": (2,420), "opposed_hold_retarget": (2,420),
         "outer_channel_prefix": (2,280)}
COOPERATIVE_CASES = {"follower_chain_forward": (6,176), "follower_chain_reverse": (6,176)}
CONVOY_CASES = {"convoy_stop": (6,168), "convoy_hold": (6,168), "convoy_retarget": (6,364)}
CASES.update(COOPERATIVE_CASES)
CASES.update(CONVOY_CASES)
REDISTRIBUTION_CASES = {"goal_redistribution": (6,760)}
BOUNDS = (320,320,7872,5824)
RIDGES = [(3776,704,4416,2368), (3776,4032,4416,5440)]

def require(ok, message):
    if not ok:
        raise AssertionError(message)

def parse(data):
    reader = csv.DictReader(io.StringIO(data))
    require(reader.fieldnames == ['case','tick','id','x','z','hp','order','target_id','detour_count','blocked_ticks','hash'], 'unexpected CSV schema')
    return [{k: v if k == 'case' else int(v) for k,v in row.items()} for row in reader]

def verify(rows, cases=None):
    cases = CASES if cases is None else cases
    require({r['case'] for r in rows} == set(cases), 'missing/unknown scenario')
    results = {}
    for name,(count,ticks) in cases.items():
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
        previous_hp = {uid:100 for uid in initial}
        pursuit_detour = False
        for t in range(1,ticks+1):
            group = entries[(t-1)*2*count:t*2*count]
            require([r['id'] for r in group] == list(initial), 'missing/reordered/duplicate unit')
            combat = name.startswith('opposed_') or name=='firing_then_target_leaves'
            require(all(r['tick']==t and (0<=r['hp']<=100 if combat else r['hp']==100) and 0<=r['order']<=3 for r in group), 'tick/state invalid')
            require(all(0<=r['target_id']<=2*count and 0<=r['detour_count']<=2 and 0<=r['blocked_ticks']<=8 for r in group), 'avoidance state invalid')
            require(len({r['hash'] for r in group})==1 and 0<=group[0]['hash']<2**64, 'hash coverage invalid')
            current = {r['id']: (r['x'],r['z']) for r in group}
            for uid,now in current.items():
                old = previous[uid]
                require(group[uid-1]['hp']<=previous_hp[uid], 'health increased')
                require(sum((a-b)**2 for a,b in zip(old,now))<=1024, f'{name}: speed violation')
                require(segment_clear(old,now,BOUNDS,RIDGES), f'{name}: swept terrain collision')
                for other in range(uid+1,2*count+1):
                    if previous_hp[uid]<=0 or previous_hp[other]<=0: continue
                    # Relative linear motion checks every interpolation time,
                    # independently of the simulation's conservative sweep boxes.
                    a = tuple(x-y for x,y in zip(old,previous[other]))
                    b = tuple(x-y for x,y in zip(now,current[other]))
                    require(not midpoint_enters_open_rect(a,b,(-128,-128,128,128)),
                            f'{name}: units overlap during interpolation')
                enemy_moves = name.startswith('opposed_') or name in ('firing_then_target_leaves','outer_channel_prefix') or (name=='moving_attack_target' and uid==3)
                if (uid>count and not enemy_moves) or (name.startswith('stationary') and uid!=1):
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
            if name=='short_disjoint' and t>200:
                require(current[1][1]==current[2][1]==2560 and current[1][0]>=previous[1][0] and current[2][0]<=previous[2][0],
                        'disjoint short journeys diverted')
            if name.startswith('opposed_') and 80<t<=120:
                stopped=entries[79*2*count]
                require(current[1]==(stopped['x'],stopped['z']) and group[0]['order']==(3 if 'hold' in name else 0),
                        'opposed Stop/Hold drift')
            if name=='firing_then_target_leaves':
                require(all(r['hp']==(92 if t==251 and r['id']==2 else 100) for r in group), 'firing departure damage differs')
                if t==250: require(current=={1:(1024,2560),2:(1792,2560)},'firing departure setup differs')
                if t==251:
                    require(current[1]==previous[1] and group[0]['target_id']==2 and current[2][0]>1792,
                            'shooter moved after firing or target failed departure')
            if name=='outer_channel_prefix':
                z_by_id={1:400,2:600,3:400,4:600}
                if t==200:
                    require(current=={uid:(1200 if uid<=2 else 6800,z) for uid,z in z_by_id.items()},'outer channel setup differs')
                if t>200:
                    require(all(now[1]<=704 for now in current.values()),'outer channel diverted into central portal')
                    if t<=240: require(all(current[uid][1]==z for uid,z in z_by_id.items()),'premature outer channel diversion')
            if name in COOPERATIVE_CASES or name in CONVOY_CASES or name=='goal_redistribution':
                if t==160:
                    require(all(current[uid]==(640,2432+128*(uid-1)) for uid in range(1,7)),
                            'cooperative setup differs')
                if name in COOPERATIVE_CASES and t>160:
                    direction=1 if name.endswith('forward') else -1
                    require(all(current[uid]==(640,2432+128*(uid-1)+direction*32*(t-160)) for uid in range(1,7)),
                            'unobstructed tangent followers stopped or diverted')
            if name in ('convoy_stop','convoy_hold') and t>160:
                require(current[6]==(640,3072) and group[5]['order']==(3 if name=='convoy_hold' else 0),
                        'blocked convoy displaced stationary leader')
                if t==161:
                    require(all(current[uid]==(640,2432+128*(uid-1)) for uid in range(1,6)),
                            'blocked convoy partially committed failed dependency')
            if name=='convoy_retarget':
                if 160<t<=164:
                    require(all(current[uid]==(640,2432+128*(uid-1)+32*(t-160)) for uid in range(1,7)),
                            'retarget convoy never advanced together')
                if t==165: require(current[3][0]>640 and current[3][1]==2816,'retarget convoy followed obsolete direction')
            previous = current
            previous_hp = {r['id']:r['hp'] for r in group}
        expected = {'stationary_blocker': {1:(640,4300)}, 'stationary_chain': {1:(640,4300)},
                    'opposing_swap': {1:(640,2688),2:(640,2432)},
                    'moving_blocker': {1:(640,3500),2:(2300,2688)},
                    'detour_stop_retarget': {1:(2101,2117)},
                    'occupied_goal': {1:(640,2688),2:(2100,2688)},
                    'moving_attack_target': {2:(900,2432)},
                    'short_disjoint': {1:(1124,2560),2:(1524,2560)},
                    'opposed_stop_retarget': {1:(1301,2107)}, 'opposed_hold_retarget': {1:(1301,2107)},
                    'firing_then_target_leaves': {}, 'outer_channel_prefix': {},
                    'follower_chain_forward': {uid:(640,2432+128*(uid-1)+512) for uid in range(1,7)},
                    'follower_chain_reverse': {uid:(640,2432+128*(uid-1)-512) for uid in range(1,7)},
                    'convoy_stop': {}, 'convoy_hold': {},
                    'convoy_retarget': {uid:((1901,2816) if uid==3 else (640,2944+128*(uid-1))) for uid in range(1,7)},
                    'goal_redistribution': {uid:(2304,2432+128*(6-uid)) for uid in range(1,7)}}[name]
        for uid,goal in expected.items():
            require(previous[uid]==goal and entries[-2*count+uid-1]['order']==0, f'{name}: false/missing arrival')
        if name=='moving_attack_target': require(pursuit_detour, 'moving pursuit never detoured')
        results[name] = {'ticks':ticks, 'rows':len(entries), 'exact_arrivals':len(expected)}
    return results

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--build',type=Path,default=Path('build/windows/sim'))
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--cooperative',action='store_true',help='compatibility flag; cooperative regression coverage is always included')
    parser.add_argument('--redistribution',action='store_true',help='include opt-in reversed same-stream goal diagnostic')
    args=parser.parse_args()
    cases=dict(CASES)
    flags=[]
    if args.redistribution: cases.update(REDISTRIBUTION_CASES); flags.append('--redistribution')
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
            run=subprocess.run([str(exe),str(ledger),*flags],capture_output=True,text=True)
            (args.out/f'{config}-{repeat}.log').write_text(run.stdout+run.stderr)
            require(run.returncode==0,f'{config} run {repeat} failed; complete stdout/stderr retained beside {ledger}')
            raw=ledger.read_bytes()
            if golden is None:
                golden=raw
                summary['fixtures']=verify(parse(raw.decode()),cases)
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
        try: verify(changed,cases)
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
         {'order':1},'false/missing arrival'),
        ('hold_lost',lambda r:r['case']=='opposed_hold_retarget' and r['tick']==81 and r['id']==1,
         {'order':0},'opposed Stop/Hold drift'),
        ('disjoint_diversion',lambda r:r['case']=='short_disjoint' and r['tick']==240 and r['id']==1,
         {'z':2561},'disjoint short journeys diverted'),
        ('shooter_displaced',lambda r:r['case']=='firing_then_target_leaves' and r['tick']==251 and r['id']==1,
         {'x':1025},'shooter moved after firing'),
        ('retarget_false_arrival',lambda r:r['case']=='opposed_stop_retarget' and r['tick']==420 and r['id']==1,
         {'order':1},'false/missing arrival'),
        ('outer_channel_diversion',lambda r:r['case']=='outer_channel_prefix' and r['tick']==240 and r['id']==3,
         {'x':None,'z':401},'premature outer channel diversion')]:
        changed=copy.deepcopy(rows)
        row=next(r for r in changed if predicate(r))
        row.update({k:row[k]+1 if v is None else v for k,v in update.items()})
        try: verify(changed,cases)
        except AssertionError as error:
            require(expected_error in str(error), f'{label}: wrong assertion: {error}')
        else: raise AssertionError(f'accepted corrupted evidence: {label}')
        mutations.append((label,None))
    for name,uid,z in [('follower_chain_forward',1,2943),('follower_chain_reverse',6,2561)]:
        changed=copy.deepcopy(rows)
        row=next(r for r in changed if r['case']==name and r['tick']==176 and r['id']==uid)
        row['z']=z
        try: verify(changed,cases)
        except AssertionError as error:
            require('unobstructed tangent followers stopped or diverted' in str(error),f'{name}: wrong assertion: {error}')
        else: raise AssertionError(f'accepted one-coordinate follower shortfall: {name}')
        mutations.append((name+'_shortfall',None))
    for name,tick,uid,update,expected_error in [
        ('convoy_hold',161,6,{'order':0},'blocked convoy displaced stationary leader'),
        ('convoy_stop',161,6,{'x':641},'blocked convoy displaced stationary leader'),
        ('convoy_retarget',364,3,{'order':1},'false/missing arrival')]:
        changed=copy.deepcopy(rows)
        row=next(r for r in changed if r['case']==name and r['tick']==tick and r['id']==uid)
        row.update(update)
        try: verify(changed,cases)
        except AssertionError as error:
            require(expected_error in str(error),f'{name}: wrong assertion: {error}')
        else: raise AssertionError(f'accepted corrupted convoy evidence: {name}')
        mutations.append((name+'_semantic_mutation',None))
    summary['rejected_mutations']=[name for name,_ in mutations]
    summary['finished_utc']=datetime.now(timezone.utc).isoformat()
    summary['scope']='Seventeen bounded small-map fixtures; original twelve safety/arrival checks retained, plus tangent followers in both ID directions, blocked Stop/Hold convoys, and interrupted convoy retarget. Optional reversed-goal redistribution remains diagnostic. Outer-channel prefix and blocked convoy prefixes do not establish eventual completion. No transaction-cap exhaustion, dense-stream, 128x128/performance or human-play acceptance.'
    (args.out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({'ok':True,'fixtures':summary['fixtures'],'runs':len(summary['runs']),
                      'rejected_mutations':summary['rejected_mutations']}))

if __name__=='__main__': main()
