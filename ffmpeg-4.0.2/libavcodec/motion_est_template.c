/*
 * Motion estimation
 * Copyright (c) 2002-2004 Michael Niedermayer
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * Motion estimation template.
 */

#include "libavutil/qsort.h"
#include "mpegvideo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Let us hope gcc will remove the unused vars ...(gcc 3.2.2 seems to do it ...)
#define LOAD_COMMON                                                            \
  uint32_t av_unused *const score_map = c->score_map;                          \
  const int av_unused xmin = c->xmin;                                          \
  const int av_unused ymin = c->ymin;                                          \
  const int av_unused xmax = c->xmax;                                          \
  const int av_unused ymax = c->ymax;                                          \
  uint8_t *mv_penalty = c->current_mv_penalty;                                 \
  const int pred_x = c->pred_x;                                                \
  const int pred_y = c->pred_y;

#define CHECK_HALF_MV(dx, dy, x, y)                                            \
  {                                                                            \
    const int hx = 2 * (x) + (dx);                                             \
    const int hy = 2 * (y) + (dy);                                             \
    d = cmp_hpel(s, x, y, dx, dy, size, h, ref_index, src_index, cmp_sub,      \
                 chroma_cmp_sub, flags);                                       \
    d += (mv_penalty[hx - pred_x] + mv_penalty[hy - pred_y]) * penalty_factor; \
    COPY3_IF_LT(dmin, d, bx, hx, by, hy)                                       \
  }

static int hpel_motion_search(MpegEncContext *s, int *mx_ptr, int *my_ptr,
                              int dmin, int src_index, int ref_index, int size,
                              int h) {
  MotionEstContext *const c = &s->me;
  const int mx = *mx_ptr;
  const int my = *my_ptr;
  const int penalty_factor = c->sub_penalty_factor;
  me_cmp_func cmp_sub, chroma_cmp_sub;
  int bx = 2 * mx, by = 2 * my;

  LOAD_COMMON
  int flags = c->sub_flags;

  // FIXME factorize

  cmp_sub = s->mecc.me_sub_cmp[size];
  chroma_cmp_sub = s->mecc.me_sub_cmp[size + 1];

  if (c->skip) { // FIXME move out of hpel?
    *mx_ptr = 0;
    *my_ptr = 0;
    return dmin;
  }

  if (c->avctx->me_cmp != c->avctx->me_sub_cmp) {
    dmin = cmp(s, mx, my, 0, 0, size, h, ref_index, src_index, cmp_sub,
               chroma_cmp_sub, flags);
    if (mx || my || size > 0)
      dmin += (mv_penalty[2 * mx - pred_x] + mv_penalty[2 * my - pred_y]) *
              penalty_factor;
  }

  if (mx > xmin && mx < xmax && my > ymin && my < ymax) {
    int d = dmin;
    const int index = (my << ME_MAP_SHIFT) + mx;
    const int t = score_map[(index - (1 << ME_MAP_SHIFT)) & (ME_MAP_SIZE - 1)] +
                  (mv_penalty[bx - pred_x] + mv_penalty[by - 2 - pred_y]) *
                      c->penalty_factor;
    const int l = score_map[(index - 1) & (ME_MAP_SIZE - 1)] +
                  (mv_penalty[bx - 2 - pred_x] + mv_penalty[by - pred_y]) *
                      c->penalty_factor;
    const int r = score_map[(index + 1) & (ME_MAP_SIZE - 1)] +
                  (mv_penalty[bx + 2 - pred_x] + mv_penalty[by - pred_y]) *
                      c->penalty_factor;
    const int b = score_map[(index + (1 << ME_MAP_SHIFT)) & (ME_MAP_SIZE - 1)] +
                  (mv_penalty[bx - pred_x] + mv_penalty[by + 2 - pred_y]) *
                      c->penalty_factor;

#if defined(ASSERT_LEVEL) && ASSERT_LEVEL > 1
    unsigned key;
    unsigned map_generation = c->map_generation;
    key = ((my - 1) << ME_MAP_MV_BITS) + (mx) + map_generation;
    av_assert2(c->map[(index - (1 << ME_MAP_SHIFT)) & (ME_MAP_SIZE - 1)] ==
               key);
    key = ((my + 1) << ME_MAP_MV_BITS) + (mx) + map_generation;
    av_assert2(c->map[(index + (1 << ME_MAP_SHIFT)) & (ME_MAP_SIZE - 1)] ==
               key);
    key = ((my) << ME_MAP_MV_BITS) + (mx + 1) + map_generation;
    av_assert2(c->map[(index + 1) & (ME_MAP_SIZE - 1)] == key);
    key = ((my) << ME_MAP_MV_BITS) + (mx - 1) + map_generation;
    av_assert2(c->map[(index - 1) & (ME_MAP_SIZE - 1)] == key);
#endif
    if (t <= b) {
      CHECK_HALF_MV(0, 1, mx, my - 1)
      if (l <= r) {
        CHECK_HALF_MV(1, 1, mx - 1, my - 1)
        if (t + r <= b + l) {
          CHECK_HALF_MV(1, 1, mx, my - 1)
        } else {
          CHECK_HALF_MV(1, 1, mx - 1, my)
        }
        CHECK_HALF_MV(1, 0, mx - 1, my)
      } else {
        CHECK_HALF_MV(1, 1, mx, my - 1)
        if (t + l <= b + r) {
          CHECK_HALF_MV(1, 1, mx - 1, my - 1)
        } else {
          CHECK_HALF_MV(1, 1, mx, my)
        }
        CHECK_HALF_MV(1, 0, mx, my)
      }
    } else {
      if (l <= r) {
        if (t + l <= b + r) {
          CHECK_HALF_MV(1, 1, mx - 1, my - 1)
        } else {
          CHECK_HALF_MV(1, 1, mx, my)
        }
        CHECK_HALF_MV(1, 0, mx - 1, my)
        CHECK_HALF_MV(1, 1, mx - 1, my)
      } else {
        if (t + r <= b + l) {
          CHECK_HALF_MV(1, 1, mx, my - 1)
        } else {
          CHECK_HALF_MV(1, 1, mx - 1, my)
        }
        CHECK_HALF_MV(1, 0, mx, my)
        CHECK_HALF_MV(1, 1, mx, my)
      }
      CHECK_HALF_MV(0, 1, mx, my)
    }
    av_assert2(bx >= xmin * 2 && bx <= xmax * 2 && by >= ymin * 2 &&
               by <= ymax * 2);
  }

  *mx_ptr = bx;
  *my_ptr = by;

  return dmin;
}

static int no_sub_motion_search(MpegEncContext *s, int *mx_ptr, int *my_ptr,
                                int dmin, int src_index, int ref_index,
                                int size, int h) {
  (*mx_ptr) <<= 1;
  (*my_ptr) <<= 1;
  return dmin;
}

static inline int get_mb_score(MpegEncContext *s, int mx, int my, int src_index,
                               int ref_index, int size, int h, int add_rate) {
  MotionEstContext *const c = &s->me;
  const int penalty_factor = c->mb_penalty_factor;
  const int flags = c->mb_flags;
  const int qpel = flags & FLAG_QPEL;
  const int mask = 1 + 2 * qpel;
  me_cmp_func cmp_sub, chroma_cmp_sub;
  int d;

  LOAD_COMMON

  // FIXME factorize

  cmp_sub = s->mecc.mb_cmp[size];
  chroma_cmp_sub = s->mecc.mb_cmp[size + 1];

  d = cmp(s, mx >> (qpel + 1), my >> (qpel + 1), mx & mask, my & mask, size, h,
          ref_index, src_index, cmp_sub, chroma_cmp_sub, flags);
  // FIXME check cbp before adding penalty for (0,0) vector
  if (add_rate && (mx || my || size > 0))
    d += (mv_penalty[mx - pred_x] + mv_penalty[my - pred_y]) * penalty_factor;

  return d;
}

int ff_get_mb_score(MpegEncContext *s, int mx, int my, int src_index,
                    int ref_index, int size, int h, int add_rate) {
  return get_mb_score(s, mx, my, src_index, ref_index, size, h, add_rate);
}

#define CHECK_QUARTER_MV(dx, dy, x, y)                                         \
  {                                                                            \
    const int hx = 4 * (x) + (dx);                                             \
    const int hy = 4 * (y) + (dy);                                             \
    d = cmp_qpel(s, x, y, dx, dy, size, h, ref_index, src_index, cmpf,         \
                 chroma_cmpf, flags);                                          \
    d += (mv_penalty[hx - pred_x] + mv_penalty[hy - pred_y]) * penalty_factor; \
    COPY3_IF_LT(dmin, d, bx, hx, by, hy)                                       \
  }

static int qpel_motion_search(MpegEncContext *s, int *mx_ptr, int *my_ptr,
                              int dmin, int src_index, int ref_index, int size,
                              int h) {
  MotionEstContext *const c = &s->me;
  const int mx = *mx_ptr;
  const int my = *my_ptr;
  const int penalty_factor = c->sub_penalty_factor;
  const unsigned map_generation = c->map_generation;
  const int subpel_quality = c->avctx->me_subpel_quality;
  uint32_t *map = c->map;
  me_cmp_func cmpf, chroma_cmpf;
  me_cmp_func cmp_sub, chroma_cmp_sub;

  LOAD_COMMON
  int flags = c->sub_flags;

  cmpf = s->mecc.me_cmp[size];
  chroma_cmpf = s->mecc.me_cmp[size + 1]; // FIXME: factorize
                                          // FIXME factorize

  cmp_sub = s->mecc.me_sub_cmp[size];
  chroma_cmp_sub = s->mecc.me_sub_cmp[size + 1];

  if (c->skip) { // FIXME somehow move up (benchmark)
    *mx_ptr = 0;
    *my_ptr = 0;
    return dmin;
  }

  if (c->avctx->me_cmp != c->avctx->me_sub_cmp) {
    dmin = cmp(s, mx, my, 0, 0, size, h, ref_index, src_index, cmp_sub,
               chroma_cmp_sub, flags);
    if (mx || my || size > 0)
      dmin += (mv_penalty[4 * mx - pred_x] + mv_penalty[4 * my - pred_y]) *
              penalty_factor;
  }

  if (mx > xmin && mx < xmax && my > ymin && my < ymax) {
    int bx = 4 * mx, by = 4 * my;
    int d = dmin;
    int i, nx, ny;
    const int index = (my << ME_MAP_SHIFT) + mx;
    const int t = score_map[(index - (1 << ME_MAP_SHIFT)) & (ME_MAP_SIZE - 1)];
    const int l = score_map[(index - 1) & (ME_MAP_SIZE - 1)];
    const int r = score_map[(index + 1) & (ME_MAP_SIZE - 1)];
    const int b = score_map[(index + (1 << ME_MAP_SHIFT)) & (ME_MAP_SIZE - 1)];
    const int c = score_map[(index) & (ME_MAP_SIZE - 1)];
    int best[8];
    int best_pos[8][2];

    memset(best, 64, sizeof(int) * 8);
    if (s->me.dia_size >= 2) {
      const int tl =
          score_map[(index - (1 << ME_MAP_SHIFT) - 1) & (ME_MAP_SIZE - 1)];
      const int bl =
          score_map[(index + (1 << ME_MAP_SHIFT) - 1) & (ME_MAP_SIZE - 1)];
      const int tr =
          score_map[(index - (1 << ME_MAP_SHIFT) + 1) & (ME_MAP_SIZE - 1)];
      const int br =
          score_map[(index + (1 << ME_MAP_SHIFT) + 1) & (ME_MAP_SIZE - 1)];

      for (ny = -3; ny <= 3; ny++) {
        for (nx = -3; nx <= 3; nx++) {
          // FIXME this could overflow (unlikely though)
          const int64_t t2 =
              nx * nx * (tr + tl - 2 * t) + 4 * nx * (tr - tl) + 32 * t;
          const int64_t c2 =
              nx * nx * (r + l - 2 * c) + 4 * nx * (r - l) + 32 * c;
          const int64_t b2 =
              nx * nx * (br + bl - 2 * b) + 4 * nx * (br - bl) + 32 * b;
          int score = (ny * ny * (b2 + t2 - 2 * c2) + 4 * ny * (b2 - t2) +
                       32 * c2 + 512) >>
                      10;
          int i;

          if ((nx & 3) == 0 && (ny & 3) == 0)
            continue;

          score += (mv_penalty[4 * mx + nx - pred_x] +
                    mv_penalty[4 * my + ny - pred_y]) *
                   penalty_factor;

          //                    if(nx&1) score-=1024*c->penalty_factor;
          //                    if(ny&1) score-=1024*c->penalty_factor;

          for (i = 0; i < 8; i++) {
            if (score < best[i]) {
              memmove(&best[i + 1], &best[i], sizeof(int) * (7 - i));
              memmove(&best_pos[i + 1][0], &best_pos[i][0],
                      sizeof(int) * 2 * (7 - i));
              best[i] = score;
              best_pos[i][0] = nx + 4 * mx;
              best_pos[i][1] = ny + 4 * my;
              break;
            }
          }
        }
      }
    } else {
      int tl;
      // FIXME this could overflow (unlikely though)
      const int cx = 4 * (r - l);
      const int cx2 = r + l - 2 * c;
      const int cy = 4 * (b - t);
      const int cy2 = b + t - 2 * c;
      int cxy;

      if (map[(index - (1 << ME_MAP_SHIFT) - 1) & (ME_MAP_SIZE - 1)] ==
          ((my - 1) << ME_MAP_MV_BITS) + (mx - 1) + map_generation) {
        tl = score_map[(index - (1 << ME_MAP_SHIFT) - 1) & (ME_MAP_SIZE - 1)];
      } else {
        tl = cmp(s, mx - 1, my - 1, 0, 0, size, h, ref_index, src_index, cmpf,
                 chroma_cmpf, flags); // FIXME wrong if chroma me is different
      }

      cxy = 2 * tl + (cx + cy) / 4 - (cx2 + cy2) - 2 * c;

      av_assert2(16 * cx2 + 4 * cx + 32 * c == 32 * r);
      av_assert2(16 * cx2 - 4 * cx + 32 * c == 32 * l);
      av_assert2(16 * cy2 + 4 * cy + 32 * c == 32 * b);
      av_assert2(16 * cy2 - 4 * cy + 32 * c == 32 * t);
      av_assert2(16 * cxy + 16 * cy2 + 16 * cx2 - 4 * cy - 4 * cx + 32 * c ==
                 32 * tl);

      for (ny = -3; ny <= 3; ny++) {
        for (nx = -3; nx <= 3; nx++) {
          // FIXME this could overflow (unlikely though)
          int score = ny * nx * cxy + nx * nx * cx2 + ny * ny * cy2 + nx * cx +
                      ny * cy + 32 * c; // FIXME factor
          int i;

          if ((nx & 3) == 0 && (ny & 3) == 0)
            continue;

          score += 32 *
                   (mv_penalty[4 * mx + nx - pred_x] +
                    mv_penalty[4 * my + ny - pred_y]) *
                   penalty_factor;
          //                    if(nx&1) score-=32*c->penalty_factor;
          //                  if(ny&1) score-=32*c->penalty_factor;

          for (i = 0; i < 8; i++) {
            if (score < best[i]) {
              memmove(&best[i + 1], &best[i], sizeof(int) * (7 - i));
              memmove(&best_pos[i + 1][0], &best_pos[i][0],
                      sizeof(int) * 2 * (7 - i));
              best[i] = score;
              best_pos[i][0] = nx + 4 * mx;
              best_pos[i][1] = ny + 4 * my;
              break;
            }
          }
        }
      }
    }
    for (i = 0; i < subpel_quality; i++) {
      nx = best_pos[i][0];
      ny = best_pos[i][1];
      CHECK_QUARTER_MV(nx & 3, ny & 3, nx >> 2, ny >> 2)
    }

    av_assert2(bx >= xmin * 4 && bx <= xmax * 4 && by >= ymin * 4 &&
               by <= ymax * 4);

    *mx_ptr = bx;
    *my_ptr = by;
  } else {
    *mx_ptr = 4 * mx;
    *my_ptr = 4 * my;
  }

  return dmin;
}

#define CHECK_MV(x, y)                                                         \
  {                                                                            \
    const unsigned key =                                                       \
        ((unsigned)(y) << ME_MAP_MV_BITS) + (x) + map_generation;              \
    const int index =                                                          \
        (((unsigned)(y) << ME_MAP_SHIFT) + (x)) & (ME_MAP_SIZE - 1);           \
    av_assert2((x) >= xmin);                                                   \
    av_assert2((x) <= xmax);                                                   \
    av_assert2((y) >= ymin);                                                   \
    av_assert2((y) <= ymax);                                                   \
    if (map[index] != key) {                                                   \
      d = cmp(s, x, y, 0, 0, size, h, ref_index, src_index, cmpf, chroma_cmpf, \
              flags);                                                          \
      map[index] = key;                                                        \
      score_map[index] = d;                                                    \
      d += (mv_penalty[((x) * (1 << shift)) - pred_x] +                        \
            mv_penalty[((y) * (1 << shift)) - pred_y]) *                       \
           penalty_factor;                                                     \
      COPY3_IF_LT(dmin, d, best[0], x, best[1], y)                             \
    }                                                                          \
  }

#define CHECK_CLIPPED_MV(ax, ay)                                               \
  {                                                                            \
    const int Lx = ax;                                                         \
    const int Ly = ay;                                                         \
    const int Lx2 = FFMAX(xmin, FFMIN(Lx, xmax));                              \
    const int Ly2 = FFMAX(ymin, FFMIN(Ly, ymax));                              \
    CHECK_MV(Lx2, Ly2)                                                         \
  }

#define CHECK_MV_DIR(x, y, new_dir)                                            \
  {                                                                            \
    const unsigned key =                                                       \
        ((unsigned)(y) << ME_MAP_MV_BITS) + (x) + map_generation;              \
    const int index =                                                          \
        (((unsigned)(y) << ME_MAP_SHIFT) + (x)) & (ME_MAP_SIZE - 1);           \
    if (map[index] != key) {                                                   \
      d = cmp(s, x, y, 0, 0, size, h, ref_index, src_index, cmpf, chroma_cmpf, \
              flags);                                                          \
      map[index] = key;                                                        \
      score_map[index] = d;                                                    \
      d += (mv_penalty[(int)((unsigned)(x) << shift) - pred_x] +               \
            mv_penalty[(int)((unsigned)(y) << shift) - pred_y]) *              \
           penalty_factor;                                                     \
      if (d < dmin) {                                                          \
        best[0] = x;                                                           \
        best[1] = y;                                                           \
        dmin = d;                                                              \
        next_dir = new_dir;                                                    \
      }                                                                        \
    }                                                                          \
  }

#define check(x, y, S, v)                                                      \
  if ((x) < (xmin << (S)))                                                     \
    av_log(NULL, AV_LOG_ERROR, "%d %d %d %d %d xmin" #v, xmin, (x), (y),       \
           s->mb_x, s->mb_y);                                                  \
  if ((x) > (xmax << (S)))                                                     \
    av_log(NULL, AV_LOG_ERROR, "%d %d %d %d %d xmax" #v, xmax, (x), (y),       \
           s->mb_x, s->mb_y);                                                  \
  if ((y) < (ymin << (S)))                                                     \
    av_log(NULL, AV_LOG_ERROR, "%d %d %d %d %d ymin" #v, ymin, (x), (y),       \
           s->mb_x, s->mb_y);                                                  \
  if ((y) > (ymax << (S)))                                                     \
    av_log(NULL, AV_LOG_ERROR, "%d %d %d %d %d ymax" #v, ymax, (x), (y),       \
           s->mb_x, s->mb_y);

#define LOAD_COMMON2                                                           \
  uint32_t *map = c->map;                                                      \
  const int qpel = flags & FLAG_QPEL;                                          \
  const int shift = 1 + qpel;

static av_always_inline int small_diamond_search(MpegEncContext *s, int *best,
                                                 int dmin, int src_index,
                                                 int ref_index,
                                                 const int penalty_factor,
                                                 int size, int h, int flags) {
  MotionEstContext *const c = &s->me;
  me_cmp_func cmpf, chroma_cmpf;
  int next_dir = -1;
  LOAD_COMMON
  LOAD_COMMON2
  unsigned map_generation = c->map_generation;

  // FILE *DJ;
  // DJ = fopen("DiamondInformation.txt", "a");
  cmpf = s->mecc.me_cmp[size];
  chroma_cmpf = s->mecc.me_cmp[size + 1];

  // original stuff
  { // ensure that the best point is in the MAP as h/qpel refinement needs it
    const unsigned key =
        ((unsigned)best[1] << ME_MAP_MV_BITS) + best[0] + map_generation;
    const int index =
        (((unsigned)best[1] << ME_MAP_SHIFT) + best[0]) & (ME_MAP_SIZE - 1);

    if (map[index] != key) { // this will be executed only very rarely
      score_map[index] = cmp(s, best[0], best[1], 0, 0, size, h, ref_index,
                             src_index, cmpf, chroma_cmpf, flags);
      // this does the actual calculation^
      map[index] = key;
    }
  }
  for (;;) {
    int d;
    const int dir = next_dir;
    const int x = best[0];
    const int y = best[1];
    next_dir = -1;

    if (dir != 2 && x > xmin)
      CHECK_MV_DIR(x - 1, y, 0)
    if (dir != 3 && y > ymin)
      CHECK_MV_DIR(x, y - 1, 1)
    if (dir != 0 && x < xmax)
      CHECK_MV_DIR(x + 1, y, 2)
    if (dir != 1 && y < ymax)
      CHECK_MV_DIR(x, y + 1, 3)

    if (next_dir == -1) {
      // fprintf(DJ, "%d %d %d %d %d %d %d \n", s->coded_picture_number,
      // s->mb_x,
      //         s->mb_y, best[0], best[1], src_index, ref_index);
      // fclose(DJ);
      return dmin;
    }
  }
  return dmin;
}

static av_always_inline int zero_search(MpegEncContext *s, int *best, int dmin,
                                        int src_index, int ref_index,
                                        const int penalty_factor, int size,
                                        int h, int flags) {
  MotionEstContext *const c = &s->me;
  me_cmp_func cmpf, chroma_cmpf;
  int next_dir = -1;
  LOAD_COMMON
  LOAD_COMMON2
  unsigned map_generation = c->map_generation;
  best[0] = 0;
  best[1] = 0;

  cmpf = s->mecc.me_cmp[size];
  chroma_cmpf = s->mecc.me_cmp[size + 1];

  // original stuff
  { // ensure that the best point is in the MAP as h/qpel refinement needs it
    const unsigned key =
        ((unsigned)best[1] << ME_MAP_MV_BITS) + best[0] + map_generation;
    const int index =
        (((unsigned)best[1] << ME_MAP_SHIFT) + best[0]) & (ME_MAP_SIZE - 1);

    if (map[index] != key) { // this will be executed only very rarely
      score_map[index] = cmp(s, best[0], best[1], 0, 0, size, h, ref_index,
                             src_index, cmpf, chroma_cmpf, flags);
      // this does the actual calculation^
      map[index] = key;
    }
  }

  int d;
  const int dir = next_dir;
  const int x = best[0];
  const int y = best[1];
  next_dir = -1;

  CHECK_MV(x, y)

  return dmin;
}

static int funny_diamond_search(MpegEncContext *s, int *best, int dmin,
                                int src_index, int ref_index,
                                const int penalty_factor, int size, int h,
                                int flags) {
  MotionEstContext *const c = &s->me;
  me_cmp_func cmpf, chroma_cmpf;
  int dia_size;
  LOAD_COMMON
  LOAD_COMMON2
  unsigned map_generation = c->map_generation;

  cmpf = s->mecc.me_cmp[size];
  chroma_cmpf = s->mecc.me_cmp[size + 1];

  for (dia_size = 1; dia_size <= 4; dia_size++) {
    int dir;
    const int x = best[0];
    const int y = best[1];

    if (dia_size & (dia_size - 1))
      continue;

    if (x + dia_size > xmax || x - dia_size < xmin || y + dia_size > ymax ||
        y - dia_size < ymin)
      continue;

    for (dir = 0; dir < dia_size; dir += 2) {
      int d;

      CHECK_MV(x + dir, y + dia_size - dir);
      CHECK_MV(x + dia_size - dir, y - dir);
      CHECK_MV(x - dir, y - dia_size + dir);
      CHECK_MV(x - dia_size + dir, y + dir);
    }

    if (x != best[0] || y != best[1])
      dia_size = 0;
  }
  return dmin;
}

static int hex_search(MpegEncContext *s, int *best, int dmin, int src_index,
                      int ref_index, const int penalty_factor, int size, int h,
                      int flags, int dia_size) {
  MotionEstContext *const c = &s->me;
  me_cmp_func cmpf, chroma_cmpf;
  LOAD_COMMON
  LOAD_COMMON2
  unsigned map_generation = c->map_generation;
  int x, y, d;
  //  FILE *DJ;
  //       DJ=fopen("/home/dj/experiments/SintelResultsHex.bin","ab"); //rename
  //       motion results
  const int dec = dia_size & (dia_size - 1);

  cmpf = s->mecc.me_cmp[size];
  chroma_cmpf = s->mecc.me_cmp[size + 1];

  for (; dia_size; dia_size = dec ? dia_size - 1 : dia_size >> 1) {
    do {
      x = best[0];
      y = best[1];

      CHECK_CLIPPED_MV(x - dia_size, y);
      CHECK_CLIPPED_MV(x + dia_size, y);
      CHECK_CLIPPED_MV(x + (dia_size >> 1), y + dia_size);
      CHECK_CLIPPED_MV(x + (dia_size >> 1), y - dia_size);
      if (dia_size > 1) {
        CHECK_CLIPPED_MV(x + (-dia_size >> 1), y + dia_size);
        CHECK_CLIPPED_MV(x + (-dia_size >> 1), y - dia_size);
      }
    } while (best[0] != x || best[1] != y);
  }
  // y	printf("%d %d %d %d %d %d \n", s->coded_picture_number, s->mb_x,
  // s->mb_y, best[0], best[1], dmin);
  //   fprintf(DJ,"%d %d %d %d %d %d \n", s->coded_picture_number, s->mb_x,
  //   s->mb_y, best[0], best[1], dmin); fclose(DJ);

  return dmin;
}

static int l2s_dia_search(MpegEncContext *s, int *best, int dmin, int src_index,
                          int ref_index, const int penalty_factor, int size,
                          int h, int flags) {
  MotionEstContext *const c = &s->me;
  me_cmp_func cmpf, chroma_cmpf;
  LOAD_COMMON
  LOAD_COMMON2
  unsigned map_generation = c->map_generation;
  int x, y, i, d;
  int dia_size = c->dia_size & 0xFF;
  const int dec = dia_size & (dia_size - 1);
  static const int hex[8][2] = {{-2, 0}, {-1, -1}, {0, -2}, {1, -1},
                                {2, 0},  {1, 1},   {0, 2},  {-1, 1}};

  cmpf = s->mecc.me_cmp[size];
  chroma_cmpf = s->mecc.me_cmp[size + 1];

  for (; dia_size; dia_size = dec ? dia_size - 1 : dia_size >> 1) {
    do {
      x = best[0];
      y = best[1];
      for (i = 0; i < 8; i++) {
        CHECK_CLIPPED_MV(x + hex[i][0] * dia_size, y + hex[i][1] * dia_size);
      }
    } while (best[0] != x || best[1] != y);
  }

  x = best[0];
  y = best[1];
  CHECK_CLIPPED_MV(x + 1, y);
  CHECK_CLIPPED_MV(x, y + 1);
  CHECK_CLIPPED_MV(x - 1, y);
  CHECK_CLIPPED_MV(x, y - 1);

  return dmin;
}

static int umh_search(MpegEncContext *s, int *best, int dmin, int src_index,
                      int ref_index, const int penalty_factor, int size, int h,
                      int flags) {
  MotionEstContext *const c = &s->me;
  me_cmp_func cmpf, chroma_cmpf;
  LOAD_COMMON
  LOAD_COMMON2
  unsigned map_generation = c->map_generation;
  int x, y, x2, y2, i, j, d;
  const int dia_size = c->dia_size & 0xFE;
  static const int hex[16][2] = {
      {-4, -2}, {-4, -1}, {-4, 0}, {-4, 1}, {-4, 2}, {4, -2},  {4, -1}, {4, 0},
      {4, 1},   {4, 2},   {-2, 3}, {0, 4},  {2, 3},  {-2, -3}, {0, -4}, {2, -3},
  };

  cmpf = s->mecc.me_cmp[size];
  chroma_cmpf = s->mecc.me_cmp[size + 1];

  x = best[0];
  y = best[1];
  for (x2 = FFMAX(x - dia_size + 1, xmin); x2 <= FFMIN(x + dia_size - 1, xmax);
       x2 += 2) {
    CHECK_MV(x2, y);
  }
  for (y2 = FFMAX(y - dia_size / 2 + 1, ymin);
       y2 <= FFMIN(y + dia_size / 2 - 1, ymax); y2 += 2) {
    CHECK_MV(x, y2);
  }

  x = best[0];
  y = best[1];
  for (y2 = FFMAX(y - 2, ymin); y2 <= FFMIN(y + 2, ymax); y2++) {
    for (x2 = FFMAX(x - 2, xmin); x2 <= FFMIN(x + 2, xmax); x2++) {
      CHECK_MV(x2, y2);
    }
  }

  // FIXME prevent the CLIP stuff

  for (j = 1; j <= dia_size / 4; j++) {
    for (i = 0; i < 16; i++) {
      CHECK_CLIPPED_MV(x + hex[i][0] * j, y + hex[i][1] * j);
    }
  }

  return hex_search(s, best, dmin, src_index, ref_index, penalty_factor, size,
                    h, flags, 2);
}

static int full_search(MpegEncContext *s, int *best, int dmin, int src_index,
                       int ref_index, const int penalty_factor, int size, int h,
                       int flags) {
  MotionEstContext *const c = &s->me;
  me_cmp_func cmpf, chroma_cmpf;

  LOAD_COMMON LOAD_COMMON2 unsigned map_generation = c->map_generation;
  int x, y, d;
  const int dia_size = c->dia_size & 0xFF;

  cmpf = s->mecc.me_cmp[size];
  chroma_cmpf = s->mecc.me_cmp[size + 1];

  for (y = FFMAX(-dia_size, ymin); y <= FFMIN(dia_size, ymax); y++) {
    for (x = FFMAX(-dia_size, xmin); x <= FFMIN(dia_size, xmax); x++) {
      CHECK_MV(x, y);
    }
  }

  x = best[0];
  y = best[1];
  d = dmin;
  CHECK_CLIPPED_MV(x, y);
  CHECK_CLIPPED_MV(x + 1, y);
  CHECK_CLIPPED_MV(x, y + 1);
  CHECK_CLIPPED_MV(x - 1, y);
  CHECK_CLIPPED_MV(x, y - 1);
  best[0] = x;
  best[1] = y;

  return dmin;
}

#define SAB_CHECK_MV(ax, ay)                                                   \
  {                                                                            \
    const unsigned key = ((ay) << ME_MAP_MV_BITS) + (ax) + map_generation;     \
    const int index = (((ay) << ME_MAP_SHIFT) + (ax)) & (ME_MAP_SIZE - 1);     \
    if (map[index] != key) {                                                   \
      d = cmp(s, ax, ay, 0, 0, size, h, ref_index, src_index, cmpf,            \
              chroma_cmpf, flags);                                             \
      map[index] = key;                                                        \
      score_map[index] = d;                                                    \
      d += (mv_penalty[((ax) << shift) - pred_x] +                             \
            mv_penalty[((ay) << shift) - pred_y]) *                            \
           penalty_factor;                                                     \
      if (d < minima[minima_count - 1].height) {                               \
        int j = 0;                                                             \
                                                                               \
        while (d >= minima[j].height)                                          \
          j++;                                                                 \
                                                                               \
        memmove(&minima[j + 1], &minima[j],                                    \
                (minima_count - j - 1) * sizeof(Minima));                      \
                                                                               \
        minima[j].checked = 0;                                                 \
        minima[j].height = d;                                                  \
        minima[j].x = ax;                                                      \
        minima[j].y = ay;                                                      \
                                                                               \
        i = -1;                                                                \
        continue;                                                              \
      }                                                                        \
    }                                                                          \
  }

#define MAX_SAB_SIZE ME_MAP_SIZE
static int sab_diamond_search(MpegEncContext *s, int *best, int dmin,
                              int src_index, int ref_index,
                              const int penalty_factor, int size, int h,
                              int flags) {
  MotionEstContext *const c = &s->me;
  me_cmp_func cmpf, chroma_cmpf;
  Minima minima[MAX_SAB_SIZE];
  const int minima_count = FFABS(c->dia_size);
  int i, j;
  LOAD_COMMON
  LOAD_COMMON2
  unsigned map_generation = c->map_generation;

  av_assert1(minima_count <= MAX_SAB_SIZE);

  cmpf = s->mecc.me_cmp[size];
  chroma_cmpf = s->mecc.me_cmp[size + 1];

  /*Note j<MAX_SAB_SIZE is needed if MAX_SAB_SIZE < ME_MAP_SIZE as j can
    become larger due to MVs overflowing their ME_MAP_MV_BITS bits space in map
   */
  for (j = i = 0; i < ME_MAP_SIZE && j < MAX_SAB_SIZE; i++) {
    uint32_t key = map[i];

    key += (1 << (ME_MAP_MV_BITS - 1)) + (1 << (2 * ME_MAP_MV_BITS - 1));

    if ((key & (-(1 << (2 * ME_MAP_MV_BITS)))) != map_generation)
      continue;

    minima[j].height = score_map[i];
    minima[j].x = key & ((1 << ME_MAP_MV_BITS) - 1);
    key >>= ME_MAP_MV_BITS;
    minima[j].y = key & ((1 << ME_MAP_MV_BITS) - 1);
    minima[j].x -= (1 << (ME_MAP_MV_BITS - 1));
    minima[j].y -= (1 << (ME_MAP_MV_BITS - 1));

    // all entries in map should be in range except if the mv overflows their
    // ME_MAP_MV_BITS bits space
    if (minima[j].x > xmax || minima[j].x < xmin || minima[j].y > ymax ||
        minima[j].y < ymin)
      continue;

    minima[j].checked = 0;
    if (minima[j].x || minima[j].y)
      minima[j].height += (mv_penalty[((minima[j].x) << shift) - pred_x] +
                           mv_penalty[((minima[j].y) << shift) - pred_y]) *
                          penalty_factor;

    j++;
  }

  AV_QSORT(minima, j, Minima, minima_cmp);

  for (; j < minima_count; j++) {
    minima[j].height = 256 * 256 * 256 * 64;
    minima[j].checked = 0;
    minima[j].x = minima[j].y = 0;
  }

  for (i = 0; i < minima_count; i++) {
    const int x = minima[i].x;
    const int y = minima[i].y;
    int d;

    if (minima[i].checked)
      continue;

    if (x >= xmax || x <= xmin || y >= ymax || y <= ymin)
      continue;

    SAB_CHECK_MV(x - 1, y)
    SAB_CHECK_MV(x + 1, y)
    SAB_CHECK_MV(x, y - 1)
    SAB_CHECK_MV(x, y + 1)

    minima[i].checked = 1;
  }

  best[0] = minima[0].x;
  best[1] = minima[0].y;
  dmin = minima[0].height;

  if (best[0] < xmax && best[0] > xmin && best[1] < ymax && best[1] > ymin) {
    int d;
    // ensure that the reference samples for hpel refinement are in the map
    CHECK_MV(best[0] - 1, best[1])
    CHECK_MV(best[0] + 1, best[1])
    CHECK_MV(best[0], best[1] - 1)
    CHECK_MV(best[0], best[1] + 1)
  }
  return dmin;
}

static int var_diamond_search(MpegEncContext *s, int *best, int dmin,
                              int src_index, int ref_index,
                              const int penalty_factor, int size, int h,
                              int flags) {
  MotionEstContext *const c = &s->me;
  me_cmp_func cmpf, chroma_cmpf;
  int dia_size;
  LOAD_COMMON
  LOAD_COMMON2
  unsigned map_generation = c->map_generation;

  cmpf = s->mecc.me_cmp[size];
  chroma_cmpf = s->mecc.me_cmp[size + 1];

  for (dia_size = 1; dia_size <= c->dia_size; dia_size++) {
    int dir, start, end;
    const int x = best[0];
    const int y = best[1];

    start = FFMAX(0, y + dia_size - ymax);
    end = FFMIN(dia_size, xmax - x + 1);
    for (dir = start; dir < end; dir++) {
      int d;

      // check(x + dir,y + dia_size - dir,0, a0)
      CHECK_MV(x + dir, y + dia_size - dir);
    }

    start = FFMAX(0, x + dia_size - xmax);
    end = FFMIN(dia_size, y - ymin + 1);
    for (dir = start; dir < end; dir++) {
      int d;

      // check(x + dia_size - dir, y - dir,0, a1)
      CHECK_MV(x + dia_size - dir, y - dir);
    }

    start = FFMAX(0, -y + dia_size + ymin);
    end = FFMIN(dia_size, x - xmin + 1);
    for (dir = start; dir < end; dir++) {
      int d;

      // check(x - dir,y - dia_size + dir,0, a2)
      CHECK_MV(x - dir, y - dia_size + dir);
    }

    start = FFMAX(0, -x + dia_size + xmin);
    end = FFMIN(dia_size, ymax - y + 1);
    for (dir = start; dir < end; dir++) {
      int d;

      // check(x - dia_size + dir, y + dir,0, a3)
      CHECK_MV(x - dia_size + dir, y + dir);
    }

    if (x != best[0] || y != best[1])
      dia_size = 0;
  }
  return dmin;
}
int truthCount;
int diaCount;
int equalCount;
truthCount = 0;
diaCount = 0;
equalCount = 0;

static int frame_num = 0;
static float flo_mvs[4320][7680][2] = {0};

#define UNKNOWN_FLOW_THRESH                                                    \
  1e9 // flow component is greater, it's considered unknown
#define TAG_FLOAT 202021.25
static const char *flow_files_dir =
    "/home/dj/temp_flo/frame_%04d_%04d.flo"; //_%04d_%04d.flo
static void read_flow_file(const char *filename) {

  FILE *stream = NULL;
  const int n_bands = 2;
  int width, height;
  float tag;

  if (filename == NULL) {
    fprintf(stderr, "read_flow_file: empty filename\n");
    goto read_flow_file_fail;
  }

  char *dot = strrchr(filename, '.');
  if (strcmp(dot, ".flo") != 0) {
    fprintf(stderr, "read_flow_file (%s): extension .flo expected\n", filename);
    goto read_flow_file_fail;
  }

  if ((stream = fopen(filename, "rb")) == NULL) {
    fprintf(stderr, "read_flow_file: could not open %s\n", filename);
    goto read_flow_file_fail;
  }

  if ((int)fread(&tag, sizeof(float), 1, stream) != 1 ||
      (int)fread(&width, sizeof(int), 1, stream) != 1 ||
      (int)fread(&height, sizeof(int), 1, stream) != 1) {
    fprintf(stderr, "read_flow_file: problem reading file %s\n", filename);
    goto read_flow_file_fail;
  }

  if (tag != TAG_FLOAT) { // simple test for correct endian-ness
    fprintf(
        stderr,
        "read_flow_file(%s): wrong tag (possibly due to big-endian machine?)\n",
        filename);
    goto read_flow_file_fail;
  }

  // another sanity check to see that integers were read correctly (99999 should
  // do the trick...)
  if (width < 1 || width > 99999) {
    fprintf(stderr, "read_flow_file(%s): illegal width %d\n", filename, width);
    goto read_flow_file_fail;
  }

  if (height < 1 || height > 99999) {
    fprintf(stderr, "read_flow_file(%s): illegal height %d\n", filename,
            height);
    goto read_flow_file_fail;
  }

  // printf("reading %d x %d x 2 = %d floats\n", width, height, width * height *
  // n_bands);
  int n = n_bands * width;
  for (int y = 0; y < height; y++) {
    float *ptr = &flo_mvs[y][0][0];
    if ((int)fread(ptr, sizeof(float), n, stream) != n) {
      fprintf(stderr, "read_flow_file(%s): file is too short\n", filename);
      goto read_flow_file_fail;
    }
  }

  if (fgetc(stream) != EOF) {
    fprintf(stderr, "read_flow_file(%s): file is too long\n", filename);
    goto read_flow_file_fail;
  }

  fclose(stream);
  return;

read_flow_file_fail:

  if (stream)
    fclose(stream);

  assert(0);
}

static int fill_gt_mvs(int i_frame_num, int ref_index) { //,int picturetype
  if (i_frame_num == 1)
    return 0;

  if (frame_num != i_frame_num) {

    char filename[100];

    sprintf(filename, flow_files_dir, i_frame_num - 1,
            i_frame_num - ref_index); // note the offset

    read_flow_file(filename);

    frame_num = i_frame_num;
  }

  return 1;
}

static int unknown_flow(float u, float v) {
  return fabs(u) > UNKNOWN_FLOW_THRESH || fabs(v) > UNKNOWN_FLOW_THRESH ||
         isnan(u) || isnan(v);
}

#define TEMP_DIR_FRAME_BIN_PNGS "/home/dj/";
static void decoded_frame(MpegEncContext *s, int ref_index) {
  // take the most recent image and save as png

  const char *frame_enc_bin = TEMP_DIR_FRAME_BIN_PNGS "frame_enc.bin";
  FILE *w_frame = fopen(frame_enc_bin, "w");
  // assert (w_frame);
  // assert (sizeof(pixel) == sizeof(uint8_t)); //assert it's not 10bit video
  // fwrite(h->mb.pic.p_fenc_plane[0], s->mb_height * mb_stride,
  // sizeof(uint8_t), w_frame);
  fclose(w_frame);

  // call deepflo with decoded image and the target frame (do with P first)

  // generate flow file
}
static int flo_search(MpegEncContext *s, int *best, int dmin, int src_index,
                      int ref_index, const int penalty_factor, int size, int h,
                      int flags) {
  MotionEstContext *const c = &s->me;
  me_cmp_func cmpf, chroma_cmpf;
  cmpf = s->mecc.me_cmp[size];
  chroma_cmpf = s->mecc.me_cmp[size + 1];
  LOAD_COMMON
  LOAD_COMMON2

  unsigned map_generation = c->map_generation;

  { /* ensure that the best point is in the MAP as h/qpel refinement needs it */
    const int key = (best[1] << ME_MAP_MV_BITS) + best[0] + map_generation;
    const int index = ((best[1] << ME_MAP_SHIFT) + best[0]) & (ME_MAP_SIZE - 1);
    if (map[index] != key) { // this will be executed only very rarey
      score_map[index] = cmp(s, best[0], best[1], 0, 0, size, h, ref_index,
                             src_index, cmpf, chroma_cmpf, flags);
      map[index] = key;
    }
  }

  // condensing method goes here
  //	int BorP;
  //	if (s->pict_type = AV_PICTURE_TYPE_B)
  //		BorP = 1;
  //	else if (s->pict_type = AV_PICTURE_TYPE_P)
  //		BorP = 2;
  // put decoded frame call here-----------------------------------------------
  // decoded_frame(s,ref_index);

  fill_gt_mvs(s->coded_picture_number, ref_index);
  int y;
  y = s->mb_y;
  int x;
  x = s->mb_x;
  int bestx;
  int besty;
  int d;
  int k;
  k = 0;
  int dmintemp;
  dmintemp = UNKNOWN_FLOW_THRESH;
  for (int j = (y * 16); j < (y + 16); j++)
    for (int i = (x * 16); i < (x + 16); i++) {
      if (unknown_flow(flo_mvs[j][i][0], flo_mvs[j][i][1])) {
        printf("unknown flow\n");
        assert(0);
      }

      int mv_x;
      mv_x = round(flo_mvs[j][i][0]);
      int mv_y;
      mv_y = round(flo_mvs[j][i][1]);
      k++;
      CHECK_MV(mv_x, mv_y)
      if (dmin < dmintemp) {
        // 	printf("bestmv updated \n");
        dmintemp = dmin;
        bestx = mv_x;
        besty = mv_y;
      }
      k++;
    }

  return dmin;
}

static int my_search(MpegEncContext *s, int *best, int dmin, int src_index,
                     int ref_index, const int penalty_factor, int size, int h,
                     int flags) {

  FILE *DJ2;
  DJ2 = fopen("/home/dj/temp_flo/MBInfo.txt", "a");
  int dTru;
  int dDia;
  dTru = dmin;
  dDia = dmin;
  int *bestTempTru;
  int *bestTempDia;
  int bestxStart;
  int bestyStart;
  bestxStart = best[0];
  bestyStart = best[1];
  bestTempTru = best;
  bestTempDia = best;
  //	printf ("start best %d %d %d %d \n", bestTemp1[0],
  // bestTemp1[1],bestTemp2[0],bestTemp2[1]);
  int penalty_factor_tempTru;
  int penalty_factor_tempDia;
  penalty_factor_tempTru = penalty_factor;
  penalty_factor_tempDia = penalty_factor;
  MpegEncContext *stempTru;
  MpegEncContext *stempDia;
  stempTru = s;
  stempDia = s;

  // printf ("Tru %d %d %d %d %d %d %d %d %d %d %d \n",
  // bestTempTru[0],bestTempTru[1], dTru, src_index, ref_index,
  // penalty_factor_tempTru, size, h, flags, s->mb_x, s->mb_y);
  dTru = flo_search(stempTru, bestTempTru, dTru, src_index, ref_index,
                    penalty_factor_tempTru, size, h, flags);
  int xTru;
  int yTru;
  xTru = bestTempTru[0];
  yTru = bestTempTru[1];
  bestTempDia[0] = bestxStart;
  bestTempDia[1] = bestyStart;
  // printf ("after true best %d %d %d %d \n", xTru, yTru
  // ,bestTempDia[0],bestTempDia[1]);

  // printf ("Dia %d %d %d %d %d %d %d %d %d %d %d \n",
  // bestTempDia[0],bestTempDia[1], dDia, src_index, ref_index,
  // penalty_factor_tempDia, size, h, flags, s->mb_x, s->mb_y);
  dDia = small_diamond_search(stempDia, bestTempDia, dDia, src_index, ref_index,
                              penalty_factor_tempDia, size, h, flags);

  int xDia;
  int yDia;
  xDia = bestTempDia[0];
  yDia = bestTempDia[1];

  // printf ("after dia best %d %d %d %d \n", xTru, yTru ,xDia,yDia);
  //	printf ("%d %d \n", dTru, dDia);
  // dDia=10000;
  int Thres = dDia;
  // fprintf(DJ3,"%d %d %d %d %d \n", s->coded_picture_number,s->mb_x, s->mb_y,
  // dTru, dDia);

  if (dTru <= Thres) {
    s = stempTru;
    int equal;
    if (dTru = Thres) {
      equal = 1;
    } else
      equal = 0;
    fprintf(DJ2, "%d, %d, %d, %d \n", s->coded_picture_number, s->mb_x, s->mb_y,
            equal);
    best[0] = xTru;
    best[1] = yTru;
    dmin = dTru;
    // truthCount++;
    //	printf( "Truth: %d
    //--------------------------------------------------------------- \n",
    // truthCount); 	penalty_factor=penalty_factor_tempTru; 	printf ("Tru :
    // returning mv  %d %d \n ", best[0], best[1]);

  } else {
    s = stempDia;
    fprintf(DJ2, "%d, %d, %d, %d  \n", s->coded_picture_number, s->mb_x,
            s->mb_y, 2);
    best[0] = xDia;
    best[1] = yDia;
    //	 penalty_factor=penalty_factor_tempDia;
    //	 printf ("Dia : returning mv  %d %d \n ", best[0], best[1]);
    dmin = dDia;
    // diaCount++;
    //      	printf( "Dia: %d \n", diaCount);
    // dDia= small_diamond_search(s, best, dmin, src_index, ref_index,
    // penalty_factor, size, h, flags);
  }
  fclose(DJ2);
  return dmin;
}

static av_always_inline int diamond_search(MpegEncContext *s, int *best,
                                           int dmin, int src_index,
                                           int ref_index,
                                           const int penalty_factor, int size,
                                           int h, int flags) {
  MotionEstContext *const c = &s->me;
  if (c->dia_size == -1) {
    printf("FUNNY \n");
    return funny_diamond_search(s, best, dmin, src_index, ref_index,
                                penalty_factor, size, h, flags);
  } else if (c->dia_size < -1) {

    printf("SAB \n");
    return sab_diamond_search(s, best, dmin, src_index, ref_index,
                              penalty_factor, size, h, flags);
  } else if (c->dia_size < 2) {
    //   	printf("small \n");
    //  	return zero_search(s, best, dmin, src_index, ref_index,
    //  penalty_factor, size, h, flags);
    return small_diamond_search(s, best, dmin, src_index, ref_index,
                                penalty_factor, size, h, flags);
    //  return      my_search(s, best, dmin, src_index, ref_index,
    //    penalty_factor, size, h, flags);
    // return flo_search(s, best, dmin, src_index, ref_index, penalty_factor,
    // size, h, flags);

  } else if (c->dia_size > 1024) {
    printf("full \n");
    return full_search(s, best, dmin, src_index, ref_index, penalty_factor,
                       size, h, flags);
  } else if (c->dia_size > 768) {
    printf(" \n");

    return umh_search(s, best, dmin, src_index, ref_index, penalty_factor, size,
                      h, flags);
  } else if (c->dia_size > 512) {
    printf("hex \n");
    return hex_search(s, best, dmin, src_index, ref_index, penalty_factor, size,
                      h, flags, c->dia_size & 0xFF);
  } else if (c->dia_size > 256) {
    printf("l2s \n");
    return l2s_dia_search(s, best, dmin, src_index, ref_index, penalty_factor,
                          size, h, flags);

  } else {
    printf("var \n");
    return var_diamond_search(s, best, dmin, src_index, ref_index,
                              penalty_factor, size, h, flags);
  }
}

/**
   @param P a list of candidate mvs to check before starting the
   iterative search. If one of the candidates is close to the optimal mv, then
   it takes fewer iterations. And it increases the chance that we find the
   optimal mv.
 */
static av_always_inline int
epzs_motion_search_internal(MpegEncContext *s, int *mx_ptr, int *my_ptr,
                            int P[10][2], int src_index, int ref_index,
                            int16_t (*last_mv)[2], int ref_mv_scale, int flags,
                            int size, int h) {
  MotionEstContext *const c = &s->me;
  int best[2] = {0, 0}; /**< x and y coordinates of the best motion vector.
                          i.e. the difference between the position of the
                          block currently being encoded and the position of
                          the block chosen to predict it from. */
  int d;                ///< the score (cmp + penalty) of any given mv
  int dmin;             /**< the best value of d, i.e. the score
                          corresponding to the mv stored in best[]. */
  unsigned map_generation;
  int penalty_factor;
  const int ref_mv_stride = s->mb_stride; // pass as arg  FIXME
  const int ref_mv_xy =
      s->mb_x + s->mb_y * ref_mv_stride; // add to last_mv before passing FIXME
  me_cmp_func cmpf, chroma_cmpf;

  LOAD_COMMON
  LOAD_COMMON2

  if (c->pre_pass) {
    penalty_factor = c->pre_penalty_factor;
    cmpf = s->mecc.me_pre_cmp[size];
    chroma_cmpf = s->mecc.me_pre_cmp[size + 1];
  } else {
    penalty_factor = c->penalty_factor;
    cmpf = s->mecc.me_cmp[size];
    chroma_cmpf = s->mecc.me_cmp[size + 1];
  }

  map_generation = update_map_generation(c);

  av_assert2(cmpf);
  dmin = cmp(s, 0, 0, 0, 0, size, h, ref_index, src_index, cmpf, chroma_cmpf,
             flags);
  map[0] = map_generation;
  score_map[0] = dmin;

  // FIXME precalc first term below?
  if ((s->pict_type == AV_PICTURE_TYPE_B && !(c->flags & FLAG_DIRECT)) ||
      s->mpv_flags & FF_MPV_FLAG_MV0)
    dmin += (mv_penalty[pred_x] + mv_penalty[pred_y]) * penalty_factor;

  /* first line */
  if (s->first_slice_line) {
    CHECK_MV(P_LEFT[0] >> shift, P_LEFT[1] >> shift)
    CHECK_CLIPPED_MV((last_mv[ref_mv_xy][0] * ref_mv_scale + (1 << 15)) >> 16,
                     (last_mv[ref_mv_xy][1] * ref_mv_scale + (1 << 15)) >> 16)
  } else {
    if (dmin < ((h * h * s->avctx->mv0_threshold) >> 8) &&
        (P_LEFT[0] | P_LEFT[1] | P_TOP[0] | P_TOP[1] | P_TOPRIGHT[0] |
         P_TOPRIGHT[1]) == 0) {
      *mx_ptr = 0;
      *my_ptr = 0;
      c->skip = 1;
      return dmin;
    }
    CHECK_MV(P_MEDIAN[0] >> shift, P_MEDIAN[1] >> shift)
    CHECK_CLIPPED_MV((P_MEDIAN[0] >> shift), (P_MEDIAN[1] >> shift) - 1)
    CHECK_CLIPPED_MV((P_MEDIAN[0] >> shift), (P_MEDIAN[1] >> shift) + 1)
    CHECK_CLIPPED_MV((P_MEDIAN[0] >> shift) - 1, (P_MEDIAN[1] >> shift))
    CHECK_CLIPPED_MV((P_MEDIAN[0] >> shift) + 1, (P_MEDIAN[1] >> shift))
    CHECK_CLIPPED_MV((last_mv[ref_mv_xy][0] * ref_mv_scale + (1 << 15)) >> 16,
                     (last_mv[ref_mv_xy][1] * ref_mv_scale + (1 << 15)) >> 16)
    CHECK_MV(P_LEFT[0] >> shift, P_LEFT[1] >> shift)
    CHECK_MV(P_TOP[0] >> shift, P_TOP[1] >> shift)
    CHECK_MV(P_TOPRIGHT[0] >> shift, P_TOPRIGHT[1] >> shift)
  }
  if (dmin > h * h * 4) {
    if (c->pre_pass) {
      CHECK_CLIPPED_MV(
          (last_mv[ref_mv_xy - 1][0] * ref_mv_scale + (1 << 15)) >> 16,
          (last_mv[ref_mv_xy - 1][1] * ref_mv_scale + (1 << 15)) >> 16)
      if (!s->first_slice_line)
        CHECK_CLIPPED_MV((last_mv[ref_mv_xy - ref_mv_stride][0] * ref_mv_scale +
                          (1 << 15)) >>
                             16,
                         (last_mv[ref_mv_xy - ref_mv_stride][1] * ref_mv_scale +
                          (1 << 15)) >>
                             16)
    } else {
      CHECK_CLIPPED_MV(
          (last_mv[ref_mv_xy + 1][0] * ref_mv_scale + (1 << 15)) >> 16,
          (last_mv[ref_mv_xy + 1][1] * ref_mv_scale + (1 << 15)) >> 16)
      if (s->mb_y + 1 <
          s->end_mb_y) // FIXME replace at least with last_slice_line
        CHECK_CLIPPED_MV((last_mv[ref_mv_xy + ref_mv_stride][0] * ref_mv_scale +
                          (1 << 15)) >>
                             16,
                         (last_mv[ref_mv_xy + ref_mv_stride][1] * ref_mv_scale +
                          (1 << 15)) >>
                             16)
    }
  }

  if (c->avctx->last_predictor_count) {
    const int count = c->avctx->last_predictor_count;
    const int xstart = FFMAX(0, s->mb_x - count);
    const int ystart = FFMAX(0, s->mb_y - count);
    const int xend = FFMIN(s->mb_width, s->mb_x + count + 1);
    const int yend = FFMIN(s->mb_height, s->mb_y + count + 1);
    int mb_y;

    for (mb_y = ystart; mb_y < yend; mb_y++) {
      int mb_x;
      for (mb_x = xstart; mb_x < xend; mb_x++) {
        const int xy = mb_x + 1 + (mb_y + 1) * ref_mv_stride;
        int mx = (last_mv[xy][0] * ref_mv_scale + (1 << 15)) >> 16;
        int my = (last_mv[xy][1] * ref_mv_scale + (1 << 15)) >> 16;

        if (mx > xmax || mx < xmin || my > ymax || my < ymin)
          continue;
        CHECK_MV(mx, my)
      }
    }
  }

  // check(best[0],best[1],0, b0)
  //   printf("epzs_motion_search_internal \n");
  dmin = diamond_search(s, best, dmin, src_index, ref_index, penalty_factor,
                        size, h, flags);

  // check(best[0],best[1],0, b1)
  *mx_ptr = best[0];
  *my_ptr = best[1];

  return dmin;
}

// this function is dedicated to the brain damaged gcc
int ff_epzs_motion_search(MpegEncContext *s, int *mx_ptr, int *my_ptr,
                          int P[10][2], int src_index, int ref_index,
                          int16_t (*last_mv)[2], int ref_mv_scale, int size,
                          int h) {
  MotionEstContext *const c = &s->me;
  // FIXME convert other functions in the same way if faster
  if (c->flags == 0 && h == 16 && size == 0) {
    // printf("option 1: ");
    return epzs_motion_search_internal(s, mx_ptr, my_ptr, P, src_index,
                                       ref_index, last_mv, ref_mv_scale, 0, 0,
                                       16);
    //    case FLAG_QPEL:
    //        return epzs_motion_search_internal(s, mx_ptr, my_ptr, P,
    //        src_index, ref_index, last_mv, ref_mv_scale, FLAG_QPEL);
  } else {

    return epzs_motion_search_internal(s, mx_ptr, my_ptr, P, src_index,
                                       ref_index, last_mv, ref_mv_scale,
                                       c->flags, size, h);
  }
}

static int epzs_motion_search4(MpegEncContext *s, int *mx_ptr, int *my_ptr,
                               int P[10][2], int src_index, int ref_index,
                               int16_t (*last_mv)[2], int ref_mv_scale) {
  MotionEstContext *const c = &s->me;
  int best[2] = {0, 0};
  int d, dmin;
  unsigned map_generation;
  const int penalty_factor = c->penalty_factor;
  const int size = 1;
  const int h = 8;
  const int ref_mv_stride = s->mb_stride;
  const int ref_mv_xy = s->mb_x + s->mb_y * ref_mv_stride;
  me_cmp_func cmpf, chroma_cmpf;
  LOAD_COMMON
  int flags = c->flags;
  LOAD_COMMON2

  cmpf = s->mecc.me_cmp[size];
  chroma_cmpf = s->mecc.me_cmp[size + 1];

  map_generation = update_map_generation(c);

  dmin = 1000000;

  /* first line */
  if (s->first_slice_line) {
    CHECK_MV(P_LEFT[0] >> shift, P_LEFT[1] >> shift)
    CHECK_CLIPPED_MV((last_mv[ref_mv_xy][0] * ref_mv_scale + (1 << 15)) >> 16,
                     (last_mv[ref_mv_xy][1] * ref_mv_scale + (1 << 15)) >> 16)
    CHECK_MV(P_MV1[0] >> shift, P_MV1[1] >> shift)
  } else {
    CHECK_MV(P_MV1[0] >> shift, P_MV1[1] >> shift)
    // FIXME try some early stop
    CHECK_MV(P_MEDIAN[0] >> shift, P_MEDIAN[1] >> shift)
    CHECK_MV(P_LEFT[0] >> shift, P_LEFT[1] >> shift)
    CHECK_MV(P_TOP[0] >> shift, P_TOP[1] >> shift)
    CHECK_MV(P_TOPRIGHT[0] >> shift, P_TOPRIGHT[1] >> shift)
    CHECK_CLIPPED_MV((last_mv[ref_mv_xy][0] * ref_mv_scale + (1 << 15)) >> 16,
                     (last_mv[ref_mv_xy][1] * ref_mv_scale + (1 << 15)) >> 16)
  }
  if (dmin > 64 * 4) {
    CHECK_CLIPPED_MV(
        (last_mv[ref_mv_xy + 1][0] * ref_mv_scale + (1 << 15)) >> 16,
        (last_mv[ref_mv_xy + 1][1] * ref_mv_scale + (1 << 15)) >> 16)
    if (s->mb_y + 1 < s->end_mb_y) // FIXME replace at least with
                                   // last_slice_line
      CHECK_CLIPPED_MV(
          (last_mv[ref_mv_xy + ref_mv_stride][0] * ref_mv_scale + (1 << 15)) >>
              16,
          (last_mv[ref_mv_xy + ref_mv_stride][1] * ref_mv_scale + (1 << 15)) >>
              16)
  }
  //   printf("epzs_motion_search4 \n");
  dmin = diamond_search(s, best, dmin, src_index, ref_index, penalty_factor,
                        size, h, flags);

  *mx_ptr = best[0];
  *my_ptr = best[1];

  return dmin;
}

// try to merge with above FIXME (needs PSNR test)
static int epzs_motion_search2(MpegEncContext *s, int *mx_ptr, int *my_ptr,
                               int P[10][2], int src_index, int ref_index,
                               int16_t (*last_mv)[2], int ref_mv_scale) {
  MotionEstContext *const c = &s->me;
  int best[2] = {0, 0};
  int d, dmin;
  unsigned map_generation;
  const int penalty_factor = c->penalty_factor;
  const int size = 0; // FIXME pass as arg
  const int h = 8;
  const int ref_mv_stride = s->mb_stride;
  const int ref_mv_xy = s->mb_x + s->mb_y * ref_mv_stride;
  me_cmp_func cmpf, chroma_cmpf;
  LOAD_COMMON
  int flags = c->flags;
  LOAD_COMMON2

  cmpf = s->mecc.me_cmp[size];
  chroma_cmpf = s->mecc.me_cmp[size + 1];

  map_generation = update_map_generation(c);

  dmin = 1000000;

  /* first line */
  if (s->first_slice_line) {
    CHECK_MV(P_LEFT[0] >> shift, P_LEFT[1] >> shift)
    CHECK_CLIPPED_MV((last_mv[ref_mv_xy][0] * ref_mv_scale + (1 << 15)) >> 16,
                     (last_mv[ref_mv_xy][1] * ref_mv_scale + (1 << 15)) >> 16)
    CHECK_MV(P_MV1[0] >> shift, P_MV1[1] >> shift)
  } else {
    CHECK_MV(P_MV1[0] >> shift, P_MV1[1] >> shift)
    // FIXME try some early stop
    CHECK_MV(P_MEDIAN[0] >> shift, P_MEDIAN[1] >> shift)
    CHECK_MV(P_LEFT[0] >> shift, P_LEFT[1] >> shift)
    CHECK_MV(P_TOP[0] >> shift, P_TOP[1] >> shift)
    CHECK_MV(P_TOPRIGHT[0] >> shift, P_TOPRIGHT[1] >> shift)
    CHECK_CLIPPED_MV((last_mv[ref_mv_xy][0] * ref_mv_scale + (1 << 15)) >> 16,
                     (last_mv[ref_mv_xy][1] * ref_mv_scale + (1 << 15)) >> 16)
  }
  if (dmin > 64 * 4) {
    CHECK_CLIPPED_MV(
        (last_mv[ref_mv_xy + 1][0] * ref_mv_scale + (1 << 15)) >> 16,
        (last_mv[ref_mv_xy + 1][1] * ref_mv_scale + (1 << 15)) >> 16)
    if (s->mb_y + 1 < s->end_mb_y) // FIXME replace at least with
                                   // last_slice_line
      CHECK_CLIPPED_MV(
          (last_mv[ref_mv_xy + ref_mv_stride][0] * ref_mv_scale + (1 << 15)) >>
              16,
          (last_mv[ref_mv_xy + ref_mv_stride][1] * ref_mv_scale + (1 << 15)) >>
              16)
  }
  //  printf("epzs_motion_search2 \n");
  dmin = diamond_search(s, best, dmin, src_index, ref_index, penalty_factor,
                        size, h, flags);

  *mx_ptr = best[0];
  *my_ptr = best[1];

  return dmin;
}
