/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Slippi replay playback for parity measurement (MELEE_SLP_PLAY=<file.slp>).
 *
 * With MELEE_BOOT_SCENE=vs the game boots straight into the VS match the
 * recording's Game Start event describes, and every present port is fed the
 * recorded controller, one sample per simulation tick. The match then records
 * itself through src/pc/slp.c (MELEE_SLP_DIR), so the two files can be
 * compared frame by frame (parity/tools/mpc_compare.py).
 *
 * Event layouts are Slippi's replay spec (project-slippi/slippi-wiki
 * SPEC.md); the recordings this reads are version 3.17.0 or later:
 *   Game Start 0x36   +0x05 the 0x138-byte game info block, +0x13D the RNG
 *                     seed at the top of the VS scene setup
 *   Pre-Frame 0x37    +0x0D/0x11/0x15 position x/y and facing, +0x31
 *                     physical buttons, +0x33/0x37 physical L/R (0..1),
 *                     +0x3B raw stick X, +0x40 raw stick Y, +0x41/0x42 raw
 *                     C-stick X/Y
 *   Frame Start 0x3A  +0x05 the frame's RNG seed, +0x09 the scene frame
 *                     counter
 *
 * Inputs: buttons and raw sticks as recorded; each trigger is the clamped
 * byte the pad library divided by scale_analogLR (140) to get the recorded
 * value (controller.c HSD_PadADConvert), rounded back: (u8)(v * 140 + 0.5).
 * The sample is written into the raw pad queue entry the tick is about to
 * consume (HSD_PadLibData.qread), so queue depth and timing cannot shift it.
 *
 * Ticks are mapped by the scene frame counter (gm_801A4BB8): recorded frame
 * f runs on scene tick fsc[f]. A gap between consecutive frames is a pause,
 * which Slippi does not record: its first tick is fed the next frame's sample
 * (whose Start press pauses), the held ticks the same sample with Start
 * released, and tick fsc[f] the sample again (the unpause, simulated).
 *
 * The RNG is set to the Game Start seed where slp.c reads it (top of
 * fn_8016E730) and, unless MELEE_SLP_PLAY_SEED=once, to each frame's Frame
 * Start seed where slp.c reads that (before the tick's GObj procs). Spawn
 * points are the first frame's pre-frame position and facing. With no
 * periodic pad alarm the scene loop polls once per iteration and frame
 * pacing is skipped, so playback runs as fast as the host allows. When the
 * last recorded frame has run, or the VS scene ends, the process exits
 * through pc_exit_requested. */
#include "compat.h"
#include "pc/pc.h"
#include "pc/slp.h"

#include <dolphin/pad.h>
#include <sysdolphin/baselib/controller.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/random.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wscalar-storage-order"
#include <melee/gm/gm_1A3F.h>
#include <melee/gm/gmscene.h>
#include <melee/gm/types.h>
#include <melee/mn/types.h>
#include <melee/pl/player.h>
#pragma GCC diagnostic pop

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct GameSceneInfo* gm_804D6720; /* current scene, gmscene.c */

#define BTN_START 0x1000

typedef struct Sample {
    u16 button;
    s8 sx, sy, cx, cy;
    u8 l, r;
} Sample;

static int s_on = -1;
static bool s_once;
static int s_nports;
static int s_port[4];   /* present ports, ascending */
static u32 s_mask;
static s32 s_first;     /* first recorded frame (-123) */
static int s_n;         /* frames */
static Sample* s_in;    /* [s_n][s_nports] */
static u32* s_fseed;    /* Frame Start seed per frame */
static u32* s_fsc;      /* scene frame counter per frame */
static u32 s_gs_seed;
static u8 s_block[0x138];
static float s_spawn[4][3];
static int s_k;         /* cursor: first frame whose fsc >= tick */
static int s_cur = -1;  /* frame simulated on this tick, -1 = none */
static bool s_done;
static u32 s_ticks, s_bad, s_pauses;

static u32 be32(const u8* p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}
static u16 be16(const u8* p) {
    return (u16)((p[0] << 8) | p[1]);
}
static float bef(const u8* p) {
    union {
        u32 u;
        float f;
    } x;
    x.u = be32(p);
    return x.f;
}
static u8 trig(const u8* p) {
    float v = bef(p);
    float x = v * 140.0F;
    x = x + 0.5F;
    s32 r = (s32)x;
    return (u8)(r < 0 ? 0 : r > 140 ? 140 : r);
}

static void fail(const char* why) {
    pc_log_line("slp-play: %s", why);
    fprintf(stderr, "slp-play: %s\n", why);
    exit(3);
}

static void load(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        fail("cannot open the recording");
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8* d = malloc((size_t)sz);
    if (d == NULL || fread(d, 1, (size_t)sz, f) != (size_t)sz) {
        fail("cannot read the recording");
    }
    fclose(f);
    const u8* raw = NULL;
    for (long i = 0; i + 12 <= sz; i++) {
        if (memcmp(d + i, "raw[$U#l", 8) == 0) {
            u32 n = be32(d + i + 8);
            raw = d + i + 12;
            if (n == 0 || (long)(i + 12 + n) > sz) {
                n = (u32)(sz - i - 12);
            }
            sz = n;
            break;
        }
    }
    if (raw == NULL || raw[0] != 0x35) {
        fail("no raw element / Event Payloads");
    }
    u16 size[256] = {0};
    for (int k = 2; k < raw[1]; k += 3) {
        size[raw[k]] = be16(raw + k + 1);
    }
    /* pass 1: frame range and ports */
    const u8* gs = NULL;
    s32 lo = 0x7FFFFFFF, hi = -0x7FFFFFFF;
    for (long off = 1 + raw[1]; off < sz && size[raw[off]] != 0; off += 1 + size[raw[off]]) {
        const u8* e = raw + off;
        if (e[0] == 0x36 && gs == NULL) {
            gs = e;
        } else if (e[0] == 0x37 && e[6] == 0) {
            s32 fr = (s32)be32(e + 1);
            lo = fr < lo ? fr : lo;
            hi = fr > hi ? fr : hi;
            s_mask |= 1u << (e[5] & 3);
        }
    }
    if (gs == NULL || lo > hi || size[0x37] < 0x42 || size[0x3A] < 0x0C) {
        fail("no Game Start / pre-frames, or a pre-v3.17 recording");
    }
    for (int p = 0; p < 4; p++) {
        if (s_mask >> p & 1) {
            s_port[s_nports++] = p;
        }
    }
    s_first = lo;
    s_n = hi - lo + 1;
    s_in = calloc((size_t)s_n * (size_t)s_nports, sizeof *s_in);
    s_fseed = calloc((size_t)s_n, sizeof *s_fseed);
    s_fsc = calloc((size_t)s_n, sizeof *s_fsc);
    u8* have = calloc((size_t)s_n, 1);
    memcpy(s_block, gs + 5, 0x138);
    s_gs_seed = be32(gs + 0x13D);
    /* pass 2: samples (a rollback re-send keeps the last version) */
    for (long off = 1 + raw[1]; off < sz && size[raw[off]] != 0; off += 1 + size[raw[off]]) {
        const u8* e = raw + off;
        if (e[0] == 0x3A) {
            s32 fr = (s32)be32(e + 1);
            if (fr >= lo && fr <= hi) {
                s_fseed[fr - lo] = be32(e + 5);
                s_fsc[fr - lo] = be32(e + 9);
                have[fr - lo] |= 0x10;
            }
        } else if (e[0] == 0x37 && e[6] == 0) {
            s32 fr = (s32)be32(e + 1);
            int port = e[5] & 3, j;
            for (j = 0; j < s_nports && s_port[j] != port; j++) {
            }
            Sample* s = &s_in[(size_t)(fr - lo) * (size_t)s_nports + (size_t)j];
            s->button = be16(e + 0x31);
            s->l = trig(e + 0x33);
            s->r = trig(e + 0x37);
            s->sx = (s8)e[0x3B];
            s->sy = (s8)e[0x40];
            s->cx = (s8)e[0x41];
            s->cy = (s8)e[0x42];
            have[fr - lo] |= (u8)(1u << j);
            if (fr == lo) {
                s_spawn[port][0] = bef(e + 0x0D);
                s_spawn[port][1] = bef(e + 0x11);
                s_spawn[port][2] = bef(e + 0x15);
            }
        }
    }
    const u8 full = (u8)(0x10 | ((1u << s_nports) - 1));
    for (int i = 0; i < s_n; i++) {
        if (have[i] != full || (i > 0 && s_fsc[i] <= s_fsc[i - 1])) {
            fail("a frame is missing a port, its Frame Start, or the scene counter goes back");
        }
    }
    free(have);
    free(d);
    pc_log_line("slp-play: %s: frames %d..%d (%d), ports mask %X, game-start seed %08X, "
                "scene ticks %u..%u, per-frame seed %s",
        path, lo, hi, s_n, s_mask, s_gs_seed, s_fsc[0], s_fsc[s_n - 1], s_once ? "off" : "on");
}

bool pc_slp_play_active(void) {
    if (s_on < 0) {
        const char* p = getenv("MELEE_SLP_PLAY");
        const char* o = getenv("MELEE_SLP_PLAY_SEED");
        s_once = o != NULL && strcmp(o, "once") == 0;
        s_on = p != NULL && p[0] != '\0';
        if (s_on) {
            load(p);
        }
    }
    return s_on;
}

static bool in_vs(void) {
    return gm_804D6720 != NULL && gm_804D6720->scene_kind == GS_VS;
}

/* --- match setup ----------------------------------------------------------- */

/* bit n of a block byte, the GameCube's order: n = 0 is the 0x80 bit, the
 * first-declared bitfield (slp.c BITS8, slp_format.c slp_info_block) */
static u32 bit(const u8* g, int byte, int n) {
    return (g[byte] >> (7 - n)) & 1;
}

/* The recording's game info block over StartMeleeData: the inverse of
 * slp.c build_info and slp_format.c slp_info_block. The words at 0x38..0x5F
 * (callbacks and data pointers, zero in a recording) are left as the mode
 * set them. */
void pc_slp_play_start_data(struct StartMeleeData* d) {
    if (!pc_slp_play_active()) {
        return;
    }
    const u8* g = s_block;
    StartMeleeRules* r = &d->rules;
    r->match_kind = g[0x0] >> 5;
    r->x0_3 = (g[0x0] >> 2) & 7;
    r->timer_enabled = bit(g, 0x0, 6);
    r->timer_counts_up = bit(g, 0x0, 7);
    r->x1_0 = bit(g, 0x1, 0);
    r->x1_1 = bit(g, 0x1, 1);
    r->x1_2 = bit(g, 0x1, 2);
    r->x1_3 = bit(g, 0x1, 3);
    r->x1_4 = bit(g, 0x1, 4);
    r->x1_5 = bit(g, 0x1, 5);
    r->timer_shows_hours = bit(g, 0x1, 6);
    r->friendly_fire = bit(g, 0x1, 7);
    r->is_stock = bit(g, 0x2, 0);
    r->x2_1 = bit(g, 0x2, 1);
    r->x2_2 = bit(g, 0x2, 2);
    r->single_button = bit(g, 0x2, 3);
    r->disable_pausing = bit(g, 0x2, 4);
    r->x2_5 = bit(g, 0x2, 5);
    r->x2_6 = bit(g, 0x2, 6);
    r->x2_7 = bit(g, 0x2, 7);
    r->x3_0 = bit(g, 0x3, 0);
    r->x3_1 = bit(g, 0x3, 1);
    r->x3_2 = bit(g, 0x3, 2);
    r->x3_3 = bit(g, 0x3, 3);
    r->x3_4 = bit(g, 0x3, 4);
    r->x3_5 = bit(g, 0x3, 5);
    r->x3_6 = bit(g, 0x3, 6);
    r->x3_7 = bit(g, 0x3, 7);
    r->x4_0 = bit(g, 0x4, 0);
    r->is_vs = bit(g, 0x4, 1);
    r->x4_2 = bit(g, 0x4, 2);
    r->x4_3 = bit(g, 0x4, 3);
    r->x4_4 = bit(g, 0x4, 4);
    r->x4_5 = bit(g, 0x4, 5);
    r->x4_6 = bit(g, 0x4, 6);
    r->x4_7 = bit(g, 0x4, 7);
    r->x5_0 = bit(g, 0x5, 0);
    r->x5_1 = bit(g, 0x5, 1);
    r->x5_2 = bit(g, 0x5, 2);
    r->x5_3 = bit(g, 0x5, 3);
    r->x5_4 = bit(g, 0x5, 4);
    r->x5_5 = bit(g, 0x5, 5);
    r->x5_6 = bit(g, 0x5, 6);
    r->x5_7 = bit(g, 0x5, 7);
    r->x6 = g[0x6];
    r->x7 = g[0x7];
    r->is_teams = g[0x8];
    r->x9 = g[0x9];
    r->xA = g[0xA];
    r->item_freq = (s8)g[0xB];
    r->sd_penalty = (s8)g[0xC];
    r->xD = g[0xD];
    r->stkind = be16(g + 0xE);
    r->time_limit = be32(g + 0x10);
    r->x14 = g[0x14];
    r->x18 = be32(g + 0x18);
    r->x1C_pad[0] = be32(g + 0x1C);
    r->x20 = ((u64)be32(g + 0x20) << 32) | be32(g + 0x24);
    r->it_kind = (ItemKind)(s32)be32(g + 0x28);
    r->x2C = bef(g + 0x2C);
    r->x30 = bef(g + 0x30);
    r->game_speed = bef(g + 0x34);
    for (int i = 0; i < GM_MAX_PLAYERS; i++) {
        const u8* o = g + 0x60 + 0x24 * i;
        PlayerInitData* p = &d->players[i];
        p->ckind = (s8)o[0x0];
        p->slot_type = o[0x1];
        p->stocks = (s8)o[0x2];
        p->color = o[0x3];
        p->slot = o[0x4];
        p->spawn_pos = (s8)o[0x5];
        p->spawn_dir = (s8)o[0x6];
        p->sub_color = o[0x7];
        p->handicap = (s8)o[0x8];
        p->team = o[0x9];
        p->nametag = o[0xA];
        p->xB = o[0xB];
        p->rumble_enabled = bit(o, 0xC, 0);
        p->xC_b1 = bit(o, 0xC, 1);
        p->vs_metal = bit(o, 0xC, 2);
        p->xC_b3 = bit(o, 0xC, 3);
        p->vs_invisible = bit(o, 0xC, 4);
        p->xC_b5 = bit(o, 0xC, 5);
        p->xC_b6 = bit(o, 0xC, 6);
        p->xC_b7 = bit(o, 0xC, 7);
        p->xD_b0 = bit(o, 0xD, 0);
        p->xD_b1 = bit(o, 0xD, 1);
        p->xD_b2 = bit(o, 0xD, 2);
        p->xD_b3 = bit(o, 0xD, 3);
        p->xD_b4 = bit(o, 0xD, 4);
        p->xD_b5 = bit(o, 0xD, 5);
        p->xD_b6 = bit(o, 0xD, 6);
        p->xD_b7 = bit(o, 0xD, 7);
        p->cpu_kind = o[0xE];
        p->cpu_level = o[0xF];
        p->damage = be16(o + 0x10);
        p->damage1 = be16(o + 0x12);
        p->hp = be16(o + 0x14);
        p->attack_ratio = bef(o + 0x18);
        p->defense_ratio = bef(o + 0x1C);
        p->model_scale = bef(o + 0x20);
    }
    pc_log_line("slp-play: start data from the recording: stage %u, match kind %u, "
                "p0 %d/%u p1 %d/%u p2 %d/%u p3 %d/%u",
        (unsigned)r->stkind, (unsigned)r->match_kind, d->players[0].ckind,
        d->players[0].slot_type, d->players[1].ckind, d->players[1].slot_type,
        d->players[2].ckind, d->players[2].slot_type, d->players[3].ckind,
        d->players[3].slot_type);
}

/* gmvs.c fn_8016E730's top: the Game Start seed, where Slippi reads it */
void pc_slp_play_vs_setup(void) {
    if (!pc_slp_play_active()) {
        return;
    }
    *HSD_RandSeedPtr = s_gs_seed;
    s_k = 0;
    s_cur = -1;
    s_done = false;
}

/* gmvs.c, after getSpawnPoint: the recording's first-frame position and facing */
void pc_slp_play_spawn(int slot, float* x, float* y, float* z) {
    if (!pc_slp_play_active() || slot < 0 || slot > 3 || !(s_mask >> slot & 1)) {
        return;
    }
    *x = s_spawn[slot][0];
    *y = s_spawn[slot][1];
    *z = 0.0F;
    Player_SetFacingDirection(slot, s_spawn[slot][2]);
}

/* --- per tick ---------------------------------------------------------------- */

static void fill(PADStatus* st, int frame, u16 clear) {
    for (int p = 0, j = 0; p < 4; p++) {
        memset(&st[p], 0, sizeof st[p]);
        if (!(s_mask >> p & 1)) {
            st[p].err = PAD_ERR_NO_CONTROLLER;
            continue;
        }
        st[p].err = PAD_ERR_NONE;
        if (frame >= 0) {
            const Sample* s = &s_in[(size_t)frame * (size_t)s_nports + (size_t)j];
            st[p].button = (u16)(s->button & ~clear);
            st[p].stickX = s->sx;
            st[p].stickY = s->sy;
            st[p].substickX = s->cx;
            st[p].substickY = s->cy;
            st[p].triggerLeft = s->l;
            st[p].triggerRight = s->r;
        }
        j++;
    }
}

/* gmscene.c, before each simulation tick: the queue entry the tick consumes */
void pc_slp_play_pre_tick(void) {
    static bool seen_vs;
    if (!pc_slp_play_active()) {
        return;
    }
    if (!in_vs()) {
        if (seen_vs && !pc_exit_requested) {
            pc_log_line("slp-play: the VS scene ended before the recording did "
                        "(%u ticks played); exiting", s_ticks);
            pc_exit_requested = true;
        }
        return;
    }
    seen_vs = true;
    PadLibData* p = &HSD_PadLibData;
    p->qtype = 2; /* a full queue drops new samples, never moves qread */
    s_cur = -1;
    if (p->qcount == 0 || p->queue == NULL) {
        if (s_bad++ < 8) {
            pc_log_line("slp-play: tick with an empty pad queue");
        }
        return;
    }
    PADStatus* head = &p->queue->stat[p->qread * 4];
    const u32 t = gm_801A4BB8();
    while (s_k < s_n && s_fsc[s_k] < t) {
        s_k++;
    }
    if (s_k >= s_n) {
        fill(head, -1, 0);
        if (!s_done) {
            s_done = true;
            pc_log_line("slp-play: recording over at scene tick %u (%u ticks, %u pauses, %u bad)",
                t, s_ticks, s_pauses, s_bad);
            pc_exit_requested = true;
        }
        return;
    }
    s_ticks++;
    if (s_fsc[s_k] == t) {
        s_cur = s_k;
        fill(head, s_k, 0);
    } else if (s_k > 0 && t == s_fsc[s_k - 1] + 1) {
        s_pauses++;
        pc_log_line("slp-play: pause before frame %d: ticks %u..%u", s_k + s_first, t,
            s_fsc[s_k] - 1);
        fill(head, s_k, 0); /* the pause press */
    } else {
        fill(head, s_k, BTN_START); /* held paused, Start released */
    }
}

/* gmscene.c, right before slp.c reads the frame-start seed */
void pc_slp_play_frame_start(void) {
    if (!pc_slp_play_active() || !in_vs() || s_cur < 0 || s_once) {
        return;
    }
    *HSD_RandSeedPtr = s_fseed[s_cur];
}

/* gmscene.c, after the tick: the fighters ran exactly on recorded frames */
void pc_slp_play_post_tick(uint64_t proc_mask) {
    if (!pc_slp_play_active() || !in_vs() || s_done) {
        return;
    }
    const bool ran = !(proc_mask & (1ULL << HSD_GOBJ_PLINK_FIGHTER));
    if (ran != (s_cur >= 0) && s_bad++ < 16) {
        pc_log_line("slp-play: tick %u: fighters %s but the recording %s", gm_801A4BB8() - 1,
            ran ? "ran" : "held", s_cur >= 0 ? "has a frame" : "has none");
    }
}
