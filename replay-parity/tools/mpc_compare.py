"""Compare a melee-pc playback with its reference Slippi recording, frame by frame.

The playback (MELEE_SLP_PLAY) records itself with melee-pc's own Slippi writer
(MELEE_SLP_DIR), so both sides are .slp files numbered by the same rule: frame -123 is the first
tick the fighters ran, and frame f's post-frame event is the state after frame f's update.

Per frame and per port (leaders only) the post-frame action, x, y, facing and percent are compared
as raw bits (sibling of mu_compare.py, same fields). Before that, alignment is checked
independently of the state: every frame's Frame Start scene tick (0x3A +9) and every port's
pre-frame physical buttons and raw sticks (0x31, 0x3B, 0x40..0x42) must agree, i.e. the playback
fed frame f's inputs on frame f's tick. The pre-frame seed (0x37 +7) is reported too.

Usage: mpc_compare.py REFERENCE.slp PLAYBACK.slp
Output: one JSON line.
"""
import json
import struct
import sys
from pathlib import Path

FIELDS = ("action", "x", "y", "facing", "percent")


def parse(path):
    d = Path(path).read_bytes()
    i = d.find(b"raw[$U#l")
    n = struct.unpack(">I", d[i + 8:i + 12])[0]
    raw = d[i + 12:i + 12 + n] if n else d[i + 12:]
    tsz = raw[1]
    sizes = {raw[2 + 3 * k]: struct.unpack(">H", raw[3 + 3 * k:5 + 3 * k])[0] for k in range((tsz - 1) // 3)}
    pos = 1 + tsz
    post, pre, start = {}, {}, {}
    while pos < len(raw):
        c = raw[pos]
        size = sizes.get(c)
        if size is None or pos + 1 + size > len(raw):
            break
        b = raw[pos:pos + 1 + size]
        if c == 0x3A:
            f, seed, tick = struct.unpack(">iII", b[1:13])
            start[f] = dict(seed=seed, tick=tick)
        elif c == 0x37 and b[6] == 0:
            f, p = struct.unpack(">i", b[1:5])[0], b[5]
            pre.setdefault(f, {})[p] = dict(
                seed=b[7:11].hex().upper(),
                inputs=(b[0x31:0x33].hex(), b[0x3B], b[0x40], b[0x41], b[0x42]))
        elif c == 0x38 and b[6] == 0:
            f, p = struct.unpack(">i", b[1:5])[0], b[5]
            post.setdefault(f, {})[p] = dict(
                character=b[7], action=struct.unpack(">H", b[8:10])[0],
                x=b[0xA:0xE].hex().upper(), y=b[0xE:0x12].hex().upper(),
                facing=b[0x12:0x16].hex().upper(), percent=b[0x16:0x1A].hex().upper(),
                stocks=b[0x21], action_frame=b[0x22:0x26].hex().upper(),
                vel=b[0x35:0x49].hex().upper())
        pos += 1 + size
    return post, pre, start


def ulps(a, b):
    """distance in float32 ULPs between two hex bit patterns (sign-aware ordering)"""
    def key(h):
        u = int(h, 16)
        return u if u < 0x80000000 else -(u - 0x80000000)
    return abs(key(a) - key(b))


def f32(h):
    return struct.unpack(">f", bytes.fromhex(h))[0]


def compare(ref_path, mpc_path):
    rpost, rpre, rstart = parse(ref_path)
    mpost, mpre, mstart = parse(mpc_path)
    first, last = min(rpost), max(rpost)
    res = dict(recording=Path(ref_path).stem, first=first, last=last,
               mpc_last=max(mpost) if mpost else None)
    # alignment: same scene tick and same fed inputs on every frame both have
    tick_bad = inp_bad = seed_first_diff = None
    for f in range(first, last + 1):
        if f not in mstart or f not in mpre:
            break
        if tick_bad is None and rstart[f]["tick"] != mstart[f]["tick"]:
            tick_bad = dict(frame=f, ref=rstart[f]["tick"], mpc=mstart[f]["tick"])
        for p, r in rpre[f].items():
            m = mpre[f].get(p)
            if inp_bad is None and (m is None or m["inputs"] != r["inputs"]):
                inp_bad = dict(frame=f, port=p, ref=r["inputs"], mpc=m and m["inputs"])
            if seed_first_diff is None and m is not None and m["seed"] != r["seed"]:
                seed_first_diff = dict(frame=f, port=p, ref=r["seed"], mpc=m["seed"])
    res.update(scene_tick_mismatch=tick_bad, input_mismatch=inp_bad, preframe_seed_first_diff=seed_first_diff)
    players = sorted({p for fr in rpost.values() for p in fr})
    res["characters"] = {p: rpost[first][p]["character"] for p in players}
    exact, first_div = 0, None
    for f in range(first, last + 1):
        if f not in mpost:
            first_div = dict(frame=f, why="playback ended (frame missing)")
            break
        bad = []
        for p in players:
            ref = rpost.get(f, {}).get(p)
            mine = mpost[f].get(p)
            if ref is None:
                continue
            if mine is None:
                bad.append(dict(port=p, field="missing"))
                continue
            for key in FIELDS:
                if mine[key] != ref[key]:
                    d = dict(port=p, field=key, ref=ref[key], mpc=mine[key])
                    if key != "action":
                        d.update(ulps=ulps(ref[key], mine[key]), ref_f=f32(ref[key]), mpc_f=f32(mine[key]))
                    bad.append(d)
        if bad:
            ctx = {p: dict(action=rpost[f][p]["action"], action_frame=f32(rpost[f][p]["action_frame"]),
                           prev_action=rpost.get(f - 1, {}).get(p, {}).get("action"))
                   for p in players if p in rpost.get(f, {})}
            first_div = dict(frame=f, diffs=bad[:6], ref_context=ctx)
            break
        exact += 1
    res.update(compared_to=last, exact_frames=exact, first_divergence=first_div)
    # earliest difference in the five post-frame velocities (self air x, self y, attack x/y,
    # self ground x), which can lead the position by a frame or more; reported, not scored
    first_vel = None
    names = ("self_air_x", "self_y", "attack_x", "attack_y", "self_ground_x")
    for f in range(first, last + 1):
        if f not in mpost or first_vel is not None:
            break
        for p in players:
            ref, mine = rpost.get(f, {}).get(p), mpost[f].get(p)
            if ref is None or mine is None or ref["vel"] == mine["vel"]:
                continue
            for k, nm in enumerate(names):
                a, b = ref["vel"][8 * k:8 * k + 8], mine["vel"][8 * k:8 * k + 8]
                if a != b:
                    first_vel = dict(frame=f, port=p, field=nm, ref=a, mpc=b, ulps=ulps(a, b),
                                     action=ref["action"], action_frame=f32(ref["action_frame"]))
                    break
            break
    res["first_velocity_divergence"] = first_vel
    return res


if __name__ == "__main__":
    print(json.dumps(compare(sys.argv[1], sys.argv[2])))
