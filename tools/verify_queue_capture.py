"""Independently audit queued InputEvents, FIFO transitions, geometry and replays."""
import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

from verify_economy_capture import ROOT, require, map_geometry
from verify_navigation import segment_clear, midpoint_enters_open_rect


LABELS = ['idle_shift_move', 'fifo_second', 'fifo_attack', 'fifo_fourth', 'fifo_fifth',
          'overflow_ignored', 'stop_active', 'stop_tail', 'stop_clear', 'hold_active',
          'hold_tail', 'hold_clear', 'retarget_active', 'retarget_tail', 'plain_retarget',
          'restart_active', 'restart_tail']
ORDERS = [11, 11, 12, 11, 11, 11, 1, 11, 0, 1, 11, 3, 1, 11, 1, 1, 11]


def replay_commands(raw, report):
    require(len(raw) >= 36 and raw[:4] == b'VFR\3', 'invalid VFR3 header')
    protocol, seed, count, ticks, n, content, map_id = struct.unpack_from('<5IQI', raw, 4)
    initial = report['initial_snapshot']
    require((protocol, seed, count, ticks, content, map_id) ==
            (initial['protocol'], 1, 3, report['final_snapshot']['tick'],
             int(initial['content_id'], 16), 2), 'replay setup mismatch')
    pos, commands = 36, []
    for _ in range(n):
        require(pos + 4 <= len(raw), 'truncated frame length')
        size, = struct.unpack_from('<I', raw, pos)
        pos += 4
        frame = raw[pos:pos + size]
        pos += size
        require(size >= 26 and len(frame) == size and frame[:4] == b'VFC\1', 'invalid command frame')
        tick, seq, player, order, x, z, ids_count = struct.unpack_from('<IIBBIII', frame, 4)
        require(size == 26 + 4 * ids_count and ids_count == 1, 'invalid actor encoding')
        ids = list(struct.unpack_from('<I', frame, 26))
        require(ids == [1] and player == 0 and 0 <= tick < ticks and order in (0, 1, 3, 11, 12), 'invalid command')
        require(seq == len(commands) + 1, 'sequence gap')
        if commands:
            require(tick >= commands[-1]['tick'], 'unordered command ticks')
        if order in (0, 3):
            require(x == z == 0, 'Stop/Hold operands')
        commands.append(dict(tick=tick, sequence=seq, player=player, order=order, x=x, z=z, units=ids))
    require(pos == len(raw), 'trailing replay data')
    inputs = report['inputs']
    require(len(inputs) == len(commands) == len(LABELS), 'input/replay count mismatch')
    require([i['label'] for i in inputs] == LABELS and [i['order'] for i in inputs] == ORDERS, 'missing fixture command coverage')
    for observed, command in zip(inputs, commands, strict=True):
        require(observed['accepted'] and observed['event_tick'] == command['tick'] and
                all(observed[k] == command[k] for k in ('order', 'x', 'z', 'units')), 'input/replay mismatch')
    return commands


def audit(report, commands):
    require(report['ok'] and not report['errors'] and report['mode'] == 'queue', 'packaged fixture failed')
    initial, final = report['initial_snapshot'], report['final_snapshot']
    ticks = final['tick']
    require(initial['tick'] == 0 and initial['map_id'] == final['map_id'] == 2 and 0 < ticks < 1000, 'fixture bounds')
    ledger, trace = report['snapshots'], report['trace']
    require([r['tick'] for r in ledger] == list(range(ticks + 1)), 'snapshot gap')
    require([r['tick'] for r in trace] == list(range(ticks + 1)), 'trace gap')
    require(ledger[0] == initial and ledger[-1] == final, 'ledger endpoint mismatch')
    require((initial['units'][0]['x'], initial['units'][0]['z']) == (1664, 2944), 'canonical worker start changed')
    require(all(u['kind'] == 1 and u['order'] == 0 and not u['order_queue'] for u in initial['units']), 'initial worker state')
    by_tick = {}
    for c in commands:
        by_tick.setdefault(c['tick'], []).append(c)
    bounds, terrain = map_geometry(initial)
    obstacles = terrain + [(s['x']-320, s['z']-320, s['x']+320, s['z']+320) for s in initial['structures']]
    obstacles += [(d['x']-192, d['z']-192, d['x']+192, d['z']+192) for d in initial['deposits']]
    arrivals, promotions, capped, clearings, unit_rows = [], [], 0, [], 0
    for tick, (row, hash_row) in enumerate(zip(ledger, trace, strict=True)):
        require(row['hash'] == hash_row['hash'], 'hash binding')
        require(not row['enemy_ai'] and row['replay_complete'], 'fixture AI/evidence mode changed')
        require(row['order_queue_limit'] == 4, 'queue cap drift')
        require(len(row['units']) == 6 and row['structures'] == initial['structures'] and row['deposits'] == initial['deposits'], 'passive fixture changed roster/economy objects')
        require(row['salvage'] == initial['salvage'] and row['flux'] == initial['flux'] and row['winner'] == -1, 'passive fixture changed economy/outcome')
        prior = ledger[max(0, tick-1)]
        for old, unit in zip(prior['units'], row['units'], strict=True):
            require(all(unit[k] == old[k] for k in ('id', 'kind', 'player', 'hp', 'cargo')), 'unit identity/health/cargo changed')
            require(len(unit['order_queue']) <= 4 and all(leg['order'] in (1, 2) for leg in unit['order_queue']), 'queue bounds/type')
            a, b = (old['x'], old['z']), (unit['x'], unit['z'])
            require(sum((p-q)**2 for p,q in zip(a,b)) <= 1024, 'movement speed overflow')
            require(segment_clear(a,b,bounds,obstacles), f'tick {tick}: static sweep')
            if unit['id'] != 1:
                require(unit == initial['units'][unit['id']-1], 'uncommanded worker changed')
            for other in row['units'][:unit['id']-1]:
                previous = prior['units'][other['id']-1]
                start = (a[0]-previous['x'], a[1]-previous['z'])
                end = (b[0]-other['x'], b[1]-other['z'])
                require(not midpoint_enters_open_rect(start,end,(-128,-128,128,128)), 'relative unit sweep overlap')
            unit_rows += 1
        if not tick:
            continue
        old, unit = prior['units'][0], row['units'][0]
        active, goal = old['order'], (old['goal_x'], old['goal_z'])
        queue = copy.deepcopy(old['order_queue'])
        point = (old['x'], old['z'])
        for c in by_tick.get(tick-1, []):
            if c['order'] in (11, 12):
                leg = dict(order=1 if c['order'] == 11 else 2, x=c['x'], z=c['z'])
                if active in (1, 2):
                    if len(queue) < 4:
                        queue.append(leg)
                    else:
                        capped += 1
                else:
                    queue = []
                    active, goal = leg['order'], (leg['x'],leg['z'])
            else:
                if queue:
                    clearings.append(c['order'])
                queue = []
                active = c['order']
                goal = point if active in (0,3) else (c['x'],c['z'])
            require(row['result_sequences'][0] == by_tick[tick-1][-1]['sequence'] and row['command_results'][0] == 1, 'unbound command result')
        if queue and active in (1,2) and point == goal:
            promotions.append(dict(tick=tick, arrived=list(goal), next=queue[0]))
            leg = queue.pop(0)
            active, goal = leg['order'], (leg['x'],leg['z'])
        if active == 1 and not queue and (unit['x'],unit['z']) == goal:
            active = 0
        require(unit['order_queue'] == queue, f'tick {tick}: queue FIFO/clear/cap mismatch')
        require((unit['goal_x'],unit['goal_z']) == goal and unit['order'] == active, f'tick {tick}: active goal/order mismatch')
        if old['order'] in (0,3) and not by_tick.get(tick-1):
            require((unit['x'],unit['z']) == point, 'Stop/Hold drift')
        if point != goal and (unit['x'],unit['z']) == goal:
            arrivals.append(dict(tick=tick, x=goal[0], z=goal[1]))
    require(capped == 1 and set(clearings) == {0,1,3}, 'missing cap/clear coverage')
    fifo = commands[:5]
    expected_goals = [(c['x'],c['z']) for c in fifo]
    drain_tick = next(p['tick'] for p in report['phases'] if p['label'] == 'drained')
    observed = [(a['x'],a['z']) for a in arrivals if a['tick'] <= drain_tick]
    require(observed == expected_goals and len(promotions) == 4, 'five exact FIFO arrivals not established')
    overflow = (commands[5]['x'],commands[5]['z'])
    require(overflow not in expected_goals and not any(
        (r['units'][0]['goal_x'],r['units'][0]['goal_z']) == overflow
        for r in ledger[commands[5]['tick']+1:drain_tick+1]), 'overflow goal became active')
    stationary_windows = []
    for index in (8,11):
        command = commands[index]
        first, end = command['tick']+1, commands[index+1]['tick']
        require(end-first+1 >= 5, 'insufficient post Stop/Hold observation')
        positions = [(r['units'][0]['x'],r['units'][0]['z']) for r in ledger[first:end+1]]
        require(len(set(positions)) == 1, 'post Stop/Hold drift')
        stationary_windows.append(dict(order=command['order'],ticks=len(positions),position=list(positions[0])))
    phases = report['phases']
    phase_names = ['selected_idle','filled','overflow','deselected','reselected','drained','before_stop','stopped',
                   'before_hold','held','before_retarget','retargeted','retarget_arrived','before_restart','restarted']
    require([p['label'] for p in phases] == phase_names, 'missing presentation phases')
    for phase in phases:
        if phase['label'] == 'restarted':
            state = report['restart_snapshot']
        else:
            require(0 <= phase['tick'] <= ticks, 'phase tick outside ledger')
            state = ledger[phase['tick']]
        expected = []
        require(phase['selected'] in ([],[1]), 'invalid fixture selection')
        for u in state['units']:
            if u['id'] in phase['selected'] and u['player'] == 0 and u['hp'] > 0 and u['order_queue']:
                expected.append(dict(id=u['id'],points=[dict(order=u['order'],x=u['goal_x'],z=u['goal_z'])] + u['order_queue']))
        require(phase['paths'] == expected, 'presentation paths differ from selected canonical goals')
        if phase['label'] in ('filled','overflow'):
            require(len(expected) == 1 and len(expected[0]['points']) == 5, 'full path presentation missing')
        if phase['label'] in ('before_stop','before_hold','before_retarget','before_restart','reselected'):
            require(expected, 'queued path presentation missing')
        if phase['label'] in ('deselected','drained','stopped','held','retargeted','retarget_arrived','restarted'):
            require(not expected, 'obsolete presentation retained')
        if phase['label'] == 'deselected':
            require(not phase['selected'] and state['units'][0]['order_queue'], 'deselection lacks live queued order')
    require(report['restart_snapshot'] == initial, 'restart snapshot differs from initial')
    captures = report['captures']
    require([c['label'] for c in captures] == ['filled','drained','stopped'] and all(c['error'] == 0 for c in captures), 'missing screenshots')
    return dict(ticks=ticks, unit_rows=unit_rows, exact_arrivals=arrivals, fifo_promotions=promotions,
                capped_legs=capped, queue_clearing_orders=clearings, stationary_windows=stationary_windows,
                presentation_phases=len(phases))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report',type=Path)
    parser.add_argument('--out',type=Path,required=True)
    args = parser.parse_args()
    require(not (ROOT/'.voidfront-agent/STOP').exists(), 'STOP present')
    report = json.loads(args.report.read_text(encoding='utf-8'))
    raw = Path(report['replay_path']).read_bytes()
    commands = replay_commands(raw,report)
    summary = audit(report,commands)
    summary['screenshots'] = []
    for capture in report['captures']:
        path = Path(capture['path'])
        pixels = path.read_bytes()
        require(pixels[:8] == b'\x89PNG\r\n\x1a\n' and len(pixels) > 100, 'missing/invalid PNG')
        summary['screenshots'].append(dict(label=capture['label'],path=str(path),sha256=hashlib.sha256(pixels).hexdigest()))
    rejected = []
    for name in ('trace-gap','ledger-gap','input-omission','input-coordinate','queue-fifo','queue-overflow','active-goal','unit-speed','path-coordinate','restart-queue','cap-rule'):
        bad = copy.deepcopy(report)
        if name == 'trace-gap': bad['trace'].pop(1)
        elif name == 'ledger-gap': bad['snapshots'].pop(1)
        elif name == 'input-omission': bad['inputs'].pop(1)
        elif name == 'input-coordinate': bad['inputs'][1]['x'] += 1
        elif name == 'path-coordinate': bad['phases'][1]['paths'][0]['points'][1]['x'] += 1
        elif name == 'restart-queue': bad['restart_snapshot']['units'][0]['order_queue'].append(dict(order=1,x=1,z=1))
        elif name == 'cap-rule': bad['snapshots'][1]['order_queue_limit'] = 5
        else:
            row = next(r for r in bad['snapshots'] if len(r['units'][0]['order_queue']) == 4)
            u = row['units'][0]
            if name == 'queue-fifo': u['order_queue'][0],u['order_queue'][1] = u['order_queue'][1],u['order_queue'][0]
            elif name == 'queue-overflow': u['order_queue'].append(dict(order=1,x=1,z=1))
            elif name == 'active-goal': u['goal_x'] += 1
            else: u['x'] += 1000
        try:
            replay_commands(raw,bad)
            audit(bad,commands)
        except AssertionError:
            rejected.append(name)
        else:
            raise AssertionError(f'accepted corrupted evidence {name}')
    for name in ('replay-truncation','replay-trailing','replay-order','replay-sequence','replay-coordinate'):
        bad_raw = bytearray(raw)
        if name == 'replay-truncation': bad_raw = bad_raw[:-1]
        elif name == 'replay-trailing': bad_raw.append(0)
        elif name == 'replay-order': bad_raw[53] = 12
        elif name == 'replay-sequence': struct.pack_into('<I',bad_raw,48,2)
        else: struct.pack_into('<I',bad_raw,54,commands[0]['x']+1)
        try:
            replay_commands(bad_raw,report)
        except AssertionError:
            rejected.append(name)
        else:
            raise AssertionError(f'accepted corrupted evidence {name}')
    args.out.mkdir(parents=True,exist_ok=False)
    replay = args.out/'client.vfr'
    replay.write_bytes(raw)
    golden = ''.join(f'{r["tick"]} {int(r["hash"],16)}\n' for r in report['trace'][1:])
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    binaries = {c:ROOT/f'build/windows/sim/{c}/voidfront_headless.exe' for c in ('Debug','Release')}
    fingerprints = {c:digest(p) for c,p in binaries.items()}
    summary.update(started_utc=datetime.now(timezone.utc).isoformat(),report_sha256=digest(args.report),
                   replay_sha256=digest(replay),binaries=fingerprints,commands=commands,rejected_corruptions=rejected,runs=[])
    for config,repeats in (('Debug',1),('Release',10)):
        for run in range(repeats):
            require(not (ROOT/'.voidfront-agent/STOP').exists(), 'STOP present')
            name = f'{config}-{run}'
            trace,metrics = args.out/f'{name}.trace',args.out/f'{name}.json'
            result = subprocess.run([str(binaries[config]),'--replay',str(replay),'--trace',str(trace),'--metrics',str(metrics)],
                capture_output=True,text=True,timeout=60,creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            (args.out/f'{name}.log').write_text(result.stdout+result.stderr)
            require(result.returncode == 0,f'{name}: {result.stderr}')
            require(trace.read_text() == golden,f'{name}: replay/client divergence')
            summary['runs'].append(name)
    require(fingerprints == {c:digest(p) for c,p in binaries.items()}, 'binaries changed during audit')
    summary['completed_utc'] = datetime.now(timezone.utc).isoformat()
    summary['limitations'] = 'Scripted passive offline queued worker inputs only; no human, network, responsiveness, full-match, quality or shipping acceptance.'
    (args.out/'summary.json').write_text(json.dumps(summary,indent=2))
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
