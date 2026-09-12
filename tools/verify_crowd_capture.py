"""Check complete packaged InputEvent crowd evidence with independent geometry."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
from verify_crowds import BOUNDS, RIDGES, require
from verify_navigation import midpoint_enters_open_rect, segment_clear

def verify(report):
    require(report['mode']=='crowd' and report['tick']==400 and report['stage']==8, 'unfinished fixture')
    require(report['ok'] and not report['errors'] and not report['option_error'], 'runtime error')
    initial=report['initial']
    expected_ids=list(range(1,13))
    require([u['id'] for u in initial]==expected_ids, 'initial roster')
    initial_by_id={u['id']:u for u in initial}
    for uid,u in initial_by_id.items():
        require((u['x'],u['z'])==((2 if uid<=6 else 29)*256+128,(9+(uid-1)%6)*256+128) and u['hp']==100,
                'altered initial state')
    inputs=report['inputs']
    labels=['swap_one','swap_two','chain','stop','resume']
    require([c['label'] for c in inputs]==labels, 'missing/reordered/extra InputEvents')
    commands=dict(zip(labels,inputs))
    for c,uid in zip(inputs,[1,2,1,1,1]):
        require(c['accepted'] and c['units']==[uid] and c['order']==(0 if c['label']=='stop' else 1), 'wrong input subject/order')
    require(all(1<=c['event_tick']<400 for c in inputs) and
            all(a['event_tick']<b['event_tick'] for a,b in zip(inputs,inputs[1:])), 'input timing order')
    for label,expected in [('swap_one',(640,2688)),('swap_two',(640,2432)),('chain',(640,4300)),('resume',(640,4300))]:
        c=commands[label]
        require(abs(c['x']-expected[0])<=1 and abs(c['z']-expected[1])<=1, 'input changed fixture destination')
    positions=report['positions']
    require(len(positions)==400 and [s['tick'] for s in positions]==list(range(1,401)), 'missing/extra tick ledger')
    previous=initial_by_id
    swap_tick=report['swap_tick']
    require(commands['swap_two']['event_tick']<swap_tick<commands['chain']['event_tick'], 'swap evidence order')
    stop_first=commands['stop']['event_tick']+1
    stop_last=commands['resume']['event_tick']
    require(stop_last-stop_first+1>=9 and report['stop_samples']==stop_last-stop_first+1 and report['stop_tick']==stop_first,
            'missing stationary Stop interval')
    # Preserve the pre-application position, including the first Stop tick.
    stationary=positions[commands['stop']['event_tick']-1]['units'][0]
    oblique=0
    for snapshot in positions:
        tick=snapshot['tick']
        require([u['id'] for u in snapshot['units']]==expected_ids, 'missing/duplicate/reordered participant')
        current={u['id']:u for u in snapshot['units']}
        for uid,u in current.items():
            old=previous[uid]
            a=(old['x'],old['z']); b=(u['x'],u['z'])
            dx=b[0]-a[0]; dz=b[1]-a[1]
            require(u['hp']==100 and dx*dx+dz*dz<=1024, 'health/speed violation')
            require(segment_clear(a,b,BOUNDS,RIDGES), 'swept terrain collision')
            oblique+=int(bool(dx and dz and abs(dx)!=abs(dz)))
            for other in range(uid+1,13):
                v=current[other]; ov=previous[other]
                require(not midpoint_enters_open_rect((a[0]-ov['x'],a[1]-ov['z']),
                                                       (b[0]-v['x'],b[1]-v['z']),(-128,-128,128,128)),
                        'swept unit collision')
            if uid>=3: require(b==(initial_by_id[uid]['x'],initial_by_id[uid]['z']), 'stationary blocker moved')
        if stop_first<=tick<=stop_last:
            u=current[1]
            require((u['x'],u['z'])==(stationary['x'],stationary['z']) and u['order']==0 and not u['moving'], 'Stop drift')
        if tick==swap_tick:
            for uid,label in [(1,'swap_one'),(2,'swap_two')]:
                c=commands[label]; u=current[uid]
                require((u['x'],u['z'])==(c['x'],c['z']) and u['order']==0 and not u['moving'], 'false swap arrival')
        if tick>=swap_tick:
            u=current[2]; c=commands['swap_two']
            require((u['x'],u['z'])==(c['x'],c['z']) and u['order']==0 and not u['moving'], 'arrived swap participant drifted')
        previous=current
    c=commands['resume']; u=previous[1]
    require((u['x'],u['z'])==(c['x'],c['z']) and u['order']==0 and not u['moving'], 'false chain arrival')
    require(oblique>0 and abs(stationary['x']-commands['chain']['x'])>32, 'no actual detour before Stop')
    require([c['label'] for c in report['captures']]==['swap-detour','swap-arrived','stopped','chain-arrived'] and
            all(c['error']==0 for c in report['captures']), 'missing screenshots')
    return {'ticks':400,'unit_rows':4800,'swap_tick':swap_tick,'stop_ticks':stop_last-stop_first+1,'oblique_steps':oblique}

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('report',type=Path)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    report=json.loads(args.report.read_text())
    result=verify(report)
    for capture in report['captures']: require(Path(capture['path']).is_file(), 'missing screenshot file')
    corruptions=[]
    for label,change in [
        ('omitted_tick',lambda r:r['positions'].pop()),
        ('omitted_input',lambda r:r['inputs'].pop()),
        ('wrong_subject',lambda r:r['inputs'][1].update(units=[1])),
        ('duplicate_unit',lambda r:r['positions'][0]['units'][1].update(id=1)),
        ('speed',lambda r:r['positions'][0]['units'][0].update(x=6000)),
        ('false_arrival',lambda r:r['positions'][-1]['units'][0].update(order=1)),
        ('missing_capture',lambda r:r['captures'].pop())]:
        altered=copy.deepcopy(report); change(altered)
        try: verify(altered)
        except AssertionError: corruptions.append(label)
        else: raise AssertionError('accepted corruption: '+label)
    altered=copy.deepcopy(report)
    events={c['label']:c for c in altered['inputs']}
    for sample in altered['positions']:
        if events['stop']['event_tick']<sample['tick']<=events['resume']['event_tick']:
            sample['units'][0]['x']+=1
    try: verify(altered)
    except AssertionError as error:
        require(str(error)=='Stop drift','first-tick drift failed the wrong assertion')
        corruptions.append('first_stop_tick_drift')
    else: raise AssertionError('accepted first Stop tick drift')
    result.update(ok=True,report_sha256=hashlib.sha256(args.report.read_bytes()).hexdigest(),rejected_mutations=corruptions)
    args.out.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result))

if __name__=='__main__': main()
