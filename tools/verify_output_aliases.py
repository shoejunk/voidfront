"""Protect replay inputs and output files from aliased trace destinations."""
import argparse
import os
from pathlib import Path
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]


def verify(executable, output):
    if (ROOT / ".voidfront-agent/STOP").exists():
        raise RuntimeError("Voidfront STOP switch is present")
    output.mkdir(parents=True, exist_ok=True)
    hidden = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0

    def run(*args):
        return subprocess.run([str(executable), *map(str, args)], capture_output=True,
                              text=True, timeout=15, creationflags=hidden)

    # Every destructive regression uses disposable fixtures, never existing evidence.
    folder = output / ("aliases-" + uuid.uuid4().hex)
    folder.mkdir()
    try:
        golden = folder / "golden.vfr"
        result = run("--ticks", 50, "--record", golden)
        assert result.returncode == 0, result.stderr
        payload = golden.read_bytes()
        cases = ["exact", "hardlink"] + (["case"] if os.name == "nt" else [])
        for operation in ("replay", "record"):
            for kind in cases:
                original = folder / f"{operation}-{kind}.vfr"
                original.write_bytes(payload)
                alias = original
                if kind == "hardlink":
                    alias = folder / f"{operation}-linked.trace"
                    os.link(original, alias)
                elif kind == "case":
                    alias = original.with_name(original.name.upper())
                result = run(f"--{operation}", original, "--trace", alias)
                assert result.returncode != 0, f"{operation}/{kind}: alias accepted"
                assert "trace must differ" in result.stderr, result.stderr
                assert original.read_bytes() == payload, f"{operation}/{kind}: input changed"
                print(f"PASS output alias: {operation}/{kind}", flush=True)
        if os.name == "nt":
            # Identity checks alone cannot detect two aliases of a nonexistent output.
            original = folder / "new-record.vfr"
            alias = original.with_name(original.name.upper())
            result = run("--record", original, "--trace", alias)
            assert result.returncode != 0 and "trace must differ" in result.stderr, result.stderr
            assert not original.exists(), "rejected output alias created a file"
            print("PASS output alias: record/new-case", flush=True)
    finally:
        # Delete only direct fixtures in the exclusive directory created above.
        for fixture in folder.iterdir():
            fixture.unlink()
        folder.rmdir()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--out", type=Path, default=ROOT / "artifacts/output-aliases")
    args = parser.parse_args()
    verify(args.executable.resolve(), args.out.resolve())
