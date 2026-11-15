"""Audit two complete, negotiated Glass Reach matches in the same client pair."""

import argparse
import json
from pathlib import Path
import struct
import subprocess
import sys

from verify_net_match import ROOT, digest, require


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def capture_evidence(path):
    data = path.read_bytes()
    require(len(data) >= 33 and data.startswith(b'\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR'),
            f'{path}: missing PNG capture')
    width, height = struct.unpack_from('>II', data, 16)
    require(width > 0 and height > 0, f'{path}: zero-size PNG')
    return {'path': str(path.resolve()), 'width': width, 'height': height, 'sha256': digest(path)}


def audit_round(folder):
    host_path = folder / 'host.json'
    host = read_json(host_path)
    pids, ports = host['pids'], host['ports']
    require(len(pids) == len(set(pids)) == 2 and all(type(p) is int and p > 0 for p in pids),
            f'{folder.name}: expected two distinct process IDs')
    require(len(ports) == len(set(ports)) == 2 and all(type(p) is int and 1024 <= p <= 65535 for p in ports),
            f'{folder.name}: expected two distinct loopback ports')
    session = host['session']
    require(isinstance(session, str) and session.isascii() and session.isdecimal()
            and session == str(int(session)) and 0 < int(session) < 2**63,
            f'{folder.name}: invalid session identity')
    report_paths = [folder / f'p{p}.json' for p in (0, 1)]
    reports = [read_json(path) for path in report_paths]
    expected_setup = {
        'session': session, 'host_port': ports[0], 'guest_port': ports[1], 'map_id': 2,
        'seed': host['seed'], 'delay': host['delay'], 'ticks': host['ticks'],
    }
    archives, times, captures, replays, start_hashes = [], [], [], [], []
    for player, report in enumerate(reports):
        label = f'{folder.name}/p{player}'
        lobby, network = report['lobby'], report['network']
        require(report['pid'] == pids[player] and report['player'] == player,
                f'{label}: process/role differs from launcher')
        require(all(lobby.get(key) == value for key, value in expected_setup.items()),
                f'{label}: negotiated settings differ from launcher or peer')
        require(network['session_id'] == session and network['input_delay'] == lobby['delay']
                and network['seed'] == lobby['seed'] and network['requested_ticks'] == lobby['ticks'],
                f'{label}: native match differs from the agreed setup')
        initial = report['fixture']['initial_snapshot']
        require(initial['tick'] == 0 and initial['map_id'] == lobby['map_id'],
                f'{label}: no fresh tick-zero map snapshot')
        start, finish = report['start_usec'], report['finish_usec']
        require(type(start) is int and type(finish) is int and 0 < start < finish,
                f'{label}: invalid lifecycle timestamps')
        times.append({'start_usec': start, 'finish_usec': finish})
        start_hashes.append(initial['hash'])

        replay = folder / f'p{player}.vfr'
        archive = Path(report['archive_path'])
        require(archive.is_absolute() and archive.is_file() and not archive.samefile(replay),
                f'{label}: missing separate gameplay archive')
        replay_hash = digest(replay)
        require(digest(archive) == replay_hash, f'{label}: archived replay differs from captured replay')
        replays.append(replay_hash)
        archives.append({'path': str(archive.resolve()), 'sha256': replay_hash})

        captures.append(capture_evidence(folder / f'p{player}.png'))
    require(start_hashes[0] == start_hashes[1], f'{folder.name}: tick-zero hashes differ')
    require(replays[0] == replays[1], f'{folder.name}: peer recordings differ')
    require(captures[0]['sha256'] != captures[1]['sha256'], f'{folder.name}: reused one role\'s terminal capture')
    return {
        'pids': pids, 'ports': ports, 'setup': expected_setup, 'times': times, 'archives': archives,
        'start_hash': start_hashes[0], 'replay_sha256': replays[0], 'terminal_captures': captures,
        'offer_capture': capture_evidence(folder / 'p1-offer.png'),
        'host_sha256': digest(host_path), 'report_sha256': [digest(path) for path in report_paths],
        'package_path': str(Path(host['package_path']).resolve()),
        'package': {k: host[k].lower() for k in ('package_sha256', 'pck_sha256', 'bridge_sha256')},
    }


def audit_transition(first, second):
    require(first['pids'] == second['pids'] and first['ports'] == second['ports'],
            'rematch did not reuse both processes and ports')
    require(first['package_path'] == second['package_path'] and first['package'] == second['package'],
            'rematch used a different package')
    for key in ('session', 'seed', 'delay'):
        require(first['setup'][key] != second['setup'][key], f'rematch did not change {key}')
    require(first['start_hash'] != second['start_hash'] and first['replay_sha256'] != second['replay_sha256'],
            'rematch reused the first match state or recording')
    archive_paths = [a['path'] for row in (first, second) for a in row['archives']]
    require(len(set(archive_paths)) == 4, 'rematch reused an archive filename')
    for player in (0, 1):
        require(first['times'][player]['finish_usec'] < second['times'][player]['start_usec'],
                f'p{player}: rematch started before the initial match was captured')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    require(not (ROOT / '.voidfront-agent/STOP').exists(), 'STOP present')

    folders = [args.folder.resolve() / name for name in ('round1', 'round2')]
    rounds = [audit_round(folder) for folder in folders]
    audit_transition(*rounds)
    args.out.mkdir(parents=True, exist_ok=False)
    verifier = Path(__file__).with_name('verify_net_match.py')
    for folder, row in zip(folders, rounds, strict=True):
        out = (args.out / folder.name).resolve()
        subprocess.run([sys.executable, '-B', str(verifier), str(folder), '--out', str(out)],
                       check=True, cwd=ROOT)
        summary_path = out / 'summary.json'
        native = read_json(summary_path)
        require(native['ok'] and native['ticks'] == native['first_outcome_tick'] < native['requested_ticks']
                and all(sha == row['replay_sha256'] for sha in native['replay_sha256'])
                and native['host_sha256'] == row['host_sha256']
                and native['report_sha256'] == row['report_sha256']
                and native['terminal_capture_sha256'] == [c['sha256'] for c in row['terminal_captures']],
                f'{folder.name}: native audit did not confirm the observed first outcome')
        row.update(audit_path=str(summary_path), audit_sha256=digest(summary_path),
                   final_tick=native['ticks'], final_hash=native['final_hash'], winner=native['winner'])

    # Check again after the native audits, including the first recording's archive.
    for folder, row in zip(folders, rounds, strict=True):
        current = audit_round(folder)
        require(all(row[key] == value for key, value in current.items()),
                f'{folder.name}: evidence changed while auditing')
    summary = {'ok': True, 'rounds': rounds,
               'scope': 'Scripted rendered loopback, with independent native first-outcome verification.'}
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
