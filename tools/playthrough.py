#!/usr/bin/env python3
"""Drive the game to a map square, and print the emulator script that gets there.

tests/emu scripts are open loop: they press keys and check results, with no way
to ask where the player ended up and decide what to press next. That is fine for
a few steps, but a fixed list of moves cannot cross a level -- a door costs a
press to open before it costs one to walk through, and a robot standing in the
way costs an unknown number -- so the sequence desynchronises and every move
after that is wrong.

This closes the loop. rp6502-emu takes `--script -`, so the emulator can be
driven a line at a time: read the player's position out of the probe block,
plan a route from *there* with a breadth first search over the level's
walkability, press one key, and look again. What it prints is the script that
worked, which can then be replayed open loop -- the emulator is deterministic
under --seed, so a recorded route stays valid until the game's timing changes.

Walkability comes from TILE_ATTRIB, bit 0, which is the same bit REQUEST_WALK
tests through MOVE_TYPE. An unlocked door square counts as passable because the
door AI opens it when the player comes near; the frames that takes are what the
loop absorbs. A locked one does not, because this drives the game from a cold
start with no keys -- UNIT_C on a door unit is 0 for unlocked and 1, 2 or 3 for
the spade, heart and star keys.

The map is not the whole story, though. A door is shut in the map until the
player walks into it, and then it is a six state machine that takes a while to
open, so the first press buys nothing and the square stays impassable for some
frames. CHECK_FOR_UNIT also stops the player entering a square another unit is
standing in, and units move. So when the player stops making progress the loop
first waits, in case something is opening, and only after that gives up on the
square, marks it blocked and plans a route that avoids it.

    tools/playthrough.py e 75 42 > tests/emu/route.txt
"""
import argparse
import pathlib
import subprocess
import sys
from collections import deque

ROOT = pathlib.Path(__file__).resolve().parent.parent
KEYS = ((-1, 0, 'j'), (1, 0, 'l'), (0, -1, 'i'), (0, 1, 'k'))
DOOR = 10


def load(level):
    att = (ROOT / 'assets/gen/tiles.bin').read_bytes()[4864:5120]
    d = (ROOT / f'assets/gen/level-{level}.bin').read_bytes()
    #     attrib  UNIT_TYPE  UNIT_LOC_X  UNIT_LOC_Y  UNIT_C (a door's lock)  MAP
    return att, d[0:64], d[64:128], d[128:192], d[320:384], d[512:]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('level')
    ap.add_argument('x', type=int)
    ap.add_argument('y', type=int)
    ap.add_argument('--rom', default='build/cc65/release/src/robots.rp6502')
    ap.add_argument('--emu', default='tools/rp6502-emu')
    ap.add_argument('--seed', default='1')
    ap.add_argument('--max-steps', type=int, default=600)
    a = ap.parse_args()

    att, T, X, Y, C, MAP = load(a.level)
    doors = {(X[i], Y[i]) for i in range(64) if T[i] == DOOR and C[i] == 0}
    target = (a.x, a.y)

    def walkable(x, y):
        if not (0 <= x < 128 and 0 <= y < 64):
            return False
        if (x, y) in doors:
            return True
        return att[MAP[y * 128 + x]] & 1

    blocked = set()

    def route(src):
        seen = {src: None}
        q = deque([src])
        while q:
            c = q.popleft()
            if c == target:
                break
            for dx, dy, k in KEYS:
                n = (c[0] + dx, c[1] + dy)
                if n not in seen and n not in blocked and walkable(*n):
                    seen[n] = (c, k)
                    q.append(n)
        if target not in seen:
            return None
        out, c = [], target
        while seen[c]:
            c, k = seen[c]
            out.append(k)
        return out[::-1]

    p = subprocess.Popen([a.emu, '--mute', '--seed', a.seed, '--tmpdrive',
                          '--script', '-', a.rom],
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         text=True, bufsize=1)
    script = []

    def send(*lines):
        for line in lines:
            p.stdin.write(line + '\n')
            script.append(line)
        p.stdin.flush()

    def probe():
        p.stdin.write('dump xram:$FE00 16\n')
        p.stdin.flush()
        out = []
        while len(out) < 16:
            line = p.stdout.readline()
            if not line:
                return None
            line = line.strip()
            if line and all(c in '0123456789ABCDEFabcdef ' for c in line):
                out += [int(t, 16) for t in line.split()]
        return out

    # The intro menu: down to CHANGE MAP, space once per level, back up, start.
    send('wait "MENU"', 'run 30')
    send('press k', 'run 6', 'release k', 'run 10')
    for _ in range(ord(a.level) - ord('a')):
        send('press space', 'run 6', 'release space', 'run 10')
    send('press i', 'run 6', 'release i', 'run 10')
    send('press space', 'run 6', 'release space')
    send('wait "BRINGUP OK"', 'run 20')

    pos, stuck, status = None, 0, 'ran out of steps'
    for _ in range(a.max_steps):
        v = probe()
        if v is None:
            status = 'the emulator closed'
            break
        here = (v[4], v[5])
        if here == target:
            status = f'reached {target}'
            break
        if v[2] != 2:
            status = f'left play with state {v[2]} at {here}'
            break
        stuck = stuck + 1 if here == pos else 0
        pos = here
        plan = route(here)
        if not plan:
            status = f'no route from {here}'
            break
        if stuck and stuck % 4 == 0:
            # Give a door time to finish opening, or a unit time to wander off.
            send('run 60')
        if stuck > 16:
            # It is not going to open and it is not going to move. Take the
            # square out of the map and go round it.
            dx, dy = {'j': (-1, 0), 'l': (1, 0),
                      'i': (0, -1), 'k': (0, 1)}[plan[0]]
            blocked.add((here[0] + dx, here[1] + dy))
            stuck = 0
            continue
        send(f'press {plan[0]}', 'run 8', f'release {plan[0]}', 'run 8')

    p.stdin.close()
    p.kill()
    print('\n'.join(script))
    print(f'# {status}', file=sys.stderr)
    return 0 if status.startswith('reached') else 1


if __name__ == '__main__':
    sys.exit(main())
