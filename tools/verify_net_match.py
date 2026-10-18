"""Audit two packaged economy clients, phase semantics and canonical replays.

This is scripted loopback evidence. It does not accept human play, LAN/Internet,
latency, performance, balance, production art or shipping gates.
"""
import argparse
import copy
from datetime import datetime, timezone
import hashlib
from itertools import zip_longest
import json
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
def phase_labels(player):
    labels = ['initial', 'flux_hidden', 'harvesting', 'miners_stopped', 'build_before', 'build_after',
              'completed', 'train_before', 'train_after', 'train_second_before', 'train_second_after',
              'cancel_before', 'cancel_after', 'strider_spawned', 'flux_revealed', 'flux_gathered',
              'miners_stopped_flux', 'research_before', 'research_after', 'researched',
              'lancer_before', 'lancer_after']
    if player == 0:
        labels += [f'reinforcement_{n}_{when}' for n in range(4) for when in ('before', 'after')]
    return labels + ['lancer_spawned', 'army_ready', 'group_queue_before', 'group_queue_after',
                     'group_queue_drained', 'attack_submitted' if player == 0 else 'defender_ready',
                     'combat_started', 'anchor_damaged', 'outcome']


def input_labels(player):
    pairs = [('gather', 4), ('miners_stopped', 0), ('build', 6), ('train', 7), ('train_second', 7),
             ('cancel', 8), ('strider_rally', 1), ('scout_flux', 1), ('gather_flux', 4),
             ('miners_stopped_flux', 0), ('research', 9), ('lancer', 10)]
    if player == 0:
        pairs += [(f'reinforcement_{n}', 7) for n in range(4)]
    return pairs + [('group_move', 1), ('group_queue_move', 11), ('group_queue_attack', 12),
                    ('group_queue_move_tail', 11), ('group_queue_attack_tail', 12),
                    ('attack_anchor', 2) if player == 0 else ('defender_hold', 3)]
SCALARS = ['winner', 'salvage', 'flux', 'research_ticks', 'researched',
           'population_used', 'population_reserved', 'population_cap',
           'command_results', 'result_sequences', 'structures', 'deposits', 'vision']
UNIT_FIELDS = ['id', 'player', 'hp', 'order', 'kind', 'cargo', 'moving', 'target',
               'target_structure', 'cooldown', 'cargo_kind', 'resource_id',
               'work_ticks', 'returning', 'build_id', 'order_queue', 'max_hp']
RULES = dict(research_cost=50, research_total_ticks=200, foundry_cost=100,
             build_duration=100, strider_cost=50, lancer_cost=75,
             lancer_flux_cost=25, train_ticks=100, production_queue_limit=5,
             population_cap=12, order_queue_limit=4)


def require(ok, message):
    if not ok:
        raise AssertionError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def trace_equal(actual, expected, label):
    for line, (a, b) in enumerate(zip_longest(actual, expected), 1):
        require(a == b, f'{label}: first divergence at trace row {line}: {a!r} != {b!r}')


def hash_value(value):
    require(isinstance(value, str) and len(value) == 16 and all(c in '0123456789abcdef' for c in value), 'invalid state hash')
    return int(value, 16)


def vision_cells(snapshot):
    # Godot JSON serializes PackedByteArray as a bracketed string; native
    # diagnostics emit ordinary arrays. Normalize their representation only.
    cells = [json.loads(v) if isinstance(v, str) else v for v in snapshot['vision']]
    require(len(cells) == 2 and all(len(v) == 64 * 48 and all(c in (0, 1, 2) for c in v) for v in cells), 'invalid vision cell arrays')
    return cells


def parse_replay(raw, report):
    require(len(raw) >= 36 and raw[:4] == b'VFR\3', 'invalid VFR3 header')
    protocol, seed, count, ticks, n, content, map_id = struct.unpack_from('<5IQI', raw, 4)
    initial, network = report['fixture']['initial_snapshot'], report['network']
    require((protocol, seed, count, ticks, content, map_id) ==
            (initial['protocol'], network['seed'], 3, network['recorded_ticks'],
             hash_value(initial['content_id']), 2), 'replay setup mismatch')
    require(protocol == 12 and 0 < ticks <= 10000000 and n <= 100000, 'unsupported replay bounds/protocol')
    pos, commands, sequences = 36, [], [0, 0]
    for _ in range(n):
        require(pos + 4 <= len(raw), 'truncated frame length')
        size, = struct.unpack_from('<I', raw, pos)
        pos += 4
        frame = raw[pos:pos + size]
        pos += size
        require(len(frame) == size and size >= 30 and frame[:4] == b'VFC\1', 'invalid command frame')
        tick, sequence, player, order, x, z, ids_count = struct.unpack_from('<IIBBIII', frame, 4)
        require(1 <= ids_count <= 256 and size == 26 + 4 * ids_count, 'invalid command size/actor count')
        ids = list(struct.unpack_from('<' + 'I' * ids_count, frame, 26))
        require(ids == sorted(set(ids)) and ids[0] > 0, 'noncanonical actor ids')
        require(player in (0, 1) and order in range(13) and tick < ticks, 'invalid command identity')
        require(x < initial['width'] * 256 and z < initial['height'] * 256, 'command coordinates out of bounds')
        require(sequence == sequences[player] + 1, 'per-player command sequence gap')
        sequences[player] = sequence
        if 7 <= order <= 10:
            require(ids_count == 1 and x == z == 0, 'structure operand encoding')
        command = dict(tick=tick, sequence=sequence, player=player, order=order, x=x, z=z, units=ids)
        if commands:
            previous = commands[-1]
            require((tick, player, sequence) > (previous['tick'], previous['player'], previous['sequence']), 'unordered canonical commands')
        commands.append(command)
    require(pos == len(raw), 'trailing replay bytes')
    require(n == network['replay_command_count'], 'recording/report command count mismatch')
    return commands


def prefix(raw, commands, ticks):
    require(ticks > 0, 'prefix needs positive ticks')
    result = bytearray(raw[:36])
    selected = [c for c in commands if c['tick'] < ticks]
    struct.pack_into('<II', result, 16, ticks, len(selected))
    for command in selected:
        frame = b'VFC\1' + struct.pack('<IIBBIII', command['tick'], command['sequence'],
            command['player'], command['order'], command['x'], command['z'], len(command['units']))
        frame += struct.pack('<' + 'I' * len(command['units']), *command['units'])
        result += struct.pack('<I', len(frame)) + frame
    return bytes(result)


def snapshot_binding(snapshot, report):
    network, initial = report['network'], report['fixture']['initial_snapshot']
    tick = snapshot['tick']
    require(0 <= tick <= network['recorded_ticks'], 'snapshot tick outside recorded prefix')
    expected = initial['hash'] if tick == 0 else network['trace'][tick - 1]['hash']
    require(snapshot['hash'] == expected, f'tick {tick}: snapshot/hash trace binding')
    require(snapshot['map_id'] == 2 and snapshot['protocol'] == initial['protocol'] and
            snapshot['content_id'] == initial['content_id'] and snapshot['width'] == 64 and snapshot['height'] == 48, 'phase setup drift')
    require(snapshot['enemy_ai'] is False and snapshot['replay_complete'], 'network phase AI/evidence changed')
    vision_cells(snapshot)
    for key, value in RULES.items():
        require(snapshot[key] == value, f'rule drift: {key}')
    for roster in ('units', 'structures', 'deposits'):
        require([u['id'] for u in snapshot[roster]] == list(range(1, len(snapshot[roster]) + 1)), f'unstable {roster} ids')
    require(len(snapshot['deposits']) == 4, 'deposit roster drift')
    for p in (0, 1):
        living = sum(u['hp'] > 0 and u['player'] == p for u in snapshot['units'])
        reserved = sum(s['production_queue'] for s in snapshot['structures'] if s['hp'] > 0 and s['player'] == p)
        require((snapshot['population_used'][p], snapshot['population_reserved'][p]) == (living, reserved), 'population accounting mismatch')
        require(living + reserved <= 12 and min(snapshot['salvage'][p], snapshot['flux'][p]) >= 0, 'population/resource bounds')
    for unit in snapshot['units']:
        require(unit['kind'] in (0, 1, 2) and 0 <= unit['cargo'] <= 10 and unit['hp'] >= 0, 'unit bounds')
        require(len(unit['order_queue']) <= 4 and all(q['order'] in (1, 2) for q in unit['order_queue']), 'order queue bounds/types')
    for site in snapshot['structures']:
        require(0 <= site['production_queue'] <= 5 and site['queue_lancers'] < (1 << site['production_queue']), 'production queue/mask bounds')
        require(0 <= site['production_ticks'] <= 100 and 0 <= site['build_ticks'] <= 100, 'production/construction bounds')


def audit_reports(reports, raws):
    commands = [parse_replay(raw, report) for raw, report in zip(raws, reports, strict=True)]
    require(raws[0] == raws[1], 'two applied VFR3 recordings differ')
    caps = [report['network']['requested_ticks'] for report in reports]
    require(caps[0] == caps[1] and all(1 <= cap <= 100000 for cap in caps), 'invalid/mismatched requested session caps')
    for p, report in enumerate(reports):
        network, fixture = report['network'], report['fixture']
        require(report['ok'] and report['player'] == fixture['player'] == network['player'] == p, 'fixture/player failure')
        require(report['capture_error'] == 0 and bool(report['renderer']), 'terminal capture/renderer evidence failure')
        require(not report['option_error'] and not fixture['errors'] and fixture['fixture_done'], 'fixture incomplete/errors')
        require(network['state'] == 'complete' and not network['error'] and network['evidence_complete'] and network['ready'], 'session not complete/confirmed')
        require(network['sent'] > 0 and network['received'] > 0, 'missing real transport traffic')
        n = network['recorded_ticks']
        require(n == report['final_tick'] == network['tick'] == network['confirmed_ticks'] <= network['requested_ticks'], 'terminal tick counts differ')
        require([r['tick'] for r in network['trace']] == list(range(1, n + 1)), 'all-tick trace gap/length')
        for row in network['trace']:
            hash_value(row['hash'])
        require(network['hash'] == network['trace'][-1]['hash'], 'network endpoint hash mismatch')
        initial = fixture['initial_snapshot']
        require(initial['tick'] == 0 and initial['winner'] == -1 and len(initial['units']) == 6, 'wrong initial match')
        require(all(u['kind'] == 1 and u['hp'] == 100 for u in initial['units']), 'initial worker roster changed')
        phases = fixture['phases']
        require([s['label'] for s in phases] == phase_labels(p), 'required full phase snapshots missing/reordered')
        require(phases[0]['snapshot'] == initial and phases[-1]['tick'] <= fixture['final_snapshot']['tick'], 'phase endpoint mismatch')
        require(phases[-1]['tick'] == fixture['final_snapshot']['tick'] == n,
                'session terminal tick must equal authoritative outcome tick')
        require([s['tick'] for s in phases] == sorted(s['tick'] for s in phases), 'phase tick order')
        for phase in phases:
            require(phase['tick'] == phase['snapshot']['tick'], 'phase tick/snapshot mismatch')
            snapshot_binding(phase['snapshot'], report)
        captures = fixture['captures']
        require([capture['label'] for capture in captures] == ['harvesting', 'completed', 'production-queued',
                'refunded', 'flux-revealed', 'flux-mined', 'researching', 'lancer-queued', 'lancer',
                'group-queued', 'group-arrived', 'combat', 'anchor-damaged', 'outcome'], 'missing runtime capture phases')
        for capture in captures:
            require(capture['error'] == 0 and capture['tick'] == capture['snapshot']['tick'], 'capture failure/tick mismatch')
            snapshot_binding(capture['snapshot'], report)
        snapshot_binding(initial, report)
        snapshot_binding(fixture['final_snapshot'], report)
        local = [c for c in commands[p] if c['player'] == p]
        inputs = fixture['inputs']
        events = network['inputs']
        require(len(inputs) == len(events) == len(local) > 0, 'input/canonical command count mismatch')
        require(all(i['accepted'] and i['label'] for i in inputs), 'fixture contains rejected/unlabeled inputs')
        require([(i['label'], i['order']) for i in inputs] == input_labels(p), 'labeled scenario command schedule differs')
        for observed, event, command in zip(inputs, events, local, strict=True):
            require(event['disposition'] == 'executed' and event['player'] == p and event['sequence'] == command['sequence'] and
                    event['execution_tick'] == command['tick'], 'input execution identity mismatch')
            require(event['source_tick'] + network['input_delay'] == command['tick'] and
                    observed['event_tick'] <= event['source_tick'], 'input delay/source binding')
            require(all(observed[k] == event[k] == command[k] for k in ('order', 'x', 'z', 'units')), 'labeled InputEvent/canonical mismatch')
            require(0 <= event['input_usec'] <= event['accepted_usec'] <= event['sampled_usec'] <= event['executed_usec'], 'input timestamp ordering')
            before = observed['before_snapshot']
            require(before['tick'] == observed['event_tick'], 'input before-state tick mismatch')
            snapshot_binding(before, report)
        require({0, 4, 6, 7, 8, 9, 10, 11, 12}.issubset(c['order'] for c in local), 'missing economy/queue command coverage')
        require(fixture['final_snapshot']['winner'] == report['winner'] == 0, 'paired match did not reach player0 victory')
    trace_equal(reports[0]['network']['trace'], reports[1]['network']['trace'], 'paired clients')
    require(reports[0]['fixture']['initial_snapshot'] == reports[1]['fixture']['initial_snapshot'], 'initial shared state differs')
    return commands[0]


def compare_snapshot(snapshot, metrics, label):
    require(snapshot['tick'] == metrics['ticks'] and snapshot['protocol'] == metrics['protocol'] and
            snapshot['map_id'] == metrics['map'] and hash_value(snapshot['content_id']) == int(metrics['content_id']), f'{label}: metrics setup mismatch')
    for key in SCALARS:
        actual, expected = (vision_cells(snapshot), vision_cells(metrics)) if key == 'vision' else (snapshot[key], metrics[key])
        require(actual == expected, f'{label}: authoritative {key} differs from replay')
    require(len(snapshot['units']) == len(metrics['units']), f'{label}: unit roster length differs')
    for unit, replay in zip(snapshot['units'], metrics['units'], strict=True):
        for key in UNIT_FIELDS:
            require(unit[key] == replay[key], f'{label}: unit {unit["id"]} {key} differs from replay')
        require([unit['x'], unit['z']] == replay['end'] and
                [unit['goal_x'], unit['goal_z']] == replay['goal'], f'{label}: unit position/goal differs')


def metric_state(metrics):
    row = copy.deepcopy(metrics)
    row['tick'] = row['ticks']
    for unit in row['units']:
        unit['x'], unit['z'] = unit['end']
        unit['goal_x'], unit['goal_z'] = unit['goal']
    return row


def own(snapshot, player, category, kind=None):
    return [u for u in snapshot[category] if u['player'] == player and u['hp'] > 0 and (kind is None or u['kind'] == kind)]


def audit_semantics(reports, commands, states):
    purchased = [dict(build=0, strider=0, lancer=0, cancel=0, research=0) for _ in (0, 1)]
    spending = []
    initial = reports[0]['fixture']['initial_snapshot']
    for c in commands:
        p, tick = c['player'], c['tick']
        before, after = states[tick], states[tick + 1]
        actors = before['structures'] if 7 <= c['order'] <= 10 else before['units']
        require(all(any(u['id'] == uid and u['player'] == p and u['hp'] > 0 for u in actors) for uid in c['units']), 'canonical actor ownership/aliveness')
        if c['order'] not in (6, 7, 8, 9, 10):
            continue
        require(len([other for other in commands if other['tick'] == tick and other['player'] == p]) == 1, 'semantic audit needs one local economic command per tick')
        require(after['result_sequences'][p] == c['sequence'] and after['command_results'][p] == 1, 'economic command result not Accepted/bound')
        require(all(u['order'] not in (4, 5) for u in own(before, p, 'units', 1)), 'exact debit/refund audit needs stopped own harvesters')
        ds = after['salvage'][p] - before['salvage'][p]
        df = after['flux'][p] - before['flux'][p]
        spent_s = spent_f = 0
        if c['order'] == 6:
            added = [s for s in after['structures'] if s['id'] > len(before['structures']) and s['player'] == p]
            require(len(added) == 1 and added[0]['kind'] == 1 and added[0]['build_ticks'] <= 1 and
                    [added[0]['x'], added[0]['z']] == [c['x'] // 256 * 256 + 128, c['z'] // 256 * 256 + 128], 'new Foundry placement/debit binding')
            purchased[p]['build'] += 1
            spent_s = 100
        else:
            site = next(s for s in before['structures'] if s['id'] == c['units'][0])
            new = next(s for s in after['structures'] if s['id'] == site['id'])
            require(site['kind'] == 1 and site['build_ticks'] == 100, 'unready/nonfoundry economic actor')
            q, mask = site['production_queue'], site['queue_lancers']
            births = [u for u in after['units'] if u['id'] > len(before['units']) and u['player'] == p]
            require(len(births) <= 1, 'more than one spawn at one local Foundry')
            if c['order'] in (7, 10):
                lancer = c['order'] == 10
                require(q < 5 and (not lancer or before['researched'][p]), 'purchase missing capacity/research gate')
                expected_mask = mask | ((1 << q) if lancer else 0)
                require(new['production_queue'] == q + 1 - len(births) and
                        new['queue_lancers'] == expected_mask >> len(births), 'purchase queue kind/length mismatch')
                spent_s, spent_f = (75, 25) if lancer else (50, 0)
                purchased[p]['lancer' if lancer else 'strider'] += 1
            elif c['order'] == 8:
                require(q > 0, 'cancel empty queue')
                lancer = bool(mask & (1 << (q - 1)))
                require(new['production_queue'] == q - 1 - len(births) and
                        new['queue_lancers'] == (mask & ((1 << (q - 1)) - 1)) >> len(births), 'cancel must remove/refund queue tail')
                spent_s, spent_f = (-75, -25) if lancer else (-50, 0)
                if new['production_queue'] == 0:
                    require(new['production_ticks'] == 0 and not new['spawn_blocked'], 'empty cancellation did not reset production')
                purchased[p]['cancel'] += 1
            else:
                require(not before['researched'][p] and before['research_ticks'][p] == 0 and
                        after['research_ticks'][p] == 2 and not after['researched'][p], 'research start/progress mismatch')
                spent_f = 50
                purchased[p]['research'] += 1
                require(states[tick + 198]['research_ticks'][p] == 199 and not states[tick + 198]['researched'][p] and
                        states[tick + 199]['research_ticks'][p] == 0 and states[tick + 199]['researched'][p], 'research exact 200-count progression/end differs')
            expected_progress = (min(site['production_ticks'] + 1, 100)
                                 if new['production_queue'] and not births else 0)
            require(new['production_ticks'] == expected_progress, 'buy/cancel/research changed elapsed front work')
            if q == 0 and c['order'] in (7, 10):
                early, complete = states[tick + 99], states[tick + 100]
                early_site = next(s for s in early['structures'] if s['id'] == site['id'])
                completed_site = next(s for s in complete['structures'] if s['id'] == site['id'])
                require(early_site['production_ticks'] == 99 and not any(u['id'] > len(before['units']) and
                        u['player'] == p for u in early['units']), 'unit spawned before 100 training ticks')
                newborn = [u for u in complete['units'] if u['id'] > len(before['units']) and u['player'] == p]
                require(len(newborn) == 1 and newborn[0]['kind'] == (2 if c['order'] == 10 else 0) and
                        newborn[0]['hp'] == (70 if c['order'] == 10 else 100) and completed_site['production_ticks'] == 0,
                        'unobstructed first item did not spawn with configured type/HP at tick100')
        require((ds, df) == (-spent_s, -spent_f), f'p{p} sequence {c["sequence"]}: exact debit/refund differs')
        spending.append((tick, spent_s, spent_f))
    total = [sum(initial['salvage']), sum(initial['flux'])]
    for resource in (0, 1):
        total[resource] += sum(d['remaining'] for d in initial['deposits'] if d['kind'] == resource)
        total[resource] += sum(u['cargo'] for u in initial['units'] if u['cargo_kind'] == resource)
    phases_reviewed = 0
    for p, report in enumerate(reports):
        require(purchased[p]['build'] == 1 and purchased[p]['strider'] >= 2 and purchased[p]['lancer'] >= 1 and
                purchased[p]['cancel'] >= 1 and purchased[p]['research'] == 1, f'p{p}: missing coherent economy coverage')
        phases = {phase['label']: phase for phase in report['fixture']['phases']}
        for phase in phases.values():
            row = phase['snapshot']
            for resource, key in ((0, 'salvage'), (1, 'flux')):
                observed = sum(row[key]) + sum(d['remaining'] for d in row['deposits'] if d['kind'] == resource)
                observed += sum(u['cargo'] for u in row['units'] if u['cargo_kind'] == resource)
                observed += sum(s[resource + 1] for s in spending if s[0] < row['tick'])
                require(observed == total[resource], f'p{p} {phase["label"]}: {key} conservation')
            phases_reviewed += 1
        row = lambda label: phases[label]['snapshot']
        require(row('harvesting')['salvage'][p] > initial['salvage'][p], 'no gathered salvage income')
        require(all(u['order'] == 0 for u in own(row('miners_stopped'), p, 'units', 1)), 'miners not stopped')
        require(len(own(row('completed'), p, 'structures', 1)) == 1 and own(row('completed'), p, 'structures', 1)[0]['build_ticks'] == 100, 'construction never completed')
        require(row('flux_gathered')['flux'][p] >= 75 and row('flux_gathered')['flux'][p] > initial['flux'][p], 'no gathered flux income')
        require(row('researched')['researched'][p] and row('researched')['research_ticks'][p] == 0 and
                row('research_after')['research_ticks'][p] > 0, 'research never progressed/completed')
        require(any(u['kind'] == 2 and u['hp'] == 70 for u in own(row('lancer_spawned'), p, 'units')), 'paid Lancer never spawned at configured HP')
        require(any(u['kind'] == 0 and u['hp'] == 150 for u in own(row('researched'), p, 'units')), 'Strider plating HP bonus absent')
        flux_site = min((d for d in initial['deposits'] if d['kind'] == 1),
                        key=lambda d: (d['x'] - own(initial, p, 'structures', 0)[0]['x']) ** 2 +
                                      (d['z'] - own(initial, p, 'structures', 0)[0]['z']) ** 2)
        cell = flux_site['z'] // 256 * initial['width'] + flux_site['x'] // 256
        require(vision_cells(row('flux_hidden'))[p][cell] != 2 and vision_cells(row('flux_revealed'))[p][cell] == 2, 'contested flux visibility transition absent')
        group_phase = phases['group_queue_after']
        selected = group_phase['selection']
        require(len(selected) >= 2 and selected == sorted(set(selected)), 'multi-unit queue selection absent')
        army = [u for u in own(group_phase['snapshot'], p, 'units') if u['id'] in selected]
        require(len(army) == len(selected) and all(u['kind'] in (0, 2) and u['order'] == 1 and
                [q['order'] for q in u['order_queue']] == [1, 2, 1, 2] for u in army), 'group Move/four queued Move/AttackMove legs missing')
        for leg in range(4):
            require(len({(u['order_queue'][leg]['x'], u['order_queue'][leg]['z']) for u in army}) == len(army), 'queued formation slots collide')
        paths = group_phase['paths']
        require(sorted(path['id'] for path in paths) == selected, 'selected group overlay actor binding')
        for path in paths:
            u = next(u for u in army if u['id'] == path['id'])
            expected = [dict(order=u['order'], x=u['goal_x'], z=u['goal_z'])] + u['order_queue']
            require(path['points'] == expected, 'group overlay authoritative leg binding')
        drained = [u for u in own(row('group_queue_drained'), p, 'units') if u['id'] in selected]
        require(len(drained) == len(army), 'group lost units before queue drain')
        for unit in drained:
            old = next(u for u in army if u['id'] == unit['id'])
            goal = old['order_queue'][-1]
            require(not unit['order_queue'] and not unit['moving'] and unit['order'] == 2 and
                    (unit['x'], unit['z']) == (unit['goal_x'], unit['goal_z']) == (goal['x'], goal['z']), 'group queue did not finish its exact queued attack leg')
        require(not any(c['player'] == p and group_phase['tick'] <= c['tick'] < row('group_queue_drained')['tick'] for c in commands), 'queue drain hidden by new command')
        require(any(u['target'] or u['target_structure'] for u in row('combat_started')['units'] if u['hp'] > 0), 'combat acquisition absent')
        enemy_anchor = next(s for s in row('anchor_damaged')['structures'] if s['kind'] == 0 and s['player'] == 1)
        require(0 < enemy_anchor['hp'] < 1000, 'enemy anchor never damaged before defeat')
        outcome = row('outcome')
        anchors = {s['player']: s['hp'] for s in outcome['structures'] if s['kind'] == 0}
        require(outcome['winner'] == 0 and anchors[0] > 0 and anchors[1] == 0, 'anchor destruction/victory mismatch')
        require(states[outcome['tick'] - 1]['winner'] == -1, 'session did not stop at first authoritative outcome')
    return dict(purchases=purchased, full_phases_audited=phases_reviewed, initial_resources=total)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    require(not (ROOT / '.voidfront-agent/STOP').exists(), 'STOP present')
    paths = [args.folder / f'p{p}.json' for p in (0, 1)]
    replays = [args.folder / f'p{p}.vfr' for p in (0, 1)]
    reports = [json.loads(path.read_text(encoding='utf-8')) for path in paths]
    raws = [path.read_bytes() for path in replays]
    commands = audit_reports(reports, raws)
    host_path = args.folder / 'host.json'
    host = json.loads(host_path.read_text(encoding='utf-8'))
    require(len(host['pids']) == len(set(host['pids'])) == 2 and all(pid > 0 for pid in host['pids']), 'two distinct client process identities missing')
    require(len(host['ports']) == len(set(host['ports'])) == 2 and all(0 < port <= 65535 for port in host['ports']), 'two distinct UDP endpoints missing')
    require(host['ticks'] == reports[0]['network']['requested_ticks'] and
            all(host['seed'] == r['network']['seed'] and host['delay'] == r['network']['input_delay'] for r in reports), 'launcher/report setup mismatch')
    package = Path(host['package_path'])
    package_files = {'package_sha256': package, 'pck_sha256': package.with_suffix('.pck'),
                     'bridge_sha256': package.parent / 'voidfront_bridge.dll'}
    require(all(digest(path).lower() == host[key].lower() for key, path in package_files.items()), 'frozen package fingerprints changed since launch')
    final_captures = [args.folder / f'p{p}.png' for p in (0, 1)]
    require(all(path.is_file() and path.read_bytes()[:8] == b'\x89PNG\r\n\x1a\n' for path in final_captures), 'missing/invalid terminal PNGs')
    args.out.mkdir(parents=True, exist_ok=False)
    binaries = {c: ROOT / f'build/windows/sim/{c}/voidfront_headless.exe' for c in ('Debug', 'Release')}
    fingerprints = {c: digest(path) for c, path in binaries.items()}
    summary = dict(started_utc=datetime.now(timezone.utc).isoformat(), report_sha256=[digest(path) for path in paths],
                   verifier_sha256=digest(Path(__file__)),
                   replay_sha256=[digest(path) for path in replays], host_sha256=digest(host_path),
                   client_processes=host['pids'], udp_ports=host['ports'], package={key: host[key] for key in package_files},
                   terminal_capture_sha256=[digest(path) for path in final_captures],
                   binaries=fingerprints, runs=[], prefix_runs=[], commands=commands)
    expected = [(r['tick'], hash_value(r['hash'])) for r in reports[0]['network']['trace']]

    def run(exe, recording, name, trace_expected):
        require(not (ROOT / '.voidfront-agent/STOP').exists(), 'STOP present')
        trace, metrics = args.out / f'{name}.trace', args.out / f'{name}.json'
        result = subprocess.run([str(exe), '--replay', str(recording), '--hash-mode', 'state', '--trace', str(trace), '--metrics', str(metrics)],
            capture_output=True, text=True, timeout=120, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        (args.out / f'{name}.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        require(result.returncode == 0, f'{name}: replay failed: {result.stderr}')
        actual = [tuple(map(int, line.split())) for line in trace.read_text().splitlines()]
        trace_equal(actual, trace_expected, name)
        return json.loads(metrics.read_text())

    for config, repeats in (('Debug', 1), ('Release', 10)):
        for p in (0, 1):
            for repeat in range(repeats):
                name = f'{config}-p{p}-{repeat}'
                metrics = run(binaries[config], replays[p], name, expected)
                require(metrics['winner'] == reports[p]['winner'], f'{name}: endpoint winner differs')
                terminal_state = copy.deepcopy(reports[p]['fixture']['final_snapshot'])
                terminal_state['tick'] = metrics['ticks']
                compare_snapshot(terminal_state, metrics, f'{name}: state must remain frozen after outcome')
                summary['runs'].append(name)
    require(commands[0]['tick'] > 0, 'initial parity audit requires first empty tick')
    ticks = {1} | {c['tick'] for c in commands} | {c['tick'] + 1 for c in commands}
    for report in reports:
        ticks.update(s['tick'] for s in report['fixture']['phases'])
        ticks.update(i['event_tick'] for i in report['fixture']['inputs'])
        ticks.add(report['fixture']['final_snapshot']['tick'])
        ticks.add(report['fixture']['phases'][-1]['tick'] - 1)
        ticks.update(c['tick'] for c in report['fixture']['captures'])
    for c in commands:
        if c['order'] == 9:
            ticks.update((c['tick'] + 198, c['tick'] + 199))
        if c['order'] in (7, 10):
            ticks.update((c['tick'] + 99, c['tick'] + 100))
    states = {0: reports[0]['fixture']['initial_snapshot']}
    for tick in sorted(ticks - {0}):
        recording = args.out / f'prefix-{tick}.vfr'
        recording.write_bytes(prefix(raws[0], commands, tick))
        name = f'prefix-{tick}'
        states[tick] = metric_state(run(binaries['Debug'], recording, name, expected[:tick]))
        summary['prefix_runs'].append(name)
    for p, report in enumerate(reports):
        initial_copy = copy.deepcopy(report['fixture']['initial_snapshot'])
        initial_copy['tick'] = 1
        compare_snapshot(initial_copy, states[1], f'p{p} native constructor state after first empty tick')
        compare_snapshot(report['fixture']['final_snapshot'], states[report['fixture']['final_snapshot']['tick']], f'p{p} fixture final')
        for phase in report['fixture']['phases']:
            if phase['tick']:
                compare_snapshot(phase['snapshot'], states[phase['tick']], f'p{p} {phase["label"]}')
        for index, observed in enumerate(report['fixture']['inputs']):
            if observed['event_tick']:
                compare_snapshot(observed['before_snapshot'], states[observed['event_tick']], f'p{p} input {index} before-state')
        for capture in report['fixture']['captures']:
            compare_snapshot(capture['snapshot'], states[capture['tick']], f'p{p} capture {capture["label"]}')
            capture_path = Path(capture['path'])
            require(capture_path.is_file() and capture_path.read_bytes()[:8] == b'\x89PNG\r\n\x1a\n', 'missing/invalid captured PNG')
    summary['capture_sha256'] = [{c['label']: digest(c['path']) for c in r['fixture']['captures']} for r in reports]
    summary.update(audit_semantics(reports, commands, states))
    rejected = []
    for name in ('trace-gap', 'trace-extra', 'trace-endpoint', 'shared-false-hash', 'unexecuted-input', 'input-sequence',
                 'input-order', 'input-delay', 'missing-phase', 'phase-hash', 'phase-resource',
                 'phase-queue', 'phase-target', 'phase-winner', 'replay-truncated', 'replay-trailing', 'replay-count',
                 'post-outcome-terminal', 'cap-zero', 'cap-overflow', 'cap-below-terminal', 'paired-cap-drift'):
        bad, bad_raws = copy.deepcopy(reports), list(raws)
        if name == 'trace-gap': bad[0]['network']['trace'].pop(1)
        elif name == 'trace-extra': bad[0]['network']['trace'].append(copy.deepcopy(bad[0]['network']['trace'][-1]))
        elif name == 'trace-endpoint': bad[0]['network']['hash'] = '0000000000000000'
        elif name == 'shared-false-hash':
            for report in bad:
                report['network']['trace'][1]['hash'] = '0000000000000000'
        elif name == 'unexecuted-input': bad[0]['network']['inputs'][0]['disposition'] = 'queued'
        elif name == 'input-sequence': bad[0]['network']['inputs'][0]['sequence'] += 1
        elif name == 'input-order': bad[0]['fixture']['inputs'][0]['order'] = 12
        elif name == 'input-delay': bad[0]['network']['inputs'][0]['source_tick'] += 1
        elif name == 'missing-phase': bad[0]['fixture']['phases'].pop(1)
        elif name == 'phase-hash': bad[0]['fixture']['phases'][1]['snapshot']['hash'] = '0000000000000000'
        elif name == 'phase-resource': bad[0]['fixture']['phases'][1]['snapshot']['salvage'][0] += 1
        elif name == 'phase-queue': next(p for p in bad[0]['fixture']['phases'] if p['label'] == 'group_queue_after')['snapshot']['units'][-1]['order_queue'].append(dict(order=1, x=0, z=0))
        elif name == 'phase-target': next(p for p in bad[0]['fixture']['phases'] if p['label'] == 'combat_started')['snapshot']['units'][-1]['target'] += 1
        elif name == 'phase-winner': bad[0]['fixture']['phases'][-1]['snapshot']['winner'] = 1
        elif name == 'replay-truncated': bad_raws[0] = bad_raws[0][:-1]
        elif name == 'replay-trailing': bad_raws[0] += b'x'
        elif name == 'replay-count':
            broken = bytearray(bad_raws[0]); struct.pack_into('<I', broken, 20, len(commands) + 1); bad_raws[0] = bytes(broken)
        elif name == 'post-outcome-terminal':
            # Keep both endpoint reports, recordings and trace lengths mutually
            # consistent; the outcome-to-terminal relation must reject padding.
            for p, report in enumerate(bad):
                network, fixture = report['network'], report['fixture']
                n = network['recorded_ticks'] + 1
                report['final_tick'] = network['tick'] = network['confirmed_ticks'] = network['recorded_ticks'] = n
                network['requested_ticks'] = max(network['requested_ticks'], n)
                network['trace'].append(dict(tick=n, hash=network['hash']))
                fixture['final_snapshot']['tick'] = n
                broken = bytearray(bad_raws[p]); struct.pack_into('<I', broken, 16, n); bad_raws[p] = bytes(broken)
        elif name in ('cap-zero', 'cap-overflow', 'cap-below-terminal'):
            cap = {'cap-zero': 0, 'cap-overflow': 100001,
                   'cap-below-terminal': bad[0]['network']['recorded_ticks'] - 1}[name]
            for report in bad:
                report['network']['requested_ticks'] = cap
        else:
            bad[1]['network']['requested_ticks'] += 1
        try:
            audit_reports(bad, bad_raws)
            trace_equal([(r['tick'], hash_value(r['hash'])) for r in bad[0]['network']['trace']], expected, name)
            for phase in bad[0]['fixture']['phases']:
                if phase['tick']:
                    compare_snapshot(phase['snapshot'], states[phase['tick']], name)
            audit_semantics(bad, commands, states)
        except AssertionError as error:
            # These regressions must fail their intended endpoint/cap check,
            # rather than a later trace or snapshot comparison masking a gap.
            endpoint_errors = {
                'post-outcome-terminal': 'session terminal tick must equal authoritative outcome tick',
                'cap-zero': 'invalid/mismatched requested session caps',
                'cap-overflow': 'invalid/mismatched requested session caps',
                'cap-below-terminal': 'terminal tick counts differ',
                'paired-cap-drift': 'invalid/mismatched requested session caps',
            }
            if name in endpoint_errors:
                require(str(error) == endpoint_errors[name], f'{name}: wrong rejection: {error}')
            rejected.append(name)
        else:
            raise AssertionError(f'accepted deliberately corrupted evidence: {name}')
    for name, actual in (('replay-trace-short', expected[:-1]), ('replay-trace-long', expected + [expected[-1]])):
        try:
            trace_equal(actual, expected, name)
        except AssertionError:
            rejected.append(name)
        else:
            raise AssertionError(f'accepted deliberate replay trace length mismatch: {name}')
    require(fingerprints == {c: digest(path) for c, path in binaries.items()}, 'binaries changed during audit')
    summary.update(ok=True, ticks=len(expected), requested_ticks=reports[0]['network']['requested_ticks'],
                   first_outcome_tick=reports[0]['fixture']['phases'][-1]['tick'],
                   final_hash=reports[0]['network']['hash'], winner=0,
                   rejected_corruptions=rejected, completed_utc=datetime.now(timezone.utc).isoformat(),
                   limitations='Scripted two-process loopback economy match only; no human play, LAN/Internet, latency, performance, balance, production quality or shipping acceptance.')
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps({k: v for k, v in summary.items() if k not in ('commands', 'runs', 'prefix_runs')}))


if __name__ == '__main__':
    main()
