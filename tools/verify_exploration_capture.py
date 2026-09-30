"""Validate rendered exploration evidence and replay every tick across builds."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def require(value, message):
    if not value:
        raise AssertionError(message)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    require(not (ROOT / '.voidfront-agent/STOP').exists(), 'STOP present')
    report = json.loads(args.report.read_text(encoding='utf-8'))
    require(report['ok'] and not report['errors'], 'gameplay assertions failed')
    require([r['tick'] for r in report['trace']] == list(range(1, report['ticks']+1)), 'tick trace gap')
    phases = {s['phase']:s for s in report['vision_samples']}
    require(phases['home']['scout_cell'] == 0 and phases['scouted']['scout_cell'] == 2 and phases['explored']['scout_cell'] == 1, 'exploration transitions')
    require(not phases['unexplored']['enemy_visible'], 'camera revealed hidden anchor')
    for capture in report['captures']:
        require(capture['error'] == 0 and Path(capture['path']).stat().st_size > 100, 'missing screenshot')
    raw = Path(report['replay_path']).read_bytes()
    require(raw[:4] == b'VFR\3', 'invalid replay header')
    protocol, seed, count, ticks, n, content, map_id = struct.unpack_from('<5IQI', raw, 4)
    require(protocol == 9 and count == 3 and ticks == report['ticks'] and map_id == 2, 'replay setup')
    commands, pos = [], 36
    for _ in range(n):
        length, = struct.unpack_from('<I', raw, pos)
        pos += 4
        frame = raw[pos:pos+length]
        pos += length
        require(frame[:4] == b'VFC\1', 'invalid command header')
        tick, seq, player, order, x, z, ids_count = struct.unpack_from('<IIBBIII', frame, 4)
        require(length == 26+4*ids_count and player in (0,1) and 0 <= tick < ticks, 'invalid command')
        ids = list(struct.unpack_from('<'+'I'*ids_count, frame, 26))
        require(ids == sorted(set(ids)) and 0 <= x < 64*256 and 0 <= z < 48*256, 'command bounds')
        commands.append(dict(event_tick=tick, player=player, order=order, units=ids, x=x, z=z))
    require(pos == len(raw), 'trailing replay bytes')
    human = [c for c in commands if c['player'] == 0]
    accepted = [i for i in report['inputs'] if i['accepted']]
    require(len(human) == len(accepted), 'input coverage')
    for actual, observed in zip(human, accepted):
        require(all(actual[k] == observed[k] for k in ('event_tick','order','units','x','z')), 'input binding')
    require({1,4,6,7}.issubset({c['order'] for c in human}), 'missing player economy/scouting')
    require({2,4,6,7}.issubset({c['order'] for c in commands if c['player'] == 1}), 'missing AI economy/attack')
    args.out.mkdir(parents=True, exist_ok=False)
    golden = ''.join(f"{r['tick']} {int(r['hash'],16)}\n" for r in report['trace'])
    runs = []
    for config, repeats in [('Debug',1),('Release',10)]:
        for repeat in range(repeats):
            target = args.out / f'{config}-{repeat}.trace'
            binary = ROOT / f'build/windows/sim/{config}/voidfront_headless.exe'
            completed = subprocess.run([str(binary),'--replay',report['replay_path'],'--trace',str(target)],capture_output=True,text=True,check=True)
            require(target.read_text() == golden, f'{config} replay mismatch')
            runs.append(dict(config=config,repeat=repeat,output=completed.stdout.strip()))
    summary = dict(ok=True,ticks=ticks,checks=len(report['checks']),human_commands=len(human),ai_commands=len(commands)-len(human),replays=runs,replay_sha256=hashlib.sha256(raw).hexdigest())
    (args.out/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    print(json.dumps(summary,indent=2))

if __name__ == '__main__':
    main()
