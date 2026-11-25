# replay-parity/

How exactly does melee-pc reproduce the original game? This folder replays Slippi recordings of real
tournament matches through melee-pc and compares the result with the recording, frame by frame.

```
replay-parity/
  README.md            this file
  replays/             25 HoG tournament recordings and two tamper controls
    SOURCE.md          where each recording comes from; what the tamper controls are
    SHA256SUMS         sha256 of every .slp (sha256sum -c SHA256SUMS)
  tools/
    play_one.sh        play one recording through MELEE_SLP_PLAY (headless) and record the playback
    run_all.sh         play every recording and compare each with its original
    mpc_compare.py     compare one playback with its recording
    summ.py            print one line per recording and the summary
```

Case sheets and side-by-side videos (the original game in Dolphin | melee-pc 0.4.2 | melee-pc with
these fixes) were made with separate tooling: (link to come)

## What is compared

`MELEE_SLP_PLAY=<file.slp>` (with `MELEE_BOOT_SCENE=vs`) boots straight into the recording's match and
feeds every port the recorded controller, one sample per tick; with `MELEE_SLP_DIR` the match records
itself. `mpc_compare.py` then compares the playback's post-frame state with the original's on every
frame: action, x, y, facing and percent of every fighter, bit for bit. A recording is **exact** when
every frame matches.

## The commits and what they do to the table

melee-pc 0.4.2 (`0fb0af3`), then one commit per change:

| layer | commit | change | recordings exact | exact frames (of 156,569) | median first divergence |
|---|---|---|---|---|---|
| 0 | `fce89b7` | the playback (`MELEE_SLP_PLAY`) | 0 of 25 | 3,175 | -15 |
| 1 | `9be7fa3` | UCF 0.84 pad buffer and 1.0 cardinals | 0 of 25 | 4,455 | 34 |
| 2 | `4e75827` | the game's own sinf/cosf/tanf/atanf | 2 of 25 | 20,756 | 153 |
| 3 | `9763ac1` | FMA_* macros (doldecomp/melee PR #3691) | 4 of 25 | 54,457 | 1,316 |
| 4 | `8335e31` | UCF 0.84 Shield Drop Extended | 5 of 25 | 64,653 | 1,403 |
| 5 | `7e41f72` | PSMTXConcat/MultVec with the Gekko's rounding | 14 of 25 | 123,280 | 4,167 |
| 6 | `513d558` | UCF 0.84 Tumble and DBOOC | 20 of 25 | 130,516 | 4,069 |

`5e0c887` adds `MELEE_SLP_DUMP` (pictures for videos); it does not change the results. Each change is
written from public sources: UCF 0.84 (AltimorTASDK/ucf, project-slippi/slippi-ssbm-asm),
doldecomp/melee at 79b13a32 (MSL trig, the SDK's matrix assembly), doldecomp/melee PR #3691 (FMA
macros), and Slippi's replay spec.

The tamper controls must diverge exactly where they were altered: `b09-tamper200` at frame 200 on
every layer, `b11-tampered` at frame 500 from layer 5 on (before that, `b11` itself already diverges
earlier, and so does its tampered copy).

## Reproduce a row

You need: Linux, the build requirements of `docs/building.md`, `Xvfb` and `python3`, and your own
Melee NTSC 1.02 disc image. From the repository root, on the newest commit of this branch:

```
git checkout 513d558 -- src extern CMakeLists.txt   # one layer's code (here layer 6); skip it for the newest
cmake -B build -G Ninja && ninja -C build melee
DISC=/path/to/GALE01.iso replay-parity/tools/run_all.sh build/replay-run MELEE_BACKEND=null
```

The first line puts that layer's code under the newest `replay-parity/`; files that only later commits
add stay in the tree but are not built. `git checkout HEAD -- src extern CMakeLists.txt` goes back.

`run_all.sh` plays each recording under a private Xvfb, with the network unshared when
`unshare -rn` is available, compares it, writes `build/replay-run/results.jsonl` (one JSON line per
recording) and prints the summary. With `MELEE_BACKEND=null` nothing is drawn and a full run of the
27 files takes about 7 minutes on a 4-core machine.

## Expected results at layer 6

| recording | result | recording | result | recording | result |
|---|---|---|---|---|---|
| b01 | exact (1,984 frames) | b10 | exact (6,468 frames) | b19 | exact (4,273 frames) |
| b02 | exact (5,362 frames) | b11 | exact (6,812 frames) | b20 | exact (6,186 frames) |
| b03 | exact (5,247 frames) | b12 | exact (3,131 frames) | cal | exact (10,107 frames) |
| b04 | exact (3,075 frames) | b13 | frame 4157 (y) | m1 | exact (3,320 frames) |
| b05 | exact (4,809 frames) | b14 | exact (6,792 frames) | m4 | exact (10,063 frames) |
| b06 | exact (5,780 frames) | b15 | exact (7,846 frames) | m9 | frame 5369 (y) |
| b07 | exact (4,857 frames) | b16 | frame 2085 (y) | s01 | frame 1261 (action) |
| b08 | frame 4069 (y) | b17 | exact (3,983 frames) |  |  |
| b09 | exact (4,147 frames) | b18 | exact (8,718 frames) |  |  |

The four recordings that diverge on `y` (b08, b13, b16, m9) differ on 1 to 4 frames only, by about
5e-8, on the ground near Battlefield's main-floor line junction, and match on every frame after;
s01 diverges at frame 1261 on a Samus action change. Their causes are not found yet.
