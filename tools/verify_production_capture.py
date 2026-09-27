"""Independently audit packaged production, geometry and canonical replay evidence."""
import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

from verify_economy_capture import BOUNDS, ROOT, TERRAIN, require
from verify_navigation import segment_clear, midpoint_enters_open_rect, point_clear


def replay_commands(raw, report):
    require(len(raw) >= 36 and raw[:4] == b'VFR\3', 'invalid replay header')
    protocol, seed, count, ticks, n, content, map_id = struct.unpack_from('<5IQI', raw, 4)
    initial = report['initial_snapshot']
    require((protocol, seed, count, ticks, content, map_id) ==
            (initial['protocol'], 1, 3, report['final_snapshot']['tick'],
             int(initial['content_id'], 16), 2), 'replay setup mismatch')
    pos, commands = 36, []
    for _ in range(n):
        require(pos+4 <= len(raw), 'truncated replay length')
        size, = struct.unpack_from('<I', raw, pos)
        pos += 4
        frame = raw[pos:pos+size]
        pos += size
        require(size >= 26 and len(frame) == size and frame[:4] == b'VFC\1', 'invalid frame')
        tick, seq, player, order, x, z, ids_count = struct.unpack_from('<IIBBIII', frame, 4)
        require(size == 26+4*ids_count, 'invalid command size')
        ids = list(struct.unpack_from('<'+'I'*ids_count, frame, 26))
        require(ids and ids == sorted(set(ids)), 'invalid canonical ids')
        require(player == 0 and 0 <= tick < ticks and order in range(9), 'invalid command')
        if commands:
            require(seq > commands[-1]['sequence'] and tick >= commands[-1]['tick'], 'noncanonical command order')
        state = report['snapshots'][tick]
        actors = state['structures'] if order in (7, 8) else state['units']
        require(all(any(a['id'] == uid and a['player'] == player for a in actors) for uid in ids), 'invalid actor ownership')
        if order in (7, 8):
            require(len(ids) == 1 and x == z == 0, 'production operand encoding')
        commands.append(dict(tick=tick, sequence=seq, player=player, order=order, x=x, z=z, units=ids))
    require(pos == len(raw), 'trailing replay data')
    accepted = [r for r in report['inputs'] if r['accepted']]
    require(len(accepted) == len(commands), 'input/replay count mismatch')
    for observed, command in zip(accepted, commands, strict=True):
        require(observed['event_tick'] == command['tick'] and
                all(observed[k] == command[k] for k in ('order', 'x', 'z', 'units')), 'input/replay command mismatch')
    require({0, 1, 4, 6, 7, 8}.issubset({c['order'] for c in commands}), 'missing production fixture commands')
    return commands


def audit(report, commands):
    require(report['ok'] and not report['errors'], 'packaged fixture failed')
    initial, final = report['initial_snapshot'], report['final_snapshot']
    require(initial['map_id'] == final['map_id'] == 2 and initial['tick'] == 0, 'wrong setup')
    ticks = final['tick']
    ledger, trace = report['snapshots'], report['trace']
    require([r['tick'] for r in ledger] == list(range(ticks+1)), 'snapshot gap')
    require([r['tick'] for r in trace] == list(range(ticks+1)), 'hash trace gap')
    require(ledger[0] == initial and ledger[-1] == final, 'ledger endpoint mismatch')
    total = sum(initial['salvage']) + sum(d['remaining'] for d in initial['deposits'])
    total += sum(u['cargo'] for u in initial['units'])
    require(all(u['kind'] == 1 for u in initial['units']), 'initial units are not workers')
    by_tick = {}
    for c in commands:
        by_tick.setdefault(c['tick'], []).append(c)
    prior = initial
    rows = purchases = refunds = spawns = production_ticks = blocked_ticks = construction_ticks = 0
    insufficient = False
    for row, trace_row in zip(ledger, trace, strict=True):
        tick = row['tick']
        require(row['hash'] == trace_row['hash'], f'tick {tick}: hash binding')
        require((row['foundry_cost'], row['strider_cost'], row['train_ticks'],
                 row['production_queue_limit'], row['population_cap']) == (100, 50, 100, 5, 12), 'rules drift')
        require(len(row['salvage']) == 2 and min(row['salvage']) >= 0, 'negative resources')
        require([u['id'] for u in row['units']] == list(range(1, len(row['units'])+1)), 'unstable unit ids')
        require(len(row['units']) >= len(prior['units']), 'unit erased')
        require([s['id'] for s in row['structures']] == list(range(1, len(row['structures'])+1)), 'unstable structure ids')
        require([d['id'] for d in row['deposits']] == [1, 2], 'deposit roster changed')
        foundries = [s for s in row['structures'] if s['kind'] == 1]
        bought = sum(u['kind'] == 0 for u in row['units'])
        queued = sum(s['production_queue'] for s in row['structures'])
        observed = sum(row['salvage']) + sum(d['remaining'] for d in row['deposits'])
        observed += sum(u['cargo'] for u in row['units']) + 100*len(foundries) + 50*(bought+queued)
        require(observed == total, f'tick {tick}: resource conservation')
        for i, d in enumerate(row['deposits']):
            old = prior['deposits'][i]
            require(all(d[k] == old[k] for k in ('id', 'x', 'z')), 'deposit identity changed')
            require(0 <= d['remaining'] <= old['remaining'], 'deposit replenished')
        for player in range(2):
            used = sum(u['player'] == player and u['hp'] > 0 for u in row['units'])
            reserved = sum(s['production_queue'] for s in row['structures'] if s['player'] == player)
            require((row['population_used'][player], row['population_reserved'][player]) == (used, reserved), 'population accounting')
            require(used+reserved <= 12, 'population overflow')
        obstacles = TERRAIN + [(s['x']-320, s['z']-320, s['x']+320, s['z']+320) for s in row['structures'] if s['hp'] > 0]
        obstacles += [(d['x']-192, d['z']-192, d['x']+192, d['z']+192) for d in row['deposits']]
        added = row['units'][len(prior['units']):]
        for i, unit in enumerate(row['units']):
            require(unit['kind'] in (0, 1) and 0 <= unit['cargo'] <= 10, 'unit/cargo bounds')
            require(unit['hp'] > 0, 'this offline production fixture must retain all units')
            old = prior['units'][i] if i < len(prior['units']) else unit
            require(all(unit[k] == old[k] for k in ('id', 'kind', 'player')), 'unit identity changed')
            a, b = (old['x'], old['z']), (unit['x'], unit['z'])
            require(sum((p-q)**2 for p, q in zip(a, b)) <= 1024, 'unit speed overflow')
            require(segment_clear(a, b, BOUNDS, obstacles), f'tick {tick}: unit {unit["id"]} static sweep')
            for j in range(i):
                other = row['units'][j]
                previous = prior['units'][j] if j < len(prior['units']) else other
                start = (a[0]-previous['x'], a[1]-previous['z'])
                end = (b[0]-other['x'], b[1]-other['z'])
                require(not midpoint_enters_open_rect(start, end, (-128, -128, 128, 128)), 'moving unit/spawn sweep overlap')
            rows += 1
        tick_commands = by_tick.get(tick-1, []) if tick else []
        economy_commands = [c for c in tick_commands if c['order'] >= 4]
        require(len(economy_commands) <= 1, 'fixture needs one economic command per tick for result audit')
        births = 0
        for site in row['structures']:
            old = next((s for s in prior['structures'] if s['id'] == site['id']), None)
            if old:
                require(all(site[k] == old[k] for k in ('id', 'kind', 'player', 'x', 'z')), 'structure identity changed')
            before = old['build_ticks'] if old else 0
            require(0 <= site['build_ticks'] <= 100 and 0 <= site['build_ticks']-before <= 1, 'construction progression')
            construction_ticks += site['build_ticks']-before
            q, progress = (old['production_queue'], old['production_ticks']) if old else (0, 0)
            require(0 <= site['production_queue'] <= 5 and 0 <= site['production_ticks'] <= 100, 'queue bounds')
            for c in economy_commands:
                if c['order'] not in (7, 8) or c['units'] != [site['id']]:
                    continue
                p = c['player']
                require(row['result_sequences'][p] == c['sequence'], 'unbound application result')
                result = row['command_results'][p]
                if c['order'] == 7:
                    expected = (6 if site['kind'] != 1 else 7 if before < 100 else
                                8 if q >= 5 else 9 if sum((prior['population_used'][p], prior['population_reserved'][p])) >= 12 else
                                3 if prior['salvage'][p] < 50 else 1)
                    require(result == expected, 'wrong train result')
                    if result == 1:
                        q += 1
                        purchases += 1
                    insufficient |= result == 3
                else:
                    expected = 6 if site['kind'] != 1 else 11 if q == 0 else 1
                    require(result == expected, 'wrong cancel result')
                    if result == 1:
                        q -= 1
                        refunds += 1
                        if not q:
                            progress = 0
                # This fixture stops harvesting before construction/production:
                # isolate the buyer's exact debit/refund, not just global funds.
                require(all(u['order'] not in (4, 5) for u in prior['units']), 'production debit audit needs stopped harvesters')
                expected_delta = (-50 if c['order'] == 7 else 50) if result == 1 else 0
                require(row['salvage'][p]-prior['salvage'][p] == expected_delta, 'wrong production debit/refund')
            if tick and site['kind'] == 1 and site['build_ticks'] == 100 and q:
                next_progress = min(progress+1, 100)
                if site['production_queue'] == q-1:
                    require(next_progress == 100 and site['production_ticks'] == 0 and not site['spawn_blocked'], 'premature/invalid spawn')
                    require(births < len(added), 'missing produced unit')
                    new = added[births]
                    dx, dz = abs(new['x']-site['x']), abs(new['z']-site['z'])
                    require(new['player'] == site['player'] and max(dx, dz) == 352 and min(dx, dz) in (0, 352), 'spawn source/owner mismatch')
                    births += 1
                else:
                    require(site['production_queue'] == q and site['production_ticks'] == next_progress, 'production progression')
                    require(bool(site['spawn_blocked']) == (next_progress == 100), 'blocked-spawn state')
                    blocked_ticks += bool(site['spawn_blocked'])
                    if site['spawn_blocked']:
                        # Independent feasibility, without reproducing exit priority.
                        occupied = prior['units'] + added[:births]
                        for dx in (-352, 0, 352):
                            for dz in (-352, 0, 352):
                                if dx == dz == 0: continue
                                point = (site['x']+dx, site['z']+dz)
                                free = point_clear(point, BOUNDS, obstacles) and not any(
                                    u['hp'] > 0 and abs(u['x']-point[0]) < 128 and abs(u['z']-point[1]) < 128 for u in occupied)
                                require(not free, 'production stalled with available spawn exit')
                production_ticks += next_progress > progress
            else:
                require(site['production_queue'] == q and site['production_ticks'] == progress == 0 and not site['spawn_blocked'], 'idle production changed')
        require(births == len(added), 'spawn count does not match queue consumption')
        require(all(u['kind'] == 0 and u['cargo'] == 0 for u in added), 'invalid produced unit')
        spawns += births
        prior = row
    require(purchases >= 3 and refunds >= 1 and spawns >= 2 and insufficient, 'missing purchase/refund/spawn/rejection coverage')
    require(construction_ticks >= 100, 'missing constructed Foundry')
    require(any(u['id'] > 6 and (u['x'], u['z']) != (next(r for r in ledger if len(r['units']) >= u['id'])['units'][u['id']-1]['x'],
                next(r for r in ledger if len(r['units']) >= u['id'])['units'][u['id']-1]['z']) for u in final['units']), 'produced unit never moved')
    return dict(ticks=ticks, unit_rows=rows, conserved_salvage=total, purchases=purchases,
                refunds=refunds, spawns=spawns, production_progress_ticks=production_ticks,
                blocked_spawn_ticks=blocked_ticks, construction_progress_ticks=construction_ticks)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    require(not (ROOT/'.voidfront-agent/STOP').exists(), 'STOP present')
    report = json.loads(args.report.read_text(encoding='utf-8'))
    raw = Path(report['replay_path']).read_bytes()
    commands = replay_commands(raw, report)
    summary = audit(report, commands)
    rejected = []
    for name in ('trace-gap', 'ledger-gap', 'resource-created', 'queue-overflow', 'progress-jump', 'population-lie', 'spawn-overlap'):
        bad = copy.deepcopy(report)
        if name == 'trace-gap': bad['trace'].pop(1)
        elif name == 'ledger-gap': bad['snapshots'].pop(1)
        elif name == 'resource-created': bad['snapshots'][1]['salvage'][0] += 1
        elif name == 'population-lie': bad['snapshots'][1]['population_used'][0] += 1
        else:
            row = next(r for r in bad['snapshots'] if len(r['units']) > 6)
            if name == 'queue-overflow': row['structures'][-1]['production_queue'] = 6
            elif name == 'progress-jump': row['structures'][-1]['production_ticks'] = 102
            else:
                row['units'][-1]['x'] = row['units'][0]['x']
                row['units'][-1]['z'] = row['units'][0]['z']
        try: audit(bad, commands)
        except AssertionError: rejected.append(name)
        else: raise AssertionError(f'accepted corrupted evidence {name}')
    args.out.mkdir(parents=True, exist_ok=False)
    replay = args.out/'client.vfr'
    replay.write_bytes(raw)
    golden = ''.join(f'{r["tick"]} {int(r["hash"],16)}\n' for r in report['trace'][1:])
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    binaries = {c: ROOT/f'build/windows/sim/{c}/voidfront_headless.exe' for c in ('Debug', 'Release')}
    fingerprints = {c: digest(p) for c, p in binaries.items()}
    summary.update(started_utc=datetime.now(timezone.utc).isoformat(), report_sha256=digest(args.report),
                   replay_sha256=digest(replay), binaries=fingerprints, commands=commands,
                   rejected_corruptions=rejected, runs=[])
    for config, repeats in (('Debug', 1), ('Release', 10)):
        for run in range(repeats):
            require(not (ROOT/'.voidfront-agent/STOP').exists(), 'STOP present')
            name = f'{config}-{run}'
            trace, metrics = args.out/f'{name}.trace', args.out/f'{name}.json'
            result = subprocess.run([str(binaries[config]), '--replay', str(replay), '--trace', str(trace), '--metrics', str(metrics)],
                capture_output=True, text=True, timeout=60, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            (args.out/f'{name}.log').write_text(result.stdout+result.stderr)
            require(result.returncode == 0, f'{name}: {result.stderr}')
            require(trace.read_text() == golden, f'{name}: client/replay divergence')
            data = json.loads(metrics.read_text())
            for key in ('salvage', 'structures', 'deposits', 'population_used', 'population_reserved', 'population_cap'):
                require(data[key] == report['final_snapshot'][key], f'{name}: {key} differs')
            summary['runs'].append(name)
    require(fingerprints == {c: digest(p) for c, p in binaries.items()}, 'binaries changed during audit')
    summary['completed_utc'] = datetime.now(timezone.utc).isoformat()
    summary['limitations'] = 'Scripted offline production only. No human play, economic AI, full matches, production networking, broad blocked-spawn coverage, performance acceptance or shipping approval.'
    (args.out/'summary.json').write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
