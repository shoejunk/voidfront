"""Independent bounded static-route oracle; this is not a crowd/performance gate."""
from __future__ import annotations
import argparse
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from fractions import Fraction
import hashlib
import heapq
import json
import math
from pathlib import Path
import random
import subprocess

TOLERANCE_RATIO = 0.01  # Predeclared in navigation-contract.md, before comparison.
TOLERANCE_UNITS = 2.0
Point = tuple[int, int]
Rect = tuple[int, int, int, int]

@dataclass(frozen=True)
class Fixture:
    name: str
    bounds: Rect
    obstacles: tuple[Rect, ...]
    clearance: int
    start: Point
    goal: Point

    def stdin(self) -> str:
        values = [*self.bounds, self.clearance, len(self.obstacles)]
        for rect in self.obstacles:
            values.extend(rect)
        values.extend((*self.start, *self.goal))
        return ' '.join(map(str, values))


def valid_world(f: Fixture) -> bool:
    def valid_rect(r: Rect) -> bool:
        return all(-32768 <= v <= 32768 for v in r) and r[0] < r[2] and r[1] < r[3]
    b, c = f.bounds, f.clearance
    return (valid_rect(b) and 0 <= c <= 32768 and len(f.obstacles) <= 64
            and all(valid_rect(r) for r in f.obstacles)
            and b[0] + c <= b[2] - c and b[1] + c <= b[3] - c)


def geometry(f: Fixture) -> tuple[Rect, list[Rect]]:
    b, c = f.bounds, f.clearance
    return ((b[0]+c, b[1]+c, b[2]-c, b[3]-c),
            sorted(set((x0-c, z0-c, x1+c, z1+c) for x0, z0, x1, z1 in f.obstacles)))


def point_clear(p: Point, bounds: Rect, obstacles: list[Rect]) -> bool:
    x, z = p
    return (bounds[0] <= x <= bounds[2] and bounds[1] <= z <= bounds[3]
            and not any(a < x < c and b < z < d for a, b, c, d in obstacles))


def slab_enters_open_rect(a: Point, b: Point, r: Rect) -> bool:
    # Intersection of the segment's parameter interval [0,1] with both strict
    # open rectangle slabs. Fractions are exact, including tiny corner grazes.
    lower, upper = Fraction(0), Fraction(1)
    for p, q, lo, hi in ((a[0], b[0], r[0], r[2]), (a[1], b[1], r[1], r[3])):
        delta = q - p
        if delta == 0:
            if not lo < p < hi:
                return False
            continue
        first, last = sorted((Fraction(lo-p, delta), Fraction(hi-p, delta)))
        lower, upper = max(lower, first), min(upper, last)
        if lower >= upper:
            return False
    return lower < upper


def midpoint_enters_open_rect(a: Point, b: Point, r: Rect) -> bool:
    # A distinct event/subdivision oracle, not slab clipping: split at each
    # rectangle supporting-line intersection and classify interval midpoints.
    # Between consecutive events no rectangle-edge crossing is possible.
    events = {Fraction(0), Fraction(1)}
    for p, q, edges in ((a[0], b[0], (r[0],r[2])), (a[1], b[1], (r[1],r[3]))):
        if p != q:
            for edge in edges:
                t = Fraction(edge-p, q-p)
                if 0 < t < 1:
                    events.add(t)
    times = sorted(events)
    for low, high in zip(times,times[1:]):
        t = (low+high)/2
        x, z = a[0]+(b[0]-a[0])*t, a[1]+(b[1]-a[1])*t
        if r[0] < x < r[2] and r[1] < z < r[3]:
            return True
    return False


def segment_clear(a: Point, b: Point, bounds: Rect, obstacles: list[Rect]) -> bool:
    if not point_clear(a, bounds, obstacles) or not point_clear(b, bounds, obstacles):
        return False
    for r in obstacles:
        # Bounding-box rejection changes cost only, not the exact slab result.
        if max(a[0], b[0]) <= r[0] or min(a[0], b[0]) >= r[2]:
            continue
        if max(a[1], b[1]) <= r[1] or min(a[1], b[1]) >= r[3]:
            continue
        if midpoint_enters_open_rect(a, b, r):
            return False
    return True


def reference(f: Fixture) -> dict:
    if not valid_world(f):
        return dict(valid=False, start_clear=False, goal_clear=False, direct_clear=False,
                    distance=None, candidate_count=0)
    bounds, obstacles = geometry(f)
    sc, gc = point_clear(f.start, bounds, obstacles), point_clear(f.goal, bounds, obstacles)
    result = dict(valid=True, start_clear=sc, goal_clear=gc, direct_clear=False,
                  distance=None, candidate_count=0)
    if not (sc and gc):
        return result
    result['direct_clear'] = segment_clear(f.start, f.goal, bounds, obstacles)
    if result['direct_clear']:
        result['distance'] = math.dist(f.start, f.goal)
        result['candidate_count'] = 1 if f.start == f.goal else 2
        return result
    xs = {bounds[0], bounds[2], f.start[0], f.goal[0]}
    zs = {bounds[1], bounds[3], f.start[1], f.goal[1]}
    for x0, z0, x1, z1 in obstacles:
        xs.update(x for x in (x0, x1) if bounds[0] <= x <= bounds[2])
        zs.update(z for z in (z0, z1) if bounds[1] <= z <= bounds[3])
    # This Cartesian candidate superset is independent of the C++ exposed-corner
    # construction; includes union/bounds intersections and nonvertex points.
    points = sorted((x, z) for x in xs for z in zs if point_clear((x, z), bounds, obstacles))
    result['candidate_count'] = len(points)
    start, goal = points.index(f.start), points.index(f.goal)
    distances = [math.inf] * len(points)
    distances[start] = 0.0
    pending = [(0.0, start)]
    settled: set[int] = set()
    while pending:
        distance, i = heapq.heappop(pending)
        if i in settled:
            continue
        settled.add(i)
        if i == goal:
            result['distance'] = distance
            return result
        for j, point in enumerate(points):
            if j in settled or not segment_clear(points[i], point, bounds, obstacles):
                continue
            cost = distance + math.dist(points[i], point)
            if cost < distances[j]:
                distances[j] = cost
                heapq.heappush(pending, (cost, j))
    return result


def fixtures() -> list[Fixture]:
    b = (0, 0, 4096, 4096)
    def f(name, obstacles=(), start=(512, 2048), goal=(3584, 2048), clearance=64, bounds=b):
        return Fixture(name, bounds, tuple(obstacles), clearance, start, goal)
    result = [
        f('open_oblique', start=(129,137), goal=(3821,2673)),
        f('tiny_off_center', start=(129,137), goal=(130,139)),
        f('same_point', start=(173,201), goal=(173,201)),
        f('single_box', [(1500,1000,2500,3000)]),
        f('angled_stair_wall', [(1200,600,1456,2200),(1456,1500,1900,2400),(1900,1800,2200,2800)]),
        f('concave_u_escape', [(1200,900,1500,3100),(1500,900,2900,1200),(1500,2800,2900,3100)],
          start=(1800,2048), goal=(700,2048)),
        f('inflated_overlap', [(1450,1300,2100,2300),(2050,2100,2600,2900)]),
        f('narrow_exact_pass', [(1900,0,2100,1984),(1900,2112,2100,4096)]),
        f('narrow_one_unit_extra', [(1900,0,2100,1984),(1900,2113,2100,4096)]),
        f('narrow_one_unit_blocked', [(1900,0,2100,1985),(1900,2112,2100,4096)]),
        f('unreachable_wall', [(1900,0,2100,4096)]),
        f('long_detour', [(1900,0,2100,3600)], start=(512,512), goal=(3584,512)),
        f('alternating_detour', [(1000,0,1256,2900),(2700,1200,2956,4096)]),
        f('shrunk_boundary_tangent', start=(64,64), goal=(4032,4032)),
        f('shrunk_boundary_outside', start=(63,64)),
        f('inflated_boundary_tangent', [(1500,1000,2500,3000)], start=(1436,64), goal=(1436,4032)),
        f('inflated_interior_start', [(1500,1000,2500,3000)], start=(1437,1500)),
        f('inflated_interior_goal', [(1500,1000,2500,3000)], goal=(2000,2000)),
        f('obstacle_crosses_bounds', [(-100,1500,1900,2500)], start=(512,512), goal=(512,3584)),
        f('obstacle_outside_bounds', [(-1000,-1000,-500,-500)]),
        f('zero_clearance', [(1500,1000,2500,3000)], clearance=0),
        f('goal_on_static_vertex', [(1500,1000,2500,3000)], start=(3500,2000), goal=(1436,936)),
        f('edge_touching_boxes', [(1300,1000,1900,2500),(1900,1000,2500,2500)], clearance=0),
        f('collapsed_legal_line', start=(64,64), goal=(64,4032), bounds=(0,0,128,4096)),
        f('signed_coordinate_extremes', start=(-32768,-32768), goal=(32768,32768), clearance=0,
          bounds=(-32768,-32768,32768,32768)),
        f('invalid_zero_width', bounds=(0,0,0,4096)),
        f('invalid_rectangle', [(1500,2000,1400,3000)]),
        f('invalid_degenerate_obstacle', [(1500,2000,1500,3000)]),
        f('invalid_negative_clearance', clearance=-1),
        f('invalid_excessive_clearance', clearance=32769),
        f('invalid_shrunk_bounds', clearance=2049),
        f('invalid_high_coordinate', bounds=(0,0,32769,4096)),
        f('invalid_low_coordinate', bounds=(-32769,0,4096,4096)),
        f('invalid_obstacle_coordinate', [(1500,1000,32769,3000)]),
        f('invalid_obstacle_count', [(1500,1000,2500,3000)] * 65),
        f('maximum_obstacle_count', [(1500,1000,2500,3000)] * 64),
        f('collapsed_legal_bounds', start=(2048,2048), goal=(2048,2048), clearance=2048),
        f('subunit_corner_graze', [(2000,2000,2100,2100)], start=(1900,1972), goal=(1972,1900)),
        f('subunit_corner_penetration', [(2000,2000,2100,2100)], start=(1901,1972), goal=(1972,1901)),
    ]
    rng = random.Random(0x56464E)
    for i in range(12):
        obstacles = []
        for _ in range(4):
            x, z = rng.randint(850,2900), rng.randint(300,3100)
            obstacles.append((x,z,x+rng.randint(100,450),z+rng.randint(100,650)))
        result.append(f(f'seeded_{i:02}', obstacles, start=(200, rng.randint(100,3996)),
                        goal=(3896,rng.randint(100,3996))))
    return result


def self_test(cases: list[Fixture]) -> None:
    r = (10,10,20,20)
    assert not slab_enters_open_rect((0,10),(30,10),r)
    assert not slab_enters_open_rect((0,20),(20,0),r)
    assert slab_enters_open_rect((1,20),(20,1),r)
    assert slab_enters_open_rect((15,15),(15,15),r)
    assert not slab_enters_open_rect((10,15),(10,15),r)
    rng = random.Random(0x534C4142)
    for _ in range(4000):
        a, b = tuple(rng.randint(-30,30) for _ in range(2)), tuple(rng.randint(-30,30) for _ in range(2))
        x, z = rng.randint(-25,20), rng.randint(-25,20)
        box = (x,z,x+rng.randint(1,10),z+rng.randint(1,10))
        assert slab_enters_open_rect(a,b,box) == midpoint_enters_open_rect(a,b,box), (a,b,box)
    # Fixture-derived edge-aligned, coincident, and endpoint/corner pairs add
    # systematic boundary cases that uniformly random points rarely generate.
    for f in cases:
        if not valid_world(f):
            continue
        _, obstacles = geometry(f)
        for box in obstacles:
            x0,z0,x1,z1 = box
            points = [f.start,f.goal,(x0,z0),(x0,z1),(x1,z0),(x1,z1),
                      (x0-1,z0+1),(x0+1,z0+1)]
            for a in points:
                for b in points:
                    assert slab_enters_open_rect(a,b,box) == midpoint_enters_open_rect(a,b,box), (a,b,box)
    by_name = {f.name: f for f in cases}
    for name in ('open_oblique','tiny_off_center','narrow_exact_pass','shrunk_boundary_tangent'):
        f = by_name[name]
        assert reference(f)['distance'] == math.dist(f.start, f.goal), name
    expected = math.hypot(924,1016) + math.hypot(1020,1016) + 1128
    assert math.isclose(reference(by_name['single_box'])['distance'], expected, abs_tol=1e-9)
    expected_detour = math.hypot(1324,3152) + math.hypot(1420,3152) + 328
    assert math.isclose(reference(by_name['long_detour'])['distance'], expected_detour, abs_tol=1e-9)
    for name in ('unreachable_wall','narrow_one_unit_blocked'):
        assert reference(by_name[name])['distance'] is None, name


def verify_route(f: Fixture, expected: dict, actual: dict) -> dict:
    for key in ('valid','start_clear','goal_clear','direct_clear'):
        if actual[key] is not expected[key]:
            raise AssertionError(f'{f.name}: {key}: {actual[key]} != {expected[key]}')
    path = actual['route']
    if not isinstance(path,list) or any(not isinstance(p,list) or len(p)!=2
        or any(type(v) is not int for v in p) for p in path):
        raise AssertionError(f'{f.name}: malformed integer route')
    optimum = expected['distance']
    if optimum is None or optimum == 0:
        if path:
            raise AssertionError(f'{f.name}: invalid/unreachable/same-point route must be empty')
        return dict(reference_length=optimum, route_length=0 if optimum == 0 else None,
                    excess_units=0, excess_percent=0, segments=0)
    if not path or tuple(path[-1]) != f.goal:
        raise AssertionError(f'{f.name}: reachable goal not reached')
    bounds, obstacles = geometry(f)
    previous, length = f.start, 0.0
    for raw in path:
        p = tuple(raw)
        if previous == p or not segment_clear(previous,p,bounds,obstacles):
            raise AssertionError(f'{f.name}: illegal segment {previous} -> {p}')
        length += math.dist(previous,p)
        previous = p
    if length > (1+TOLERANCE_RATIO)*optimum + TOLERANCE_UNITS + 1e-9:
        raise AssertionError(f'{f.name}: route {length} exceeds declared reference tolerance {optimum}')
    if length < optimum - 1e-7:
        raise AssertionError(f'{f.name}: route shorter than independent oracle; oracle needs investigation')
    return dict(reference_length=optimum,route_length=length,excess_units=max(0,length-optimum),
                excess_percent=max(0,100*(length/optimum-1)),segments=len(path))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--debug',type=Path,default=Path('build/windows/sim/Debug/voidfront_navigation_probe.exe'))
    parser.add_argument('--release',type=Path,default=Path('build/windows/sim/Release/voidfront_navigation_probe.exe'))
    parser.add_argument('--out',type=Path)
    parser.add_argument('--repeats',type=int,default=10)
    parser.add_argument('--self-test',action='store_true')
    args = parser.parse_args()
    cases = fixtures()
    self_test(cases)
    print('Independent reference analytic and collision cross-check self-tests passed',flush=True)
    if args.self_test:
        return
    if args.repeats < 10:
        parser.error('at least ten repeats per configuration are required')
    started = datetime.now(timezone.utc)
    out = args.out or Path('artifacts') / started.strftime('navigation-%Y%m%dT%H%M%S%fZ')
    out.mkdir(parents=True,exist_ok=False)
    inputs = '\n'.join(f.stdin() for f in cases) + '\n'
    (out/'fixtures.json').write_text(json.dumps([asdict(f) for f in cases],indent=2)+'\n')
    (out/'probe-input.txt').write_text(inputs)
    references = [reference(f) for f in cases]
    (out/'reference.json').write_text(json.dumps(references,indent=2)+'\n')
    baseline = None
    binaries = {}
    metrics = []
    for label, executable in (('Debug',args.debug),('Release',args.release)):
        executable = executable.resolve(strict=True)
        binaries[label] = dict(path=str(executable),sha256=hashlib.sha256(executable.read_bytes()).hexdigest())
        for repeat in range(args.repeats):
            process = subprocess.run([str(executable)],input=inputs,text=True,capture_output=True,timeout=60)
            (out/f'{label}-{repeat:02}.jsonl').write_text(process.stdout)
            (out/f'{label}-{repeat:02}.stderr.txt').write_text(process.stderr)
            if process.returncode or process.stderr:
                raise AssertionError(f'{label} repeat {repeat} failed {process.returncode}: {process.stderr}')
            rows = [json.loads(line) for line in process.stdout.splitlines()]
            if len(rows) != len(cases):
                raise AssertionError(f'{label}: expected {len(cases)} fixtures, got {len(rows)}')
            current = [dict(name=f.name,**verify_route(f,ref,row)) for f,ref,row in zip(cases,references,rows)]
            if baseline is None:
                baseline, metrics = process.stdout,current
            elif baseline != process.stdout:
                raise AssertionError(f'{label} repeat {repeat}: byte-identical routes failed')
        print(f'{label}: {len(cases)} fixtures x {args.repeats} identical runs passed',flush=True)
    reachable = [row for row in metrics if row['reference_length'] not in (None,0)]
    summary = dict(started_utc=started.isoformat(),finished_utc=datetime.now(timezone.utc).isoformat(),
        status='passed',fixture_count=len(cases),repeats_per_configuration=args.repeats,
        tolerance=dict(relative=TOLERANCE_RATIO,absolute_fixed_point_units=TOLERANCE_UNITS),
        worst_excess_units=max(r['excess_units'] for r in reachable),
        worst_excess_percent=max(r['excess_percent'] for r in reachable),
        mean_excess_percent=sum(r['excess_percent'] for r in reachable)/len(reachable),
        route_sha256=hashlib.sha256(baseline.encode()).hexdigest(),binaries=binaries,fixtures=metrics,
        limits='Bounded static axis-aligned rectangle routes only; no crowd, stepping, runtime, or performance acceptance.')
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({key:summary[key] for key in ('fixture_count','worst_excess_units','worst_excess_percent','mean_excess_percent')}))
    print(f'Evidence: {out.resolve()}')

if __name__ == '__main__':
    main()
