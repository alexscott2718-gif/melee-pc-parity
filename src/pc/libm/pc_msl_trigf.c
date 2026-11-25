/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The game's own sinf / cosf / tanf / atanf, for bit-exact parity with the
 * retail game (local parity fix-02).
 *
 * Sources (public):
 *   - sinf, cosf: doldecomp/melee src/MSL/trigf.c, tables from
 *     src/MSL/math_data.c (__sincos_on_quadrant, __sincos_poly) and
 *     trigf.c's tmp_float (copied into __four_over_pi_m1 by its static
 *     initialiser), at commit 79b13a32467f7c4f91d6f831f0b9f891b7fb1146
 *     (the commit this tree's src/UPSTREAM_COMMIT names);
 *   - tanf: the same commit's src/MSL/math_1.c, sin__Ff(x) / cos__Ff(x);
 *   - atanf: this tree's src/melee/lb/lbtrigf.c, whose atanf only compiles
 *     under __MWERKS__ (on PC the game's atan2f called the musl atanf).
 *
 * The console build compiles these with fused multiply-add: every site the
 * retail DOL computes with one rounding (fmadds / fnmadds / fnmsubs) is an
 * explicit fmaf here, because this port builds with -ffp-contract=off. The
 * sites were read off the retail code (sinf 0x803263D4, cosf 0x80326240,
 * atanf 0x80022E68; addresses from the decomp's config/GALE01/symbols.txt):
 *   sinf/cosf: the four range-reduction terms (y += c[i] * x), the
 *              small-angle return, and every Horner step;
 *   cosf:      the small-angle return is fnmsubs (q[n+1] - y * q[n]); the
 *              last odd-quadrant Horner step is fnmadds (the C's negation
 *              applied after the single rounding);
 *   atanf:     the two __fnmsubs, the five Horner steps, and the final
 *              result * result_sq * poly + result (fmuls, then fmadds).
 * fmaf is correctly rounded (IEEE 754), so the results are the same on
 * every platform, as the netplay determinism rule requires. */
#include <math.h>
#include <stdint.h>
#include <string.h>

static uint32_t f2u(float f)
{
    uint32_t u;
    memcpy(&u, &f, sizeof u);
    return u;
}

static float u2f(uint32_t u)
{
    float f;
    memcpy(&f, &u, sizeof f);
    return f;
}

/* src/MSL/math_data.c */
static const float msl_sincos_on_quadrant[] = { 0, 1, 1, 0, 0, -1, -1, 0 };
static const float msl_sincos_poly[] = {
    0.0000035287617, 0.0000003089747, -0.0003259365, -0.00003657235, 0.015854323,
    0.0024903931,    -0.30842513,     -0.08074551,   1,              0.7853982,
};
/* src/MSL/trigf.c tmp_float -> __four_over_pi_m1 */
static const float msl_four_over_pi_m1[] = { 0.25f, 0.0232393741608f, 1.70555722434e-7f,
                                             1.86736494323e-11f };
#define MSL_EPSILON 3.45266983e-4f

/* the shared range reduction: n (quadrant, & 3) and y */
static float msl_reduce(float x, int* np)
{
    float z = (2.0f / (float)M_PI) * x;
    int n = (f2u(x) & 0x80000000u) ? (int)(z - 0.5f) : (int)(z + 0.5f);
    float y = x - (float)(n * 2);
    y = fmaf(msl_four_over_pi_m1[0], x, y);
    y = fmaf(msl_four_over_pi_m1[1], x, y);
    y = fmaf(msl_four_over_pi_m1[2], x, y);
    y = fmaf(msl_four_over_pi_m1[3], x, y);
    *np = n & 3;
    return y;
}

static float msl_even_poly(float ysq) /* poly[0], [2], [4], [6], [8] */
{
    float z = fmaf(msl_sincos_poly[0], ysq, msl_sincos_poly[2]);
    z = fmaf(ysq, z, msl_sincos_poly[4]);
    z = fmaf(ysq, z, msl_sincos_poly[6]);
    return fmaf(ysq, z, msl_sincos_poly[8]);
}

static float msl_odd_poly3(float ysq) /* poly[1], [3], [5], [7]: all but the last step */
{
    float z = fmaf(msl_sincos_poly[1], ysq, msl_sincos_poly[3]);
    z = fmaf(ysq, z, msl_sincos_poly[5]);
    return fmaf(ysq, z, msl_sincos_poly[7]);
}

float pc_msl_sinf(float x)
{
    int n;
    float y = msl_reduce(x, &n);
    if (fabsf(y) < MSL_EPSILON) {
        n <<= 1;
        return fmaf(msl_sincos_poly[9], msl_sincos_on_quadrant[n + 1] * y,
                    msl_sincos_on_quadrant[n]);
    }
    float ysq = y * y;
    if (n & 1) {
        n <<= 1;
        return msl_even_poly(ysq) * msl_sincos_on_quadrant[n];
    }
    n <<= 1;
    float z = fmaf(ysq, msl_odd_poly3(ysq), msl_sincos_poly[9]);
    return (y * z) * msl_sincos_on_quadrant[n + 1];
}

float pc_msl_cosf(float x)
{
    int n;
    float y = msl_reduce(x, &n);
    if (fabsf(y) < MSL_EPSILON) {
        n <<= 1;
        return fmaf(-y, msl_sincos_on_quadrant[n], msl_sincos_on_quadrant[n + 1]);
    }
    float ysq = y * y;
    if (n & 1) {
        n <<= 1;
        float z = -fmaf(ysq, msl_odd_poly3(ysq), msl_sincos_poly[9]);
        return (y * z) * msl_sincos_on_quadrant[n];
    }
    n <<= 1;
    return msl_even_poly(ysq) * msl_sincos_on_quadrant[n + 1];
}

/* src/MSL/math_1.c */
float pc_msl_tanf(float x)
{
    return pc_msl_sinf(x) / pc_msl_cosf(x);
}

/* src/melee/lb/lbtrigf.c atanf (its table and thresholds) */
static const float lb_atanf_lookup[] = {
    1.0,
    -0.3333333134651184,
    0.1999988704919815,
    -0.14281649887561798,
    0.11041180044412613,
    -0.08459755778312683,
    0.04714243486523628,
    6.828420162200928,
    3.239828109741211,
    2.0,
    1.4464620351791382,
    1.1715729236602783,
    1.039566159248352,
    7.1350000325764995e-06,
    8.200000252145401e-07,
    0.0,
    6.299999881775875e-07,
    0.0,
    0.0,
    0.0,
    0.3926900029182434,
    0.5890486240386963,
    0.7853981256484985,
    0.9817469716072083,
    1.1780970096588135,
    1.3744460344314575,
    0.0,
    9.081698408408556e-06,
    2.3000000126671694e-08,
    6.30000016599297e-08,
    7.040000014058023e-07,
    2.499999993688107e-07,
    7.900000014160469e-07,
    2.414212942123413,
    1.4966057538986206,
    1.0,
    0.6681786179542542,
    0.4142135679721832,
    0.1989123672246933,
    5.620000251838064e-07,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
};

float pc_msl_atanf(float x)
{
    const float silver_ratio = 2.4142136573791504f;
    const float silver_ratio_conjugate = 0.4142135679721832f;
    float result;
    int idx = -1; /* lookup_index; lookup_ptr[k] = lb_atanf_lookup[idx + k] */
    int x_ge_ratio = 0;
    uint32_t sign_bit_x = f2u(x) & 0x80000000u;

    x = u2f(f2u(x) & ~0x80000000u);
    if (x >= silver_ratio) {
        x_ge_ratio = 1;
        result = 1.0f / x;
    } else if (silver_ratio_conjugate < x) {
        int32_t bits = (int32_t)f2u(x);
        idx = 0;
        switch (f2u(x) & 0x7F800000u) {
        case 0x3F000000u: /* [0.5, 1) */
            if (!(bits < 0x3F08D5B9)) {
                idx = 1;
            }
            if (!(bits < 0x3F521801)) {
                idx += 1;
            }
            break;
        case 0x3F800000u: /* [1, 2) */
            idx = 2;
            if (!(bits < 0x3F9BF7EC)) {
                idx = 3;
            }
            if (!(bits < 0x3FEF789E)) {
                idx += 1;
            }
            break;
        case 0x40000000u: /* [2, 4) */
            idx = 4;
            break;
        }
        {
            float offset_39 = lb_atanf_lookup[idx + 39];
            float offset_33 = lb_atanf_lookup[idx + 33];
            result = 1.0f / (offset_33 + (x + offset_39));
            /* __fnmsubs(a, b, c) = c - a * b, one rounding each */
            result = fmaf(-result, lb_atanf_lookup[idx + 7], offset_33) +
                     fmaf(-result, lb_atanf_lookup[idx + 13], offset_39);
        }
    } else {
        result = x;
    }
    {
        float rsq = result * result;
        float p = fmaf(rsq, lb_atanf_lookup[6], lb_atanf_lookup[5]);
        p = fmaf(rsq, p, lb_atanf_lookup[4]);
        p = fmaf(rsq, p, lb_atanf_lookup[3]);
        p = fmaf(rsq, p, lb_atanf_lookup[2]);
        p = fmaf(rsq, p, lb_atanf_lookup[1]);
        result = fmaf(result * rsq, p, result);
        result += lb_atanf_lookup[idx + 27];
        result += lb_atanf_lookup[idx + 20];
    }
    if (x_ge_ratio) {
        result -= (float)M_PI_2;
        return sign_bit_x ? result : -result;
    }
    return u2f(f2u(result) | sign_bit_x);
}
