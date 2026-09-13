"""Replay actual large-map client inputs in both builds and audit final geometry."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import os
import struct
import subprocess
from datetime import datetime, timezone

from verify_scale import read_commands, audit_ledger, require

ROOT = Path(__file__).resolve().parents[1]


def recording(report):
    require(report['ok'] and not report['errors'], 'client fixture failed')
    setup = report['setup']
    final = report['final_snapshot']
    count = report['units_per_team']
    ticks = final['tick']
    require(setup['map_id'] == 1 and setup['width'] == setup['height'] == 128, 'wrong authoritative map')
    require(1 <= count <= 250 and len(final['units']) == count * 2, 'wrong population')
    trace = report['trace']
    require([row['tick'] for row in trace] == list(range(ticks + 1)), 'incomplete client trace')
    require(trace[-1]['hash'] == final['hash'], 'final client hash mismatch')
    inputs = report['inputs']
    require(len(inputs) == 3 and [i['order'] for i in inputs] == [1, 0, 1], 'missing Move/Stop/resume input')
    stop, resume = inputs[1:]
    require(resume['event_tick'] - stop['event_tick'] >= 9, 'Stop interval shorter than nine ticks')
    require(report['stop_tick'] == stop['event_tick'] + 1, 'wrong first Stop application tick')
    require(all(i['application_tick'] == i['event_tick'] + 1 for i in inputs), 'wrong application tick identity')
    frames = []
    prior_tick = -1
    for sequence, command in enumerate(inputs, 1):
        ids = command['units']
        tick = command['event_tick']
        require(command['accepted'] and prior_tick <= tick < ticks, 'input rejected or out of order')
        require(ids == list(range(1, count + 1)), 'input does not cover exact own population')
        require(0 <= command['x'] < 32768 and 0 <= command['z'] < 32768, 'invalid input coordinate')
        if command['order'] == 1:
            require(command['x'] > 32 * 256 and command['z'] > 24 * 256, 'Move did not exercise expanded bounds')
        frame = b'VFC\1' + struct.pack('<IIBBIII', tick, sequence, 0, command['order'],
                                     command['x'], command['z'], len(ids))
        frame += struct.pack('<' + 'I' * len(ids), *ids)
        frames.append(struct.pack('<I', len(frame)) + frame)
        prior_tick = tick
    raw = b'VFR\3' + struct.pack('<5IQI', setup['protocol'], report['seed'], count, ticks,
                                  len(frames), int(setup['content_id'], 16), 1) + b''.join(frames)
    return raw, '\n'.join(f"{r['tick']} {int(r['hash'], 16)}" for r in trace[1:]) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    require(not (ROOT / '.voidfront-agent/STOP').exists(), 'STOP is present')
    report = json.loads(args.report.read_text())
    raw, golden = recording(report)
    # Verify the input/trace checks actually reject evidence omissions and aliases.
    rejected = []
    for label in ('trace-gap', 'trace-duplicate', 'input-missing', 'foreign-id', 'old-map', 'old-bounds', 'collapsed-stop', 'stop-tick'):
        bad = copy.deepcopy(report)
        if label == 'trace-gap': bad['trace'].pop(1)
        elif label == 'trace-duplicate': bad['trace'][1] = bad['trace'][0]
        elif label == 'input-missing': bad['inputs'].pop()
        elif label == 'foreign-id': bad['inputs'][0]['units'][-1] = report['units_per_team'] + 1
        elif label == 'old-map': bad['setup']['map_id'] = 0
        elif label == 'old-bounds': bad['inputs'][0]['x'] = 31 * 256
        elif label == 'collapsed-stop':
            bad['inputs'][2]['event_tick'] = bad['inputs'][1]['event_tick']
            bad['inputs'][2]['application_tick'] = bad['inputs'][1]['application_tick']
        elif label == 'stop-tick': bad['stop_tick'] += 1
        try: recording(bad)
        except AssertionError: rejected.append(label)
        else: raise AssertionError(f'accepted corrupt evidence: {label}')
    args.out.mkdir(parents=True, exist_ok=False)
    replay = args.out / 'client.vfr'
    replay.write_bytes(raw)
    (args.out / 'client.trace').write_text(golden)
    binaries = {c: ROOT / f'build/windows/sim/{c}/voidfront_headless.exe' for c in ('Debug', 'Release')}
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    fingerprints = {c: digest(p) for c, p in binaries.items()}
    summary = {'started_utc': datetime.now(timezone.utc).isoformat(), 'report_sha256': digest(args.report),
               'binaries': fingerprints, 'ticks': report['final_snapshot']['tick'], 'runs': [],
               'rejected_corruptions': rejected}
    for config, repeats in (('Debug', 1), ('Release', 10)):
        for repeat in range(repeats):
            require(not (ROOT / '.voidfront-agent/STOP').exists(), 'STOP is present')
            name = f'{config}-{repeat}'
            trace = args.out / f'{name}.trace'
            metrics = args.out / f'{name}.json'
            command = [str(binaries[config]), '--replay', str(replay), '--trace', str(trace), '--metrics', str(metrics)]
            ledger = args.out / 'replayed.csv'
            if config == 'Release' and repeat == 0: command += ['--samples', str(ledger)]
            result = subprocess.run(command, capture_output=True, text=True, timeout=180,
                                    creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            (args.out / f'{name}.log').write_text(result.stdout + result.stderr)
            require(result.returncode == 0, f'{name}: replay failed: {result.stderr}')
            require(trace.read_text() == golden, f'{name}: client/replay tick hash divergence')
            data = json.loads(metrics.read_text())
            for observed, actual in zip(report['final_snapshot']['units'], data['units'], strict=True):
                require(observed['id'] == actual['id'] and [observed['x'], observed['z']] == actual['end'] and
                        observed['hp'] == actual['hp'], f'{name}: final unit differs')
            if config == 'Release' and repeat == 0:
                summary['swept_ledger'] = audit_ledger(ledger, data, read_commands(raw, data))
            summary['runs'].append(name)
    require(fingerprints == {c: digest(p) for c, p in binaries.items()}, 'binaries changed')
    summary['completed_utc'] = datetime.now(timezone.utc).isoformat()
    summary['note'] = 'Packaged inputs and complete tick hashes replay; not crowd arrival, human input latency or performance acceptance.'
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
