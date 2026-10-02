"""Audit a packaged flux/research capture against canonical replays in Debug and Release."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

from verify_economy_capture import ROOT, require


def audit(report):
    require(report['ok'] and not report['errors'] and report['mode'] == 'flux', 'packaged flux fixture failed')
    snaps = report['snapshots']
    require(snaps[0]['tick'] == 0 and all(b['tick'] == a['tick'] + 1 for a, b in zip(snaps, snaps[1:])), 'ledger gap')
    require(len(report['trace']) == len(snaps), 'trace gap')
    mined_prev, research_seen, done_seen = 0, False, False
    prev_flux_total = 0
    for s in snaps:
        flux_dep = [d for d in s['deposits'] if d.get('kind') == 1]
        require(len(flux_dep) == 2, 'flux deposits missing')
        carried = sum(u['cargo'] for u in s['units'] if u['player'] == 0 and u.get('cargo_kind') == 1 and u['cargo'] > 0)
        mined = sum(1000 - d['remaining'] for d in flux_dep)
        spent = 50 if (s['research_ticks'][0] > 0 or s['researched'][0]) else 0
        # Each Lancer costs 25 flux: queued slots plus spawned units (no cancels in this fixture).
        spent += 25 * (sum(bin(b.get('queue_lancers', 0)).count('1') for b in s['structures'] if b['player'] == 0)
                       + sum(1 for u in s['units'] if u['player'] == 0 and u.get('kind') == 2))
        require(mined == s['flux'][0] + spent + carried, f'flux not conserved at tick {s["tick"]}')
        require(mined >= mined_prev and s['flux'][1] == 0 and not s['researched'][1], 'flux ledger regressed or enemy gained flux')
        mined_prev = mined
        if s['research_ticks'][0] > 0: research_seen = True
        if s['researched'][0]:
            require(research_seen and s['research_ticks'][0] == 0, 'research completed without running')
            done_seen = True
        elif done_seen: raise AssertionError('research un-completed')
    require(done_seen and mined_prev >= 50, 'flux mining/research never completed')
    accepted = [i for i in report['inputs'] if i['accepted']]
    require(any(i['order'] == 9 for i in accepted), 'research order not recorded')
    require(any(i['order'] == 10 for i in accepted), 'lancer order not recorded')
    require(any(u['player'] == 0 and u.get('kind') == 2 and u['hp'] > 0 for u in snaps[-1]['units']), 'no Lancer in final snapshot')
    return dict(ticks=snaps[-1]['tick'], flux_mined=mined_prev, accepted_inputs=len(accepted))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    require(not (ROOT/'.voidfront-agent/STOP').exists(), 'STOP present')
    report = json.loads(args.report.read_text(encoding='utf-8'))
    summary = audit(report)
    corrupted = []
    import copy
    for name in ('flux-created', 'enemy-flux', 'research-free', 'trace-gap'):
        bad = copy.deepcopy(report)
        row = bad['snapshots'][-1]
        if name == 'flux-created': row['flux'][0] += 1
        elif name == 'enemy-flux': row['flux'][1] = 5
        elif name == 'research-free': bad['snapshots'][-1]['researched'] = [False, False]; bad['snapshots'][-2]['researched'] = [True, False]
        else: bad['trace'].pop(3)
        try: audit(bad)
        except AssertionError: corrupted.append(name)
        else: raise AssertionError(f'accepted corrupted evidence {name}')
    args.out.mkdir(parents=True, exist_ok=False)
    raw = Path(report['replay_path']).read_bytes()
    replay = args.out/'client.vfr'
    replay.write_bytes(raw)
    golden = ''.join(f'{r["tick"]} {int(r["hash"], 16)}\n' for r in report['trace'][1:])
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    binaries = {c: ROOT/f'build/windows/sim/{c}/voidfront_headless.exe' for c in ('Debug', 'Release')}
    runs = []
    for config, repeats in (('Debug', 1), ('Release', 10)):
        for run in range(repeats):
            name = f'{config}-{run}'
            trace, metrics = args.out/f'{name}.trace', args.out/f'{name}.json'
            result = subprocess.run([str(binaries[config]), '--replay', str(replay), '--trace', str(trace), '--metrics', str(metrics)],
                capture_output=True, text=True, timeout=120, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            require(result.returncode == 0, f'{name}: {result.stderr}')
            require(trace.read_text() == golden, f'{name}: client/replay divergence')
            data = json.loads(metrics.read_text())
            final = report['final_snapshot']
            require(data['flux'] == final['flux'] and data['researched'] == [int(v) for v in final['researched']] and
                    data['salvage'] == final['salvage'], f'{name}: economy differs')
            runs.append(name)
    summary.update(report_sha256=digest(args.report), replay_sha256=digest(replay), rejected_corruptions=corrupted, runs=runs,
                   limitations='Scripted offline flux/research only. No human play, balance, full match, networking or performance acceptance.')
    (args.out/'summary.json').write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
