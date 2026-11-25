#include <assert.h>
#include <dolphin/mtx.h>

/* Local parity fix: the Gekko paired-single PSMTX* routines the game calls
 * fuse their multiply-adds (ps_madd / ps_madds0 / ps_madds1, one rounding)
 * and sum in a fixed order; this mirrors each operation of the SDK assembly
 * (doldecomp/melee libs/dolphin/src/dolphin/mtx/mtxvec.c PSMTXMultVec at 79b13a32) with fmaf,
 * which is correctly rounded on every platform. */
void C_MTXMultVec(const Mtx m, const Vec* src, Vec* dst) {
  Vec vTmp;
  f32 r[3];
  int i;

  assert(m && "MTXMultVec():  NULL MtxPtr 'm' ");
  assert(src && "MTXMultVec():  NULL VecPtr 'src' ");
  assert(dst && "MTXMultVec():  NULL VecPtr 'dst' ");

  for (i = 0; i < 3; i++) {
    /* ps_mul (m0 x, m1 y); ps_madd with (z, 1); ps_sum0 */
    f32 p0 = __builtin_fmaf(m[i][2], src->z, m[i][0] * src->x);
    f32 p1 = __builtin_fmaf(m[i][3], 1.0f, m[i][1] * src->y);
    r[i] = p0 + p1;
  }
  vTmp.x = r[0];
  vTmp.y = r[1];
  vTmp.z = r[2];
  dst->x = vTmp.x;
  dst->y = vTmp.y;
  dst->z = vTmp.z;
}

void C_MTXMultVecArray(const Mtx m, const Vec* srcBase, Vec* dstBase, u32 count) {
  u32 i;
  Vec vTmp;

  assert(m && "MTXMultVecArray():  NULL MtxPtr 'm' ");
  assert(srcBase && "MTXMultVecArray():  NULL VecPtr 'srcBase' ");
  assert(dstBase && "MTXMultVecArray():  NULL VecPtr 'dstBase' ");
  assert(count > 1 && "MTXMultVecArray():  count must be greater than 1.");

  for(i = 0; i < count; i++) {
    vTmp.x = m[0][3] + ((m[0][2] * srcBase->z) + ((m[0][0] * srcBase->x) + (m[0][1] * srcBase->y)));
    vTmp.y = m[1][3] + ((m[1][2] * srcBase->z) + ((m[1][0] * srcBase->x) + (m[1][1] * srcBase->y)));
    vTmp.z = m[2][3] + ((m[2][2] * srcBase->z) + ((m[2][0] * srcBase->x) + (m[2][1] * srcBase->y)));
    dstBase->x = vTmp.x;
    dstBase->y = vTmp.y;
    dstBase->z = vTmp.z;
    srcBase++;
    dstBase++;
  }
}

/* Local parity fix: the Gekko paired-single PSMTX* routines the game calls
 * fuse their multiply-adds (ps_madd / ps_madds0 / ps_madds1, one rounding)
 * and sum in a fixed order; this mirrors each operation of the SDK assembly
 * (doldecomp/melee libs/dolphin/src/dolphin/mtx/mtxvec.c PSMTXMultVecSR at 79b13a32) with fmaf,
 * which is correctly rounded on every platform. */
void C_MTXMultVecSR(const Mtx m, const Vec* src, Vec* dst) {
  Vec vTmp;
  f32 r[3];
  int i;

  assert(m && "MTXMultVecSR():  NULL MtxPtr 'm' ");
  assert(src && "MTXMultVecSR():  NULL VecPtr 'src' ");
  assert(dst && "MTXMultVecSR():  NULL VecPtr 'dst' ");

  for (i = 0; i < 3; i++) {
    /* ps_mul (m0 x, m1 y); ps_sum0; ps_madd m2 z */
    f32 s = m[i][0] * src->x + m[i][1] * src->y;
    r[i] = __builtin_fmaf(m[i][2], src->z, s);
  }
  vTmp.x = r[0];
  vTmp.y = r[1];
  vTmp.z = r[2];
  dst->x = vTmp.x;
  dst->y = vTmp.y;
  dst->z = vTmp.z;
}

void C_MTXMultVecArraySR(const Mtx m, const Vec* srcBase, Vec* dstBase, u32 count) {
  u32 i;
  Vec vTmp;

  assert(m && "MTXMultVecArraySR():  NULL MtxPtr 'm' ");
  assert(srcBase && "MTXMultVecArraySR():  NULL VecPtr 'srcBase' ");
  assert(dstBase && "MTXMultVecArraySR():  NULL VecPtr 'dstBase' ");
  assert(count > 1 && "MTXMultVecArraySR():  count must be greater than 1.");

  for(i = 0; i < count; i++) {
    vTmp.x = (m[0][2] * srcBase->z) + ((m[0][0] * srcBase->x) + (m[0][1] * srcBase->y));
    vTmp.y = (m[1][2] * srcBase->z) + ((m[1][0] * srcBase->x) + (m[1][1] * srcBase->y));
    vTmp.z = (m[2][2] * srcBase->z) + ((m[2][0] * srcBase->x) + (m[2][1] * srcBase->y));
    dstBase->x = vTmp.x;
    dstBase->y = vTmp.y;
    dstBase->z = vTmp.z;
    srcBase++;
    dstBase++;
  }
}

void C_MTXROMultVecArray(const ROMtx m, const Vec *srcBase, Vec *dstBase, u32 count)
{
  u32 i;
  Vec vTmp;

  assert(m && "MTXROMultVecArray():  NULL MtxPtr 'm' ");
  assert(srcBase && "MTXROMultVecArray():  NULL VecPtr 'srcBase' ");
  assert(dstBase && "MTXROMultVecArray():  NULL VecPtr 'dstBase' ");

  for(i = 0; i < count; i++) {
    vTmp.x = (m[0][0] * srcBase->x) + (m[1][0] * srcBase->y) + (m[2][0] * srcBase->z) + m[3][0];
    vTmp.y = (m[0][1] * srcBase->x) + (m[1][1] * srcBase->y) + (m[2][1] * srcBase->z) + m[3][1];
    vTmp.z = (m[0][2] * srcBase->x) + (m[1][2] * srcBase->y) + (m[2][2] * srcBase->z) + m[3][2];
    dstBase->x = vTmp.x;
    dstBase->y = vTmp.y;
    dstBase->z = vTmp.z;
    srcBase++;
    dstBase++;
  }
}
