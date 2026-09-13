"""Replay and independently audit actual 128x128 Sim traffic; report failed gates."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
from datetime import datetime, timezone
from verify_navigation import midpoint_enters_open_rect, segment_clear, point_clear

ROOT=Path(__file__).resolve().parents[1]
TERRAIN=[[16128,2048,16640,15360],[16128,17408,16640,30720]]
BOUNDS=(320,320,32448,32448)
EXPANDED=[(a-64,b-64,c+64,d+64) for a,b,c,d in TERRAIN]

def require(ok,message):
    if not ok: raise AssertionError(message)

def fingerprint(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def read_commands(raw,metrics):
    require(len(raw)>=36 and raw[:4]==b'VFR\3','missing scale replay header')
    protocol,seed,count,ticks,frames=struct.unpack_from('<5I',raw,4)
    content,map_id=struct.unpack_from('<QI',raw,24)
    require((protocol,seed,count,ticks,content,map_id)==(metrics['protocol'],metrics['seed'],metrics['units_per_team'],
                metrics['ticks'],int(metrics['content_id']),metrics['map']),'replay setup differs from metrics')
    commands=[]; pos=36; sequences=[0,0]
    for _ in range(frames):
        size=struct.unpack_from('<I',raw,pos)[0]; pos+=4
        frame=raw[pos:pos+size]; pos+=size
        require(len(frame)==size and frame[:4]==b'VFC\1' and size>=30,'invalid command frame')
        tick,sequence,player,order,x,z,n=struct.unpack_from('<IIBBIII',frame,4)
        require(1<=n<=256 and size==26+4*n and player in (0,1) and 0<=order<=3,'invalid command fields')
        ids=list(struct.unpack_from('<'+'I'*n,frame,26))
        require(ids==sorted(set(ids)) and all(player*count<uid<=(player+1)*count for uid in ids),'invalid canonical ownership')
        require(0<=tick<ticks and sequence>sequences[player] and 0<=x<32768 and 0<=z<32768,'invalid command time/coordinate')
        require(not commands or (tick,player,sequence)>tuple(commands[-1][k] for k in ('tick','player','sequence')),'noncanonical command order')
        sequences[player]=sequence
        commands.append(dict(tick=tick,sequence=sequence,player=player,order=order,x=x,z=z,units=ids))
    require(pos==len(raw),'trailing replay data')
    return commands

def assigned_slots(command):
    # Independent exhaustive sort, deliberately distinct from production rings.
    x,z=command['x'],command['z']; count=len(command['units'])
    if count==1 and point_clear((x,z),BOUNDS,EXPANDED): return [(x,z)]
    cells=[]
    for row in range(1,127):
        for col in range(1,127):
            if any(a<=col*256<c and b<=row*256<d for a,b,c,d in TERRAIN): continue
            cells.append((abs(col-x//256)+abs(row-z//256),row,col))
    cells.sort()
    return [(col*256+128,row*256+128) for _,row,col in cells[:count]]

def validate_tick(rows,previous,tick,count):
    require(len(rows)==count and all(len(r)==8 for r in rows),'incomplete tick/schema')
    require([r[1] for r in rows]==list(range(1,count+1)),'missing/reordered/duplicate IDs')
    bins={}
    for row in rows:
        t,uid,x,z,hp,order,gx,gz=row
        require(t==tick and 0<=hp<=100 and 0<=order<=3,'invalid tick/hp/order')
        require(0<=gx<32768 and 0<=gz<32768,'invalid assigned goal')
        old=previous[uid-1] if previous else row
        require(hp<=old[4],'health increased')
        a,b=(old[2],old[3]),(x,z)
        require(sum((p-q)**2 for p,q in zip(a,b))<=1024,'movement speed exceeded')
        require(segment_clear(a,b,BOUNDS,EXPANDED),'swept terrain collision')
        if old[4]==0:
            require(b==a,'dead body moved')
            continue
        # Independent tick-start buckets; +/-1 covers relative motion <=64
        # plus forbidden half-extent128. Production index is not called.
        cx,cz=a[0]//256,a[1]//256
        for bx in range(cx-1,cx+2):
            for bz in range(cz-1,cz+2):
                for other in bins.get((bx,bz),[]):
                    prior=previous[other[1]-1] if previous else other
                    start=(a[0]-prior[2],a[1]-prior[3])
                    end=(x-other[2],z-other[3])
                    # Exact AABB rejection avoids Fraction work for distant pairs.
                    if (max(start[0],end[0])<=-128 or min(start[0],end[0])>=128 or
                        max(start[1],end[1])<=-128 or min(start[1],end[1])>=128): continue
                    require(not midpoint_enters_open_rect(start,end,(-128,-128,128,128)),
                            f'interpolated unit overlap at tick{tick}: {uid}/{other[1]}')
        bins.setdefault((cx,cz),[]).append(row)

def audit_ledger(path,metrics,commands):
    ticks=metrics['ticks']; count=metrics['units_per_team']*2
    require(metrics['map']==1 and metrics['width']==metrics['height']==128,'not actual scale map')
    require(metrics['terrain']==TERRAIN,'changed terrain fixture')
    require([u['id'] for u in metrics['units']]==list(range(1,count+1)),'metrics roster')
    previous=[]; first=[-1]*count; last=[0]*count; idle=[0]*count; longest=[0]*count
    stopped=0; oblique=0; command_index=0
    goals=[]; orders=[]; command_stops=0; applied_commands=0
    with path.open() as stream:
        for tick in range(ticks+1):
            rows=[tuple(map(int,stream.readline().strip().split(','))) for _ in range(count)]
            validate_tick(rows,previous,tick,count)
            if tick==0:
                goals=[row[2:4] for row in rows]; orders=[0]*count
            else:
                while command_index<len(commands) and commands[command_index]['tick']==tick-1:
                    command=commands[command_index]; command_index+=1; applied_commands+=1
                    slots=iter(assigned_slots(command)) if command['order'] in (1,2) else None
                    for uid in command['units']:
                        if previous[uid-1][4]==0: continue
                        orders[uid-1]=command['order']
                        goals[uid-1]=next(slots) if slots is not None else previous[uid-1][2:4]
                        command_stops+=command['order'] in (0,3)
            for row in rows:
                _,uid,x,z,hp,order,gx,gz=row; i=uid-1
                if tick==0:
                    p,n=divmod(i,count//2)
                    expected=((16+n//20 if p==0 else 111-n//20)*256+128,
                              (61+n if count//2<=12 else 54+n%20)*256+128)
                    require((x,z)==expected and hp==100,'spawn fixture differs')
                    require(order==0 and row[6:8]==(x,z),'initial order/goal differs')
                    require(metrics['units'][i]['start']==[x,z],'initial metadata differs')
                    continue
                old=previous[i]
                require(row[6:8]==goals[i],f'assigned goal differs from canonical command at tick{tick}/unit{uid}')
                if orders[i] in (0,3):
                    require(order==orders[i] and (x,z)==old[2:4],'Stop/Hold application or subsequent drift')
                    stopped+=1
                elif orders[i]==1:
                    require(order==1 or (order==0 and (x,z)==goals[i]),'Move falsely reports arrival/order')
                else: require(order==2,'AttackMove changed order without input')
                orders[i]=order
                dx,dz=x-old[2],z-old[3]
                if dx or dz:
                    if first[i]<0: first[i]=tick
                    last[i]=tick; idle[i]=0
                    oblique+=bool(dx and dz and abs(dx)!=abs(dz))
                elif hp>0 and (x,z)!=(gx,gz):
                    idle[i]+=1; longest[i]=max(longest[i],idle[i])
                else: idle[i]=0
            previous=rows
        require(not stream.read(),'extra ledger rows')
    for row,u in zip(previous,metrics['units']):
        i=u['id']-1
        require([row[2],row[3]]==u['end'] and [row[6],row[7]]==u['goal'] and row[4:6]==(u['hp'],u['order']),
                'final metadata differs')
        require((first[i],last[i],idle[i],longest[i])==(u['first_motion_tick'],u['last_motion_tick'],
                    u['pending_idle_tail'],u['max_pending_idle']),'motion ledger differs')
    require(command_index==len(commands),'unconsumed canonical inputs')
    return {'rows':(ticks+1)*count,'oblique_steps':oblique,'stationary_stop_samples':stopped,
            'verified_commands':applied_commands,'stop_hold_subjects':command_stops,
            'full_swept_clearance':True}

def describe(metrics):
    def times(name):
        samples=metrics[name]; require(len(samples)==metrics['ticks'] and all(v>=0 for v in samples),'timing coverage')
        ordered=sorted(samples)
        return {'p95_ms':ordered[(len(ordered)-1)*95//100]/1e6,
                'p99_ms':ordered[(len(ordered)-1)*99//100]/1e6,'max_ms':max(ordered)/1e6}
    step=times('step_ns'); harness=times('harness_ns')
    require(all(a<=b for a,b in zip(metrics['step_ns'],metrics['harness_ns'])),'step exceeds harness timing')
    units=metrics['units']
    pending=[u['id'] for u in units if u['hp']>0 and u['end']!=u['goal']]
    dead=[u['id'] for u in units if u['hp']==0]
    return {'step':step,'generation_admission_recording_and_step':harness,
            'peak_resident_bytes':metrics['peak_resident_bytes'],
            'performance_gate':step['p95_ms']<=4 and step['p99_ms']<=8 and 0<metrics['peak_resident_bytes']<=2_000_000_000,
            'alive':len(units)-len(dead),'dead_ids':dead,'goal_matched_alive':len(units)-len(dead)-len(pending),
            'unfinished_ids':pending,'all_arrived_alive':not pending and not dead,
            'max_pending_idle_tail':max(u['pending_idle_tail'] for u in units),
            'max_pending_idle':max(u['max_pending_idle'] for u in units)}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',type=Path,default=ROOT/'build/windows/sim')
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--ticks',type=int,default=4000)
    parser.add_argument('--repeats',type=int,default=10)
    parser.add_argument('--profiles',nargs='+',default=['crossing','repeated','moving-blockers','ai'])
    parser.add_argument('--counts',nargs='+',type=int,default=[100,250],help='Units per team; permits resuming a separately retained subset')
    args=parser.parse_args()
    require(not (ROOT/'.voidfront-agent/STOP').exists(),'STOP is present')
    require(args.ticks>=4000 and args.repeats>=10,'contract requires full4000 ticks and ten repeats')
    require(set(args.profiles)<=set(['crossing','repeated','moving-blockers','ai']),'unknown profile')
    require(set(args.counts)<=set([100,250]) and len(set(args.counts))==len(args.counts),'unsupported/duplicate team count')
    args.out.mkdir(parents=True,exist_ok=False)
    executables={c:(args.build/c/'voidfront_headless.exe').resolve() for c in ('Debug','Release')}
    hashes={c:fingerprint(exe) for c,exe in executables.items()}
    summary={'started_utc':datetime.now(timezone.utc).isoformat(),'binaries':hashes,'cases':{}}
    hidden=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0
    def run(config,stem,*options,ok=True):
        require(not (ROOT/'.voidfront-agent/STOP').exists(),'STOP is present')
        result=subprocess.run([str(executables[config]),*map(str,options)],capture_output=True,text=True,
                              timeout=600,creationflags=hidden)
        (args.out/(stem+'.log')).write_text(result.stdout+result.stderr)
        require((result.returncode==0)==ok,f'{stem}: unexpected exit: {result.stderr}')
    for count in args.counts:
        for profile in args.profiles:
            name=f'{count*2}-{profile}'; stem=args.out/name
            replay=stem.with_suffix('.vfr'); trace=stem.with_suffix('.trace'); ledger=stem.with_suffix('.csv'); metrics=stem.with_suffix('.json')
            run('Release',name,'--map','scale128','--profile',profile,'--units-per-team',count,'--ticks',args.ticks,
                '--record',replay,'--trace',trace,'--samples',ledger,'--metrics',metrics)
            data=json.loads(metrics.read_text()); raw=replay.read_bytes()
            require(raw[:4]==b'VFR\3' and struct.unpack_from('<I',raw,32)[0]==1,'scale replay lost map')
            commands=read_commands(raw,data)
            case={'ledger':audit_ledger(ledger,data,commands),'commands':commands,'record':describe(data),'replay_sha256':fingerprint(replay),
                  'trace_sha256':fingerprint(trace),'ledger_sha256':fingerprint(ledger),'runs':[]}
            golden=trace.read_bytes()
            for config,repeats in (('Debug',1),('Release',args.repeats)):
                for repeat in range(repeats):
                    label=f'{name}-{config}-{repeat}'; rt=args.out/(label+'.trace'); rm=args.out/(label+'.json')
                    run(config,label,'--replay',replay,'--trace',rt,'--metrics',rm)
                    require(rt.read_bytes()==golden,f'{label}: per-tick hash divergence')
                    repeated=json.loads(rm.read_text())
                    require(repeated['units']==data['units'] and repeated['terrain']==TERRAIN,'replay state/ledger differs')
                    case['runs'].append({'config':config,'repeat':repeat,**describe(repeated)})
            summary['cases'][name]=case
            (args.out/'summary.json').write_text(json.dumps(summary,indent=2))
            print(f"{name}: verified replay/sweeps; arrived {case['record']['goal_matched_alive']}/{count*2}; "
                  f"max pending idle {case['record']['max_pending_idle_tail']}",flush=True)
    require(hashes=={c:fingerprint(exe) for c,exe in executables.items()},'binaries changed during verification')
    summary['completed_utc']=datetime.now(timezone.utc).isoformat()
    (args.out/'summary.json').write_text(json.dumps(summary,indent=2))

if __name__=='__main__': main()
