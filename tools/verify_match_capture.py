"""Audit the packaged two-player economic match and replay every observed tick."""
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


def commands_from_replay(raw, report):
    require(len(raw) >= 36 and raw[:4] == b'VFR\3', 'replay header')
    protocol, seed, count, ticks, n, content, map_id = struct.unpack_from('<5IQI', raw, 4)
    initial, final = report['initial_snapshot'], report['final_snapshot']
    bounds, terrain = map_geometry(initial)
    require((protocol, seed, count, ticks, content, map_id) ==
            (initial['protocol'], 1, 3, final['tick'], int(initial['content_id'], 16), 2), 'replay setup')
    pos, commands, sequences = 36, [], [0, 0]
    for _ in range(n):
        require(pos + 4 <= len(raw), 'truncated frame length')
        size, = struct.unpack_from('<I', raw, pos)
        pos += 4
        frame = raw[pos:pos+size]
        pos += size
        require(size >= 30 and len(frame) == size and frame[:4] == b'VFC\1', 'command frame')
        tick, seq, player, order, x, z, count = struct.unpack_from('<IIBBIII', frame, 4)
        require(count in range(1, 257) and size == 26 + 4*count, 'command ids size')
        ids = list(struct.unpack_from('<'+'I'*count, frame, 26))
        require(ids == sorted(set(ids)) and player in (0, 1) and order in range(9), 'command canonical fields')
        require(0 <= tick < ticks and 0 <= x < initial['width']*256 and 0 <= z < initial['height']*256, 'command bounds')
        require(seq > sequences[player], 'player sequence')
        sequences[player] = seq
        if commands:
            prior = commands[-1]
            require((tick, player, seq) > (prior['tick'], prior['player'], prior['sequence']), 'canonical replay ordering')
        state = report['snapshots'][tick]
        actors = state['structures'] if order in (7, 8) else state['units']
        require(all(any(a['id'] == uid and a['player'] == player for a in actors) for uid in ids), 'actor ownership')
        commands.append(dict(tick=tick, sequence=seq, player=player, order=order, x=x, z=z, units=ids))
    require(pos == len(raw), 'trailing bytes')
    human = [c for c in commands if c['player'] == 0]
    inputs = [i for i in report['inputs'] if i['accepted']]
    require(len(human) == len(inputs), 'human input coverage')
    for command, observed in zip(human, inputs, strict=True):
        require(command['tick'] == observed['event_tick'] and all(command[k] == observed[k] for k in ('order', 'x', 'z', 'units')), 'human input binding')
    for player in (0, 1):
        require({2, 4, 6, 7}.issubset({c['order'] for c in commands if c['player'] == player}), 'missing player/AI economy and attack commands')
    return commands



def audit_roles(report, commands):
    events, rows = report.get('role_selection_events', []), report['snapshots']
    require(len(events) >= 7, 'missing role selection evidence')
    for event in events:
        before, after = event['before'], event['after']
        require(before == rows[before['tick']] and after == rows[after['tick']] and event['tick'] == after['tick'], 'role snapshot binding')
        require(event['key'] in ('F1', 'F2'), 'unknown role key')
        kind = 1 if event['key'] == 'F1' else 0
        expected = [u['id'] for u in before['units'] if u['player'] == 0 and u['hp'] > 0 and u['kind'] == kind]
        require(event['selected'] == expected, 'role selection includes wrong units')
        require(not any(event['pending_after'].values()), 'role switch retained targeting')
    require(any(e['pending_before']['attack'] for e in events) and any(e['pending_before']['build'] for e in events), 'missing pending mode cancellation')
    require(any(e['key'] == 'F2' and not e['selected'] for e in events), 'missing empty army selection')
    attack_inputs = [i for i in report['inputs'] if i['label'] == 'army_hotkey_anchor_attack' and i['accepted']]
    require(len(attack_inputs) == 1, 'missing whole-army input')
    event = attack_inputs[0]
    command = next(c for c in commands if c['player'] == 0 and c['tick'] == event['event_tick'] and c['order'] == 2)
    role = [e for e in events if e['key'] == 'F2' and e['tick'] <= command['tick']][-1]
    require(command['units'] == role['selected'] and len(command['units']) >= 5, 'army input selection binding')
    before, after = rows[command['tick']], rows[command['tick']+1]
    miners = [u for u in before['units'] if u['player'] == 0 and u['kind'] == 1 and u['hp'] > 0 and u['order'] == 4]
    require(miners, 'army command has no working miners')
    for miner in miners:
        current = after['units'][miner['id']-1]
        require(miner['id'] not in command['units'] and current['hp'] > 0 and current['order'] == 4 and current['resource_id'] == miner['resource_id'], 'army order disrupted miner')
    return len(events)


def audit(report, commands):
    require(report['ok'] and not report['errors'], 'fixture failed')
    initial, final = report['initial_snapshot'], report['final_snapshot']
    bounds, terrain = map_geometry(initial)
    rows, trace = report['snapshots'], report['trace']
    require(initial['map_id'] == final['map_id'] == 2 and initial['enemy_ai'], 'active economy setup')
    require([r['tick'] for r in rows] == list(range(final['tick']+1)), 'ledger gap')
    require([r['tick'] for r in trace] == list(range(final['tick']+1)), 'trace gap')
    require(rows[0] == initial and rows[-1] == final, 'ledger endpoints')
    require(report['restart_snapshot']['hash'] == initial['hash'] and report['restart_snapshot']['tick'] == 0
            and report['restart_snapshot']['enemy_ai'], 'restart parity')
    require(report['after_restart_snapshot']['tick'] >= 80 and report['after_restart_snapshot']['enemy_ai']
            and any(u['player'] == 1 and u['order'] == 4 for u in report['after_restart_snapshot']['units']), 'AI restart')
    require(report['structure_attack_requests'], 'missing rendered structure attack requests')
    for event in report['structure_attack_requests']:
        state = rows[event['tick']]
        unit = state['units'][event['unit']-1]
        require(unit['target_structure'] == event['target_structure'] and unit['cooldown'] == event['cooldown']
                and 'attack' in event['clip'].lower(), 'rendered attack request binding')
    total = sum(initial['salvage']) + sum(d['remaining'] for d in initial['deposits']) + sum(u['cargo'] for u in initial['units'])
    prior, forfeited, unit_rows, damage_ticks = initial, 0, 0, 0
    damaged, dead = set(), set()
    for row, hash_row in zip(rows, trace, strict=True):
        tick = row['tick']
        require(row['hash'] == hash_row['hash'] and row['replay_complete'], 'trace binding/evidence incomplete')
        require(min(row['salvage']) >= 0, 'negative salvage')
        require(len(row['units']) >= len(prior['units']) and [u['id'] for u in row['units']] == list(range(1, len(row['units'])+1)), 'unit identity')
        require(len(row['structures']) >= len(prior['structures']) and [s['id'] for s in row['structures']] == list(range(1, len(row['structures'])+1)), 'structure identity')
        for player in (0, 1):
            live = sum(u['player'] == player and u['hp'] > 0 for u in row['units'])
            reserved = sum(s['production_queue'] for s in row['structures'] if s['player'] == player and s['hp'] > 0)
            require((live, reserved) == (row['population_used'][player], row['population_reserved'][player]), 'population accounting')
            require(live + reserved <= 12, 'population cap')
        for s in row['structures']:
            require(s['hp'] >= 0 and s['production_queue'] <= 5 and s['production_ticks'] <= 100, 'building bounds')
            if s['id'] <= len(prior['structures']):
                old = prior['structures'][s['id']-1]
                require(all(s[k] == old[k] for k in ('id', 'player', 'kind', 'x', 'z')), 'building changed identity')
                require(s['hp'] <= old['hp'], 'building healed/resurrected')
                if s['hp'] < old['hp']:
                    firing_budget = 0
                    for u in row['units']:
                        before = prior['units'][u['id']-1] if u['id'] <= len(prior['units']) else u
                        cooldown = before['cooldown'] if u['id'] <= len(prior['units']) else 0
                        dx, dz = max(0, abs(before['x']-s['x'])-256), max(0, abs(before['z']-s['z'])-256)
                        if before['hp'] > 0 and before['kind'] == 0 and before['player'] != s['player'] and cooldown <= 1 and u['order'] != 1 and dx*dx+dz*dz <= 768*768:
                            firing_budget += 8
                    require(old['hp']-s['hp'] <= firing_budget, f'tick {tick}: structure damage exceeds in-range firing budget')
                    damaged.add(s['id'])
                    damage_ticks += 1
                if old['hp'] > 0 and s['hp'] == 0:
                    dead.add(s['id'])
                    require(not any(c['tick'] == tick-1 and c['order'] in (7, 8) and c['units'] == [s['id']] for c in commands),
                            'fixture has simultaneous purchase/destruction requiring richer queue audit')
                    births = sum(u['player'] == s['player'] and max(abs(u['x']-s['x']), abs(u['z']-s['z'])) == 352
                                 for u in row['units'][len(prior['units']):])
                    forfeited += 50*(old['production_queue']-births)
            if s['hp'] == 0:
                require(s['production_queue'] == s['production_ticks'] == 0 and not s['spawn_blocked'], 'dead producer retains paid queue')
        observed = sum(row['salvage']) + sum(row.get('flux', [0, 0])) + 50*sum(bool(r) or bool(t) for r, t in zip(row.get('researched', [0, 0]), row.get('research_ticks', [0, 0]))) + sum(d['remaining'] for d in row['deposits']) + sum(u['cargo'] for u in row['units'])
        observed += 100*sum(s['kind'] == 1 for s in row['structures'])
        observed += 50*(sum(u['kind'] == 0 for u in row['units']) + sum(s['production_queue'] for s in row['structures'])) + forfeited
        require(observed == total, f'tick {tick}: resource conservation with tombstones and lost queues')
        # Production/movement precede end-of-tick damage: a structure destroyed
        # this tick still blocks movement, and a newly purchased one already does.
        movement_structures = prior['structures'] + row['structures'][len(prior['structures']):]
        obstacles = terrain + [(s['x']-320, s['z']-320, s['x']+320, s['z']+320) for s in movement_structures if s['hp'] > 0]
        obstacles += [(d['x']-192, d['z']-192, d['x']+192, d['z']+192) for d in row['deposits']]
        for i, u in enumerate(row['units']):
            old = prior['units'][i] if i < len(prior['units']) else u
            require(all(u[k] == old[k] for k in ('id', 'player', 'kind')), 'unit changed identity')
            require(0 <= u['hp'] <= old['hp'] and 0 <= u['cargo'] <= 10, 'unit health/cargo')
            if old['hp'] <= 0: continue
            a, b = (old['x'], old['z']), (u['x'], u['z'])
            require(sum((p-q)**2 for p, q in zip(a, b)) <= 1024, 'speed limit')
            require(segment_clear(a, b, bounds, obstacles), f'tick {tick} unit {u["id"]}: static sweep')
            for j in range(i):
                v = row['units'][j]
                previous = prior['units'][j] if j < len(prior['units']) else v
                if previous['hp'] <= 0: continue
                start, end = (a[0]-previous['x'], a[1]-previous['z']), (b[0]-v['x'], b[1]-v['z'])
                require(not midpoint_enters_open_rect(start, end, (-128, -128, 128, 128)), f'tick {tick}: relative sweep')
            unit_rows += 1
        anchors = [any(s['kind'] == 0 and s['player'] == p and s['hp'] > 0 for s in row['structures']) for p in (0, 1)]
        winner = -1 if all(anchors) else 0 if anchors[0] else 1 if anchors[1] else 2
        require(row['winner'] == winner, 'anchor outcome')
        if prior['winner'] != -1:
            require(all(row[k] == prior[k] for k in ('units', 'structures', 'deposits', 'salvage', 'winner')), 'terminal gameplay changed')
        prior = row
    require(report['mode'] in ('match', 'victory'), 'unknown match route')
    expected_winner = 0 if report['mode'] == 'victory' else 1
    require(final['winner'] == expected_winner and
            any(s['kind'] == 0 and s['player'] == 1-expected_winner and s['hp'] == 0 for s in final['structures']),
            'expected anchor outcome missing')
    require(damage_ticks > 0 and dead, 'no executed building combat')
    require(all(any(u['player'] == p and u['kind'] == 0 for u in final['units']) for p in (0, 1)), 'missing paid production')
    return dict(ticks=final['tick'], unit_rows=unit_rows, damage_ticks=damage_ticks, damaged_structures=sorted(damaged),
                destroyed_structures=sorted(dead), conserved_salvage=total, forfeited_salvage=forfeited,
                final_hash=final['hash'], winner=final['winner'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    require(not (ROOT/'.voidfront-agent/STOP').exists(), 'STOP')
    report = json.loads(args.report.read_text(encoding='utf-8'))
    raw = Path(report['replay_path']).read_bytes()
    commands = commands_from_replay(raw, report)
    summary = audit(report, commands)
    if report['mode'] == 'victory': summary['role_selection_events'] = audit_roles(report, commands)
    rejected = []
    mutations = ['ledger-gap', 'trace-gap', 'resource-created', 'population-lie', 'unfunded-damage', 'wrong-outcome', 'restart-ai-off', 'shot-target-lie', 'wrong-route']
    if report['mode'] == 'victory': mutations += ['wrong-role-selection', 'retained-build-mode']
    for name in mutations:
        bad = copy.deepcopy(report)
        if name == 'ledger-gap': bad['snapshots'].pop(1)
        elif name == 'trace-gap': bad['trace'].pop(1)
        elif name == 'resource-created': bad['snapshots'][1]['salvage'][0] += 1
        elif name == 'population-lie': bad['snapshots'][1]['population_used'][0] += 1
        elif name == 'unfunded-damage': bad['snapshots'][1]['structures'][0]['hp'] -= 8
        elif name == 'wrong-outcome':
            bad['final_snapshot']['winner'] = 1 - report['final_snapshot']['winner']
            bad['snapshots'][-1]['winner'] = bad['final_snapshot']['winner']
        elif name == 'wrong-role-selection': bad['role_selection_events'][0]['selected'] = [1]
        elif name == 'retained-build-mode': bad['role_selection_events'][0]['pending_after']['build'] = True
        elif name == 'wrong-route': bad['mode'] = 'match' if report['mode'] == 'victory' else 'victory'
        elif name == 'shot-target-lie': bad['structure_attack_requests'][0]['target_structure'] += 100
        else: bad['restart_snapshot']['enemy_ai'] = False
        try:
            audit(bad, commands)
            if report['mode'] == 'victory': audit_roles(bad, commands)
        except AssertionError: rejected.append(name)
        else: raise AssertionError(f'corrupted evidence accepted: {name}')
    args.out.mkdir(parents=True, exist_ok=False)
    replay = args.out/'client.vfr'
    replay.write_bytes(raw)
    golden = ''.join(f'{r["tick"]} {int(r["hash"], 16)}\n' for r in report['trace'][1:])
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    binaries = {c: ROOT/f'build/windows/sim/{c}/voidfront_headless.exe' for c in ('Debug', 'Release')}
    fingerprints = {c: digest(p) for c, p in binaries.items()}
    summary.update(started_utc=datetime.now(timezone.utc).isoformat(), report_sha256=digest(args.report), replay_sha256=digest(replay),
                   binaries=fingerprints, commands=commands, rejected_corruptions=rejected, runs=[])
    for config, repeats in (('Debug', 1), ('Release', 10)):
        for run in range(repeats):
            name = f'{config}-{run}'
            trace, metrics = args.out/f'{name}.trace', args.out/f'{name}.json'
            result = subprocess.run([str(binaries[config]), '--replay', str(replay), '--trace', str(trace), '--metrics', str(metrics)],
                capture_output=True, text=True, timeout=120, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            (args.out/f'{name}.log').write_text(result.stdout+result.stderr)
            require(result.returncode == 0, f'{name}: replay failed {result.stderr}')
            require(trace.read_text() == golden, f'{name}: client/replay divergence')
            data = json.loads(metrics.read_text())
            for key in ('salvage', 'structures', 'deposits', 'population_used', 'population_reserved', 'population_cap', 'winner'):
                require(data[key] == report['final_snapshot'][key], f'{name}: {key} differs')
            summary['runs'].append(name)
    require(fingerprints == {c: digest(p) for c, p in binaries.items()}, 'binaries changed')
    summary['completed_utc'] = datetime.now(timezone.utc).isoformat()
    summary['timing_mode'] = report['timing_mode']
    summary['rendered_structure_attack_requests'] = len(report['structure_attack_requests'])
    summary['limitations'] = 'Software InputEvents against economic AI; observed anchor outcome and restart only. No human play, full RTS scope, networking or performance acceptance. Attack clip requests do not prove animation quality.'
    (args.out/'summary.json').write_text(json.dumps(summary, indent=2))
    print(json.dumps({k: v for k, v in summary.items() if k != 'commands'}))


if __name__ == '__main__':
    main()
