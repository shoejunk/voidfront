"""Audit packaged economy conservation/clearance and replay its actual commands."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
from datetime import datetime, timezone
from verify_navigation import segment_clear, midpoint_enters_open_rect
from verify_net_match import compare_snapshot, prefix

ROOT = Path(__file__).resolve().parents[1]
BOUNDS = (320, 320, 31 * 256 - 64, 23 * 256 - 64)
TERRAIN = [(15*256-64, 3*256-64, 17*256+64, 9*256+64),
           (15*256-64, 16*256-64, 17*256+64, 21*256+64)]


def map_geometry(snapshot):
    width, height = snapshot['width'], snapshot['height']
    require((width, height) in ((32, 24), (64, 48)), 'unsupported economy geometry')
    terrain = list(TERRAIN)
    if width == 64:
        terrain += [(x0*256-64, z0*256-64, x1*256+64, z1*256+64)
                    for x0,z0,x1,z1 in [(31,5,33,19),(31,29,33,43),(47,27,49,32),(47,39,49,45)]]
    return (320,320,(width-1)*256-64,(height-1)*256-64), terrain


def require(ok, message):
    if not ok:
        raise AssertionError(message)


def audit(report):
    require(report['ok'] and not report['errors'], 'packaged fixture failed')
    initial, final = report['initial_snapshot'], report['final_snapshot']
    bounds, terrain = map_geometry(initial)
    require(initial['map_id'] == final['map_id'] == 2, 'wrong economy map')
    require(initial['tick'] == 0 and final['tick'] > 0, 'invalid tick bounds')
    ticks = final['tick']
    trace, ledger = report['trace'], report['snapshots']
    require([r['tick'] for r in trace] == list(range(ticks+1)), 'hash trace gap')
    require([r['tick'] for r in ledger] == list(range(ticks+1)), 'economy ledger gap')
    require(ledger[0] == initial and ledger[-1] == final, 'ledger endpoint mismatch')
    total = sum(initial['salvage']) + sum(d['remaining'] for d in initial['deposits'])
    total += sum(u['cargo'] for u in initial['units'])
    prior = initial
    max_deposited = 0
    construction_ticks = 0
    for row, trace_row in zip(ledger, trace, strict=True):
        tick = row['tick']
        require(row['hash'] == trace_row['hash'], f'tick {tick}: ledger/hash mismatch')
        require(len(row['salvage']) == 2 and min(row['salvage']) >= 0, 'negative resources')
        require([u['id'] for u in row['units']] == list(range(1, 7)), 'unit roster changed')
        require([s['id'] for s in row['structures']] == list(range(1, len(row['structures'])+1)), 'structure ids invalid')
        require([d['id'] for d in row['deposits']] == [1, 2, 3, 4], 'deposit roster changed')
        require(all(0 <= d['remaining'] <= initial['deposits'][i]['remaining'] for i,d in enumerate(row['deposits'])), 'deposit bounds')
        foundries = [s for s in row['structures'] if s['kind'] == 1]
        observed = sum(row['salvage']) + sum(row.get('flux', [0, 0])) + 50*sum(bool(r) or bool(t) for r, t in zip(row.get('researched', [0, 0]), row.get('research_ticks', [0, 0]))) + sum(d['remaining'] for d in row['deposits'])
        observed += sum(u['cargo'] for u in row['units']) + 100 * len(foundries)
        require(observed == total, f'tick {tick}: salvage created or lost')
        max_deposited = max(max_deposited, row['salvage'][0])
        require(all(d['remaining'] <= prior['deposits'][i]['remaining'] for i,d in enumerate(row['deposits'])), 'deposit replenished')
        obstacles = terrain + [(s['x']-320, s['z']-320, s['x']+320, s['z']+320) for s in row['structures']]
        # Deposits are fixed service objects with half-size128 plus worker radius64.
        obstacles += [(d['x']-192, d['z']-192, d['x']+192, d['z']+192) for d in row['deposits']]
        for i, unit in enumerate(row['units']):
            old = prior['units'][i]
            require(unit['kind'] == 1 and 0 <= unit['cargo'] <= 10, 'worker/cargo bounds')
            a,b = (old['x'],old['z']), (unit['x'],unit['z'])
            require(sum((p-q)**2 for p,q in zip(a,b)) <= 1024, 'worker exceeds speed')
            require(segment_clear(a,b,bounds,obstacles), f'tick {tick}: worker {unit["id"]} crosses obstacle')
            for j in range(i):
                other, previous_other = row['units'][j], prior['units'][j]
                start = (a[0]-previous_other['x'],a[1]-previous_other['z'])
                end = (b[0]-other['x'],b[1]-other['z'])
                require(not midpoint_enters_open_rect(start,end,(-128,-128,128,128)), 'swept worker collision')
        for site in foundries:
            require(0 <= site['build_ticks'] <= 100, 'construction progress bounds')
            old = next((s for s in prior['structures'] if s['id'] == site['id']), None)
            before = old['build_ticks'] if old else 0
            require(0 <= site['build_ticks']-before <= 1, 'construction progress jumped')
            if site['build_ticks'] > before:
                construction_ticks += 1
                require(any(u['build_id'] == site['id'] and u['order'] == 6 and
                            max(abs(u['x']-site['x']),abs(u['z']-site['z'])) <= 352
                            for u in row['units']) or site['build_ticks'] == 100,
                        'construction advanced without adjacent builder')
        prior = row
    require(max_deposited >= 100, 'did not return enough harvested salvage')
    require(any(s['kind'] == 1 and s['build_ticks'] == 100 for s in final['structures']), 'no completed Foundry')
    require(construction_ticks >= 100, 'no complete construction trajectory')
    return {'ticks': ticks, 'worker_rows': len(ledger)*6, 'conserved_salvage': total,
            'construction_progress_ticks': construction_ticks}


def replay_commands(raw, report):
    require(raw[:4] == b'VFR\3' and len(raw) >= 36, 'invalid replay header')
    protocol,seed,count,ticks,n,content,map_id = struct.unpack_from('<5IQI',raw,4)
    initial = report['initial_snapshot']
    require((protocol,count,ticks,content,map_id) ==
            (initial['protocol'],3,report['final_snapshot']['tick'],int(initial['content_id'],16),2), 'replay setup mismatch')
    pos, commands = 36, []
    for _ in range(n):
        size, = struct.unpack_from('<I', raw, pos); pos += 4
        frame = raw[pos:pos+size]; pos += size
        require(frame[:4] == b'VFC\1' and len(frame) == size, 'invalid frame')
        tick,seq,player,order,x,z,ids_count = struct.unpack_from('<IIBBIII',frame,4)
        ids = list(struct.unpack_from('<'+'I'*ids_count,frame,26))
        require(size == 26+4*ids_count and ids == sorted(set(ids)), 'invalid command encoding')
        require(player == 0 and all(1 <= uid <= 3 for uid in ids) and 0 <= tick < ticks, 'invalid ownership/tick')
        require(order in range(7), 'invalid order')
        commands.append(dict(tick=tick,sequence=seq,order=order,x=x,z=z,units=ids))
    require(pos == len(raw), 'trailing replay data')
    accepted = [r for r in report['inputs'] if r['accepted']]
    require(len(accepted) == len(commands), 'input/replay count mismatch')
    for observed,command in zip(accepted,commands,strict=True):
        require(observed['event_tick'] == command['tick'] and all(observed[k] == command[k] for k in ('order','x','z','units')), 'input/replay command mismatch')
    require({0,4,5,6}.issubset({r['order'] for r in commands}), 'missing Stop/Gather/Return/Build commands')
    return commands


def audit_empty_recall(report, commands):
    ledger = report['snapshots']
    inputs = [event for event in report['inputs'] if event['accepted']]
    matches = [i for i, event in enumerate(inputs) if event['label'] == 'empty_worker_return']
    require(len(matches) == 1 and matches[0] + 1 < len(commands), 'missing isolated empty recall')
    index = matches[0]
    recall = commands[index]
    selection = recall['units']
    require(len(selection) == 1 and recall['order'] == 5, 'expected a single worker recall')
    start = ledger[recall['tick']]
    anchor, = [s for s in start['structures'] if s['player'] == 0 and s['kind'] == 0]
    require((recall['x'], recall['z']) == (anchor['x'], anchor['z']), 'recall targeted the wrong anchor')
    actor = selection[0] - 1
    worker = start['units'][actor]
    require(worker['hp'] > 0 and worker['kind'] == 1 and worker['player'] == 0 and worker['cargo'] == 0
            and worker['order'] == 0 and not worker['returning'], 'recall requires an idle empty worker')
    next_tick = commands[index + 1]['tick']
    arrival = stopped = None
    for row in ledger[start['tick'] + 1:next_tick + 1]:
        unit = row['units'][actor]
        require(row['salvage'] == start['salvage'] and row['flux'] == start['flux']
                and unit['cargo'] == 0 and unit['hp'] == worker['hp'], 'empty recall changed resources or health')
        require(all(u == v for k, (u, v) in enumerate(zip(row['units'], start['units'], strict=True)) if k != actor),
                'empty recall affected another unit')
        if unit['order'] == 5:
            require(arrival is None and unit['returning'], 'unexpected return after arrival')
            if (unit['x'], unit['z']) == (unit['goal_x'], unit['goal_z']):
                arrival = row['tick']
            continue
        dx, dz = abs(unit['x'] - anchor['x']), abs(unit['z'] - anchor['z'])
        require(unit['order'] == 0 and not unit['returning'] and max(dx, dz) == 352
                and min(dx, dz) in (0, 352), 'empty worker failed to stop at an anchor service position')
        if stopped is None:
            stopped = row['tick']
    require(arrival is not None and stopped == arrival + 1 and arrival > start['tick'] + 1
            and (worker['x'], worker['z']) != (ledger[arrival]['units'][actor]['x'], ledger[arrival]['units'][actor]['z']),
            'missing empty return trip')
    return {'worker': worker['id'], 'order_tick': start['tick'], 'arrival_tick': arrival, 'stop_tick': stopped}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--require-empty-recall', action='store_true',
                        help='Check the command-card withdrawal of an empty worker through anchor arrival.')
    args = parser.parse_args()
    require(not (ROOT/'.voidfront-agent/STOP').exists(), 'STOP present')
    report = json.loads(args.report.read_text(encoding='utf-8'))
    summary = audit(report)
    raw = Path(report['replay_path']).read_bytes()
    summary['commands'] = replay_commands(raw, report)
    if args.require_empty_recall:
        summary['empty_recall'] = audit_empty_recall(report, summary['commands'])
    rejected = []
    for name in ('trace-gap','ledger-gap','resource-created','cargo-overflow','deposit-refill','progress-jump','worker-overlap'):
        bad = copy.deepcopy(report)
        if name == 'trace-gap': bad['trace'].pop(1)
        elif name == 'ledger-gap': bad['snapshots'].pop(1)
        elif name == 'resource-created': bad['snapshots'][1]['salvage'][0] += 1
        elif name == 'cargo-overflow': bad['snapshots'][1]['units'][0]['cargo'] = 11
        elif name == 'deposit-refill': bad['snapshots'][1]['deposits'][0]['remaining'] += 1
        elif name == 'progress-jump':
            row = next(r for r in bad['snapshots'] if len(r['structures']) > 2)
            row['structures'][-1]['build_ticks'] += 2
        else:
            bad['snapshots'][1]['units'][0]['x'] = bad['snapshots'][1]['units'][1]['x']
            bad['snapshots'][1]['units'][0]['z'] = bad['snapshots'][1]['units'][1]['z']
        try: audit(bad)
        except AssertionError: rejected.append(name)
        else: raise AssertionError(f'accepted corrupted evidence {name}')
    args.out.mkdir(parents=True, exist_ok=False)
    replay = args.out/'client.vfr'; replay.write_bytes(raw)
    golden = ''.join(f'{r["tick"]} {int(r["hash"],16)}\n' for r in report['trace'][1:])
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    binaries = {c: ROOT/f'build/windows/sim/{c}/voidfront_headless.exe' for c in ('Debug','Release')}
    fingerprints = {c:digest(p) for c,p in binaries.items()}
    if args.require_empty_recall:
        recall = summary['empty_recall']
        commands = [dict(c, player=0) for c in summary['commands']]
        for t in sorted({recall['order_tick'] + 1, recall['arrival_tick'] - 1, recall['arrival_tick'], recall['stop_tick']}):
            recording = args.out/f'recall-{t}.vfr'
            recording.write_bytes(prefix(raw, commands, t))
            for config, exe in binaries.items():
                metrics = args.out/f'recall-{t}-{config}.json'
                subprocess.run([exe, '--replay', recording, '--metrics', metrics], check=True,
                               capture_output=True, timeout=60)
                compare_snapshot(report['snapshots'][t], json.loads(metrics.read_text()), f'{config} recall {t}')
    summary.update(started_utc=datetime.now(timezone.utc).isoformat(), report_sha256=digest(args.report),
                   replay_sha256=digest(replay), binaries=fingerprints, rejected_corruptions=rejected, runs=[])
    for config,repeats in (('Debug',1),('Release',10)):
        for run in range(repeats):
            require(not (ROOT/'.voidfront-agent/STOP').exists(), 'STOP present')
            name = f'{config}-{run}'
            trace, metrics = args.out/f'{name}.trace',args.out/f'{name}.json'
            result = subprocess.run([str(binaries[config]),'--replay',str(replay),'--trace',str(trace),'--metrics',str(metrics)],
                capture_output=True,text=True,timeout=60,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
            (args.out/f'{name}.log').write_text(result.stdout+result.stderr)
            require(result.returncode == 0, f'{name}: {result.stderr}')
            require(trace.read_text() == golden, f'{name}: client/replay divergence')
            data = json.loads(metrics.read_text())
            for key in ('salvage','structures','deposits'):
                require(data[key] == report['final_snapshot'][key], f'{name}: {key} differs')
            summary['runs'].append(name)
    require(fingerprints == {c:digest(p) for c,p in binaries.items()}, 'binaries changed during verification')
    summary['completed_utc'] = datetime.now(timezone.utc).isoformat()
    summary['limitations'] = 'Scripted offline economy only; no human play, production, economic AI, full match, economy networking or shipping approval.'
    (args.out/'summary.json').write_text(json.dumps(summary,indent=2))
    print(json.dumps(summary))


if __name__ == '__main__': main()
