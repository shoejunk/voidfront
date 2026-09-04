"""Compare fixed-tick hash traces, reporting the first divergent tick."""
import argparse
from itertools import zip_longest

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("left")
parser.add_argument("right")
args = parser.parse_args()
with open(args.left, encoding="utf-8") as left, open(args.right, encoding="utf-8") as right:
    count = 0
    for count, (a, b) in enumerate(zip_longest(left, right), 1):
        if a != b:
            print(f"First divergence at trace line {count}: {a.strip() if a else '<EOF>'} != {b.strip() if b else '<EOF>'}")
            raise SystemExit(1)
print(f"PASS: {count} identical tick hashes")
