"""Check the real, FLTK-linked application using its textual output."""
import collections
import os
import re
import subprocess
import sys

program = os.path.abspath(sys.argv[1])
env = dict(os.environ, DISPLAY="", WAM_TICK_MS="1")
pattern = re.compile(
    r"^\((\d+),(\d+)\) (\d+) (lawnimp_mole|lawnimp_whack) "
    r"(creating|created|whacking|whacked|expired)$"
)
for count, capacity in [(0, 4), (1, 1), (30, 1), (30, 4), (30, 0)]:
    result = subprocess.run(
        [program, str(count), str(capacity)], env=env,
        capture_output=True, text=True, timeout=30, check=True,
    )
    events = {name: collections.Counter() for name in
              ("creating", "created", "whacking", "whacked", "expired")}
    for line in result.stdout.splitlines():
        match = pattern.fullmatch(line)
        if match:
            x, y, tid, function, event = match.groups()
            expected = "lawnimp_mole" if event in ("creating", "created") else "lawnimp_whack"
            assert function == expected, line
            events[event][(x, y)] += 1
    assert sum(events["creating"].values()) == count, result.stdout
    assert all(values == events["creating"] for values in events.values()), result.stdout
    assert "lawnimp_stop finished" in result.stdout, result.stdout
    assert "lawnimp_free freed" in result.stdout, result.stdout

for args in [("-1",), ("abc",), ("1", "-1"), ("1", "abc"), ("0", "4", "extra")]:
    result = subprocess.run([program, *args], env=env, capture_output=True, timeout=10)
    assert result.returncode != 0 and result.stderr, args
print("Simulation tests passed: zero/one/many moles, capacity 1/4/unbounded, bad inputs.")
