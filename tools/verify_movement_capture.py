"""Independently check a packaged movement capture's complete position ledger."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
from verify_navigation import Fixture, geometry, point_clear, midpoint_enters_open_rect, reference


def verify(path):
    data = json.loads(path.read_text())
    assert data['ok'] and data['mode'] == 'movement'
    rows = data['positions']
    assert [r['tick'] for r in rows] == list(range(1, data['tick'] + 1))
    inputs = data['accepted_inputs']
    assert [c['label'] for c in inputs] == ['direct', 'detour', 'stop_leg', 'stop', 'resume', 'retarget']
    assert [c['order'] for c in inputs] == [1, 1, 1, 0, 1, 1]
    assert all(c['accepted'] and c['units'] == [1] for c in inputs)
    assert all(a['event_tick'] < b['event_tick'] for a, b in zip(inputs, inputs[1:]))
    setup = Fixture('current_map', (256,256,31*256,23*256),
                    ((15*256,3*256,17*256,9*256),(15*256,16*256,17*256,21*256)),64,(640,2432),(640,2432))
    bounds, obstacles = geometry(setup)
    previous = setup.start
    oblique, speeds = 0, []
    positions = {0: previous}
    for row in rows:
        at = row['x'], row['z']
        delta = at[0]-previous[0], at[1]-previous[1]
        distance2 = delta[0]**2 + delta[1]**2
        assert delta == (row['dx'],row['dz']) and distance2 == row['step_squared']
        assert distance2 <= 1024
        assert point_clear(at,bounds,obstacles)
        assert not any(midpoint_enters_open_rect(previous,at,r) for r in obstacles)
        oblique += bool(delta[0] and delta[1] and abs(delta[0]) != abs(delta[1]))
        if distance2: speeds.append(math.sqrt(distance2))
        positions[row['tick']] = at
        previous = at
    assert oblique == data['arbitrary_heading_steps'] and oblique > 20
    for command in inputs:
        for field in ('x','z','event_tick','order'):
            assert type(command[field]) is int
        assert 0 <= command['x'] < 32*256 and 0 <= command['z'] < 24*256
        assert command['event_tick'] in positions
    stop, resume = inputs[3:5]
    assert (inputs[2]['x'],inputs[2]['z']) == (resume['x'],resume['z'])
    assert resume['event_tick'] - stop['event_tick'] >= 9
    for command in (stop,inputs[5]):
        at_event=rows[command['event_tick']-1]
        assert at_event['moving'] and at_event['step_squared'] > 0
    assert all(not rows[t-1]['moving'] and rows[t-1]['order']==0 for t in range(stop['event_tick']+1,resume['event_tick']+1))
    old_goal=resume['x'],resume['z']
    assert positions[stop['event_tick']] != old_goal and positions[inputs[5]['event_tick']] != old_goal
    detour=inputs[1]
    detour_start=positions[detour['event_tick']]
    detour_goal=detour['x'],detour['z']
    assert any(midpoint_enters_open_rect(detour_start,detour_goal,r) for r in obstacles)
    assert any(3776 <= positions[t][0] <= 4416 and positions[t][1] >= 2368
               for t in range(detour['event_tick']+1,inputs[2]['event_tick']+1))
    stationary = positions[stop['event_tick']]
    assert all(positions[t] == stationary for t in range(stop['event_tick']+1,resume['event_tick']+1))
    routes = []
    for index in (0,1,5):
        command = inputs[index]
        first = command['event_tick']
        end = inputs[index+1]['event_tick'] if index+1<len(inputs) else data['tick']
        goal = command['x'],command['z']
        arrival = next(t for t in range(first+1,end+1) if positions[t] == goal)
        assert all(positions[t] == goal for t in range(arrival,end+1))
        actual = sum(math.dist(positions[t-1],positions[t]) for t in range(first+1,arrival+1))
        fixture = Fixture(command['label'],setup.bounds,setup.obstacles,64,positions[first],goal)
        oracle = reference(fixture)
        shortest = oracle['distance']
        # Route tolerance was declared before capture. Integrated travel is
        # separately measured, including fixed-point stepping variation.
        assert actual <= shortest*1.01+2
        routes.append(dict(label=command['label'],start=positions[first],goal=goal,
                           arrival_tick=arrival,actual_length=actual,reference_length=shortest,
                           excess_percent=(actual/shortest-1)*100))
    return dict(verified=True,checked_utc=datetime.now(timezone.utc).isoformat(),
                report=str(path.resolve()),report_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                position_count=len(rows),oblique_steps=oblique,stop_stationary_ticks=resume['event_tick']-stop['event_tick'],
                routes=routes,moving_step_min=min(speeds),moving_step_max=max(speeds),
                limits='Single scripted walker, fixed current-map square clearance. Final short steps included; no uniform-speed, crowd or manual responsiveness acceptance.')


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report',type=Path)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    result=verify(args.report)
    with args.out.open('x') as stream: json.dump(result,stream,indent=2)
    print(json.dumps(result,indent=2))
