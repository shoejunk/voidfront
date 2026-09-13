"""Adversarial VFR3, output preservation and independent scale-oracle controls."""
import argparse
import copy
import json
import os
from pathlib import Path
import struct
import subprocess
from verify_scale import validate_tick, audit_ledger, midpoint_enters_open_rect, require, ROOT, TERRAIN

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    require(not (ROOT/'.voidfront-agent/STOP').exists(),'STOP is present')
    args.out.mkdir(parents=True,exist_ok=False)
    hidden=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0
    def run(*options):
        return subprocess.run([str(args.executable.resolve()),*map(str,options)],capture_output=True,text=True,
                              timeout=30,creationflags=hidden)
    replay=args.out/'golden.vfr'; trace=args.out/'record.trace'
    result=run('--map','scale128','--profile','crossing','--units-per-team',100,'--ticks',50,'--record',replay,'--trace',trace)
    require(result.returncode==0,result.stderr)
    golden=replay.read_bytes(); rejected=[]
    def reject(name,payload):
        path=args.out/(name+'.vfr'); path.write_bytes(payload)
        output=args.out/(name+'.trace'); output.write_bytes(b'preserve existing output')
        result=run('--replay',path,'--trace',output)
        require(result.returncode!=0,f'accepted {name}')
        require(output.read_bytes()==b'preserve existing output',f'{name}: output clobbered')
        require(path.read_bytes()==payload,f'{name}: replay input changed')
        rejected.append(name)
    for length in (24,27,28,31,32,33,34,35,len(golden)-1): reject(f'truncated-{length}',golden[:length])
    for name,offset,value in [('map-unknown',32,2),('map-u32max',32,0xffffffff),('map-foundry-large-command',32,0),
                              ('bad-protocol',4,0),('bad-content',24,0),('x-wire-limit',54,32768),('z-wire-limit',58,32768)]:
        bad=bytearray(golden); struct.pack_into('<I',bad,offset,value); reject(name,bad)
    reject('trailing',golden+b'X')
    for flag,value in [('--map','foundry'),('--map','scale128'),('--profile','crossing'),('--ticks','50')]:
        for before in (True,False):
            options=[flag,value,'--replay',replay] if before else ['--replay',replay,flag,value]
            result=run(*options); require(result.returncode!=0,'replay setup override accepted')
            rejected.append(f'override-{flag}-{value}-{before}')
    # Every pair of output types, with existing hard-link and case aliases.
    alias_count=0
    for first in ('--record','--trace','--metrics','--samples','--replay'):
        for second in ('--trace','--metrics','--samples'):
            if first==second: continue
            path=args.out/f'alias{alias_count}.vfr'; path.write_bytes(golden)
            alias=args.out/f'alias{alias_count}.data'; os.link(path,alias)
            result=run(first,path,second,alias)
            require(result.returncode!=0 and path.read_bytes()==golden,'aliased output corrupted data')
            alias_count+=1
    for output_flag in ('--metrics','--samples'):
        for kind in ('exact','case','new-case') if os.name=='nt' else ('exact',):
            path=args.out/f'{output_flag[2:]}-{kind}.dat'
            if kind!='new-case': path.write_bytes(golden)
            alias=path if kind=='exact' else path.with_name(path.name.upper())
            result=run('--record',path,output_flag,alias)
            require(result.returncode!=0,'exact/case output alias accepted')
            require(path.read_bytes()==golden if kind!='new-case' else not path.exists(),'case alias changed data')
            alias_count+=1
    # Oracle positive controls plus intentional position/roster/tick corruptions.
    initial=[(0,1,4224,13952,100,0,4224,13952),(0,2,28416,13952,100,0,28416,13952)]
    validate_tick(initial,[],0,2)
    good=[(1,*row[1:]) for row in initial]; validate_tick(good,initial,1,2)
    mutations=[]
    for name,index,column,value in [('tick',0,0,2),('id',0,1,2),('speed',0,2,4260),('hp',0,4,101),
                                     ('order',0,5,4),('goal',0,6,32768),('terrain',0,2,16128)]:
        bad=[list(r) for r in good]; bad[index][column]=value; mutations.append((name,bad))
    mutations.extend([('missing',good[:1]),('extra',good+[good[0]])])
    for name,bad in mutations:
        try: validate_tick(bad,initial,1,2)
        except (AssertionError,ValueError): pass
        else: raise AssertionError(f'oracle accepted {name}')
    require(midpoint_enters_open_rect((128,112),(112,128),(-128,-128,128,128)),'missed swept crossing')
    require(not midpoint_enters_open_rect((128,112),(128,144),(-128,-128,128,128)),'rejected swept tangent')
    # Exercise broadphase and actual terrain checks through the full tick oracle.
    analytic=[('adjacent-bucket',[(0,1,1000,1000,100,1,2000,1000),(0,2,1191,1000,100,1,500,1000)],[(1032,1000),(1159,1000)]),
              ('swept-corner',[(0,1,1000,1000,100,1,2000,1000),(0,2,1128,1112,100,0,1128,1112)],[(1016,984),(1128,1112)]),
              ('terrain-within-speed',[(0,1,16060,2000,100,1,27776,16512),(0,2,28544,13952,100,0,28544,13952)],[(16068,2000),(28544,13952)])]
    for name,previous,positions in analytic:
        rows=[(1,r[1],*position,*r[4:]) for r,position in zip(previous,positions)]
        try: validate_tick(rows,previous,1,2)
        except AssertionError: pass
        else: raise AssertionError(f'oracle accepted {name}')
    fake={'ticks':1,'units_per_team':1,'map':1,'width':128,'height':128,'terrain':TERRAIN,
          'units':[{'id':1,'start':[4224,15744]},{'id':2,'start':[28544,15744]}]}
    base=[(0,1,4224,15744,100,0,4224,15744),(0,2,28544,15744,100,0,28544,15744)]
    for name,command,after in [
        ('frozen-despite-Move',dict(tick=0,order=1,x=27776,z=16512,units=[1]),[(1,*r[1:]) for r in base]),
        ('first-Stop-drift',dict(tick=0,order=0,x=0,z=0,units=[1]),[(1,1,4225,15744,100,0,4224,15744),(1,*base[1][1:])])]:
        path=args.out/(name+'.csv'); path.write_text(''.join(','.join(map(str,row))+'\n' for row in base+after))
        try: audit_ledger(path,fake,[command])
        except AssertionError: pass
        else: raise AssertionError(f'oracle accepted {name}')
    report={'rejected_replays_and_overrides':rejected,'preserved_output_aliases':alias_count,
            'rejected_ledger_corruptions':[name for name,_ in mutations]+[name for name,_,_ in analytic]+['frozen-despite-Move','first-Stop-drift'],
            'analytic_sweep_controls':2}
    (args.out/'summary.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report))

if __name__=='__main__': main()
