/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_ports/mem.h"

#include "aom_dsp/aom_filter.h"
#include "aom_dsp/blend.h"
#include "aom_dsp/variance.h"

#include "av1/common/filter.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/reconinter.h"

uint32_t aom_get4x4sse_cs_c(const uint8_t *a, int a_stride, const uint8_t *b,
                            int b_stride) {
  int distortion = 0;
  int r, c;

  for (r = 0; r < 4; ++r) {
    for (c = 0; c < 4; ++c) {
      int diff = a[c] - b[c];
      distortion += diff * diff;
    }

    a += a_stride;
    b += b_stride;
  }

  return distortion;
}

uint32_t aom_get_mb_ss_c(const int16_t *a) {
  unsigned int i, sum = 0;

  for (i = 0; i < 256; ++i) {
    sum += a[i] * a[i];
  }

  return sum;
}

static void variance(const uint8_t *a, int a_stride, const uint8_t *b,
                     int b_stride, int w, int h, uint32_t *sse, int *sum) {
  int i, j;

  *sum = 0;
  *sse = 0;

  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; ++j) {
      const int diff = a[j] - b[j];
      *sum += diff;
      *sse += diff * diff;
    }

    a += a_stride;
    b += b_stride;
  }
}

uint32_t aom_sse_odd_size(const uint8_t *a, int a_stride, const uint8_t *b,
                          int b_stride, int w, int h) {
  uint32_t sse;
  int sum;
  variance(a, a_stride, b, b_stride, w, h, &sse, &sum);
  return sse;
}

// Applies a 1-D 2-tap bilinear filter to the source block in either horizontal
// or vertical direction to produce the filtered output block. Used to implement
// the first-pass of 2-D separable filter.
//
// Produces int16_t output to retain precision for the next pass. Two filter
// taps should sum to FILTER_WEIGHT. pixel_step defines whether the filter is
// applied horizontally (pixel_step = 1) or vertically (pixel_step = stride).
// It defines the offset required to move from one input to the next.
void aom_var_filter_block2d_bil_first_pass_c(const uint8_t *a, uint16_t *b,
                                             unsigned int src_pixels_per_line,
                                             unsigned int pixel_step,
                                             unsigned int output_height,
                                             unsigned int output_width,
                                             const uint8_t *filter) {
  unsigned int i, j;

  for (i = 0; i < output_height; ++i) {
    for (j = 0; j < output_width; ++j) {
      b[j] = ROUND_POWER_OF_TWO(
          (int)a[0] * filter[0] + (int)a[pixel_step] * filter[1], FILTER_BITS);

      ++a;
    }

    a += src_pixels_per_line - output_width;
    b += output_width;
  }
}

// Applies a 1-D 2-tap bilinear filter to the source block in either horizontal
// or vertical direction to produce the filtered output block. Used to implement
// the second-pass of 2-D separable filter.
//
// Requires 16-bit input as produced by filter_block2d_bil_first_pass. Two
// filter taps should sum to FILTER_WEIGHT. pixel_step defines whether the
// filter is applied horizontally (pixel_step = 1) or vertically
// (pixel_step = stride). It defines the offset required to move from one input
// to the next. Output is 8-bit.
void aom_var_filter_block2d_bil_second_pass_c(const uint16_t *a, uint8_t *b,
                                              unsigned int src_pixels_per_line,
                                              unsigned int pixel_step,
                                              unsigned int output_height,
                                              unsigned int output_width,
                                              const uint8_t *filter) {
  unsigned int i, j;

  for (i = 0; i < output_height; ++i) {
    for (j = 0; j < output_width; ++j) {
      b[j] = ROUND_POWER_OF_TWO(
          (int)a[0] * filter[0] + (int)a[pixel_step] * filter[1], FILTER_BITS);
      ++a;
    }

    a += src_pixels_per_line - output_width;
    b += output_width;
  }
}

#define VAR(W, H)                                                    \
  uint32_t aom_variance##W##x##H##_c(const uint8_t *a, int a_stride, \
                                     const uint8_t *b, int b_stride, \
                                     uint32_t *sse) {                \
    int sum;                                                         \
    variance(a, a_stride, b, b_stride, W, H, sse, &sum);             \
    return *sse - (uint32_t)(((int64_t)sum * sum) / (W * H));        \
  }

#define SUBPIX_VAR(W, H)                                                      \
  uint32_t aom_sub_pixel_variance##W##x##H##_c(                               \
      const uint8_t *a, int a_stride, int xoffset, int yoffset,               \
      const uint8_t *b, int b_stride, uint32_t *sse) {                        \
    uint16_t fdata3[(H + 1) * W];                                             \
    uint8_t temp2[H * W];                                                     \
                                                                              \
    aom_var_filter_block2d_bil_first_pass_c(a, fdata3, a_stride, 1, H + 1, W, \
                                            bilinear_filters_2t[xoffset]);    \
    aom_var_filter_block2d_bil_second_pass_c(fdata3, temp2, W, W, H, W,       \
                                             bilinear_filters_2t[yoffset]);   \
                                                                              \
    return aom_variance##W##x##H##_c(temp2, W, b, b_stride, sse);             \
  }

#define SUBPIX_AVG_VAR(W, H)                                                   \
  uint32_t aom_sub_pixel_avg_variance##W##x##H##_c(                            \
      const uint8_t *a, int a_stride, int xoffset, int yoffset,                \
      const uint8_t *b, int b_stride, uint32_t *sse,                           \
      const uint8_t *second_pred) {                                            \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint8_t temp2[H * W];                                                      \
    DECLARE_ALIGNED(16, uint8_t, temp3[H * W]);                                \
                                                                               \
    aom_var_filter_block2d_bil_first_pass_c(a, fdata3, a_stride, 1, H + 1, W,  \
                                            bilinear_filters_2t[xoffset]);     \
    aom_var_filter_block2d_bil_second_pass_c(fdata3, temp2, W, W, H, W,        \
                                             bilinear_filters_2t[yoffset]);    \
                                                                               \
    aom_comp_avg_pred(temp3, second_pred, W, H, temp2, W);                     \
                                                                               \
    return aom_variance##W##x##H##_c(temp3, W, b, b_stride, sse);              \
  }                                                                            \
  uint32_t aom_dist_wtd_sub_pixel_avg_variance##W##x##H##_c(                   \
      const uint8_t *a, int a_stride, int xoffset, int yoffset,                \
      const uint8_t *b, int b_stride, uint32_t *sse,                           \
      const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) {     \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint8_t temp2[H * W];                                                      \
    DECLARE_ALIGNED(16, uint8_t, temp3[H * W]);                                \
                                                                               \
    aom_var_filter_block2d_bil_first_pass_c(a, fdata3, a_stride, 1, H + 1, W,  \
                                            bilinear_filters_2t[xoffset]);     \
    aom_var_filter_block2d_bil_second_pass_c(fdata3, temp2, W, W, H, W,        \
                                             bilinear_filters_2t[yoffset]);    \
                                                                               \
    aom_dist_wtd_comp_avg_pred(temp3, second_pred, W, H, temp2, W, jcp_param); \
                                                                               \
    return aom_variance##W##x##H(temp3, W, b, b_stride, sse);                  \
  }

/* Identical to the variance call except it takes an additional parameter, sum,
 * and returns that value using pass-by-reference instead of returning
 * sse - sum^2 / w*h
 */
#define GET_VAR(W, H)                                                         \
  void aom_get##W##x##H##var_c(const uint8_t *a, int a_stride,                \
                               const uint8_t *b, int b_stride, uint32_t *sse, \
                               int *sum) {                                    \
    variance(a, a_stride, b, b_stride, W, H, sse, sum);                       \
  }

/* Identical to the variance call except it does not calculate the
 * sse - sum^2 / w*h and returns sse in addtion to modifying the passed in
 * variable.
 */
#define MSE(W, H)                                               \
  uint32_t aom_mse##W##x##H##_c(const uint8_t *a, int a_stride, \
                                const uint8_t *b, int b_stride, \
                                uint32_t *sse) {                \
    int sum;                                                    \
    variance(a, a_stride, b, b_stride, W, H, sse, &sum);        \
    return *sse;                                                \
  }

/* All three forms of the variance are available in the same sizes. */
#define VARIANCES(W, H) \
  VAR(W, H)             \
  SUBPIX_VAR(W, H)      \
  SUBPIX_AVG_VAR(W, H)

VARIANCES(128, 128)
VARIANCES(128, 64)
VARIANCES(64, 128)
VARIANCES(64, 64)
VARIANCES(64, 32)
VARIANCES(32, 64)
VARIANCES(32, 32)
VARIANCES(32, 16)
VARIANCES(16, 32)
VARIANCES(16, 16)
VARIANCES(16, 8)
VARIANCES(8, 16)
VARIANCES(8, 8)
VARIANCES(8, 4)
VARIANCES(4, 8)
VARIANCES(4, 4)
VARIANCES(4, 2)
VARIANCES(2, 4)
VARIANCES(2, 2)
VARIANCES(4, 16)
VARIANCES(16, 4)
VARIANCES(8, 32)
VARIANCES(32, 8)
VARIANCES(16, 64)
VARIANCES(64, 16)

GET_VAR(16, 16)
GET_VAR(8, 8)

MSE(16, 16)
MSE(16, 8)
MSE(8, 16)
MSE(8, 8)

void aom_comp_avg_pred_c(uint8_t *comp_pred, const uint8_t *pred, int width,
                         int height, const uint8_t *ref, int ref_stride) {
  int i, j;

  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int tmp = pred[j] + ref[j];
      comp_pred[j] = ROUND_POWER_OF_TWO(tmp, 1);
    }
    comp_pred += width;
    pred += width;
    ref += ref_stride;
  }
}

// Get pred block from up-sampled reference.
void aom_upsampled_pred_c(MACROBLOCKD *xd, const AV1_COMMON *const cm,
                          int mi_row, int mi_col, const MV *const mv,
                          uint8_t *comp_pred, int width, int height,
                          int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
                          int ref_stride, int subpel_search) {
  // expect xd == NULL only in tests
  if (xd != NULL) {
    const MB_MODE_INFO *mi = xd->mi[0];
    const int ref_num = 0;
    const int is_intrabc = is_intrabc_block(mi);
    const struct scale_factors *const sf =
        is_intrabc ? &cm->sf_identity : xd->block_ref_scale_factors[ref_num];
    const int is_scaled = av1_is_scaled(sf);

    if (is_scaled) {
      // Note: This is mostly a copy from the >=8X8 case in
      // build_inter_predictors() function, with some small tweaks.

      // Some assumptions.
      const int plane = 0;

      // Get pre-requisites.
      const struct macroblockd_plane *const pd = &xd->plane[plane];
      const int ssx = pd->subsampling_x;
      const int ssy = pd->subsampling_y;
      assert(ssx == 0 && ssy == 0);
      const struct buf_2d *const dst_buf = &pd->dst;
      const struct buf_2d *const pre_buf =
          is_intrabc ? dst_buf : &pd->pre[ref_num];
      const int mi_x = mi_col * MI_SIZE;
      const int mi_y = mi_row * MI_SIZE;

      // Calculate subpel_x/y and x/y_step.
      const int row_start = 0;  // Because ss_y is 0.
      const int col_start = 0;  // Because ss_x is 0.
      const int pre_x = (mi_x + MI_SIZE * col_start) >> ssx;
      const int pre_y = (mi_y + MI_SIZE * row_start) >> ssy;
      int orig_pos_y = pre_y << SUBPEL_BITS;
      orig_pos_y += mv->row * (1 << (1 - ssy));
      int orig_pos_x = pre_x << SUBPEL_BITS;
      orig_pos_x += mv->col * (1 << (1 - ssx));
      int pos_y = sf->scale_value_y(orig_pos_y, sf);
      int pos_x = sf->scale_value_x(orig_pos_x, sf);
      pos_x += SCALE_EXTRA_OFF;
      pos_y += SCALE_EXTRA_OFF;

      const int top = -AOM_LEFT_TOP_MARGIN_SCALED(ssy);
      const int left = -AOM_LEFT_TOP_MARGIN_SCALED(ssx);
      const int bottom = (pre_buf->height + AOM_INTERP_EXTEND)
                         << SCALE_SUBPEL_BITS;
      const int right = (pre_buf->width + AOM_INTERP_EXTEND)
                        << SCALE_SUBPEL_BITS;
      pos_y = clamp(pos_y, top, bottom);
      pos_x = clamp(pos_x, left, right);

      const uint8_t *const pre =
          pre_buf->buf0 + (pos_y >> SCALE_SUBPEL_BITS) * pre_buf->stride +
          (pos_x >> SCALE_SUBPEL_BITS);

      InterPredParams inter_pred_params;

      const SubpelParams subpel_params = { sf->x_step_q4, sf->y_step_q4,
                                           pos_x & SCALE_SUBPEL_MASK,
                                           pos_y & SCALE_SUBPEL_MASK };

      // Get convolve parameters.
      inter_pred_params.conv_params = get_conv_params(0, plane, xd->bd);
      const int_interpfilters filters =
          av1_broadcast_interp_filter(EIGHTTAP_REGULAR);

      av1_init_inter_params(
          &inter_pred_params, width, height, mi_y >> pd->subsampling_y,
          mi_x >> pd->subsampling_x, pd->subsampling_x, pd->subsampling_y,
          xd->bd, is_cur_buf_hbd(xd), mi->use_intrabc, sf, filters);

      // Get the inter predictor.
      av1_make_inter_predictor(pre, pre_buf->stride, comp_pred, width,
                               &inter_pred_params, &subpel_params);

      return;
    }
  }

  const InterpFilterParams *filter = av1_get_filter(subpel_search);

  if (!subpel_x_q3 && !subpel_y_q3) {
    for (int i = 0; i < height; i++) {
      memcpy(comp_pred, ref, width * sizeof(*comp_pred));
      comp_pred += width;
      ref += ref_stride;
    }
  } else if (!subpel_y_q3) {
    const int16_t *const kernel =
        av1_get_interp_filter_subpel_kernel(filter, subpel_x_q3 << 1);
    aom_convolve8_horiz_c(ref, ref_stride, comp_pred, width, kernel, 16, NULL,
                          -1, width, height);
  } else if (!subpel_x_q3) {
    const int16_t *const kernel =
        av1_get_interp_filter_subpel_kernel(filter, subpel_y_q3 << 1);
    aom_convolve8_vert_c(ref, ref_stride, comp_pred, width, NULL, -1, kernel,
                         16, width, height);
  } else {
    DECLARE_ALIGNED(16, uint8_t,
                    temp[((MAX_SB_SIZE * 2 + 16) + 16) * MAX_SB_SIZE]);
    const int16_t *const kernel_x =
        av1_get_interp_filter_subpel_kernel(filter, subpel_x_q3 << 1);
    const int16_t *const kernel_y =
        av1_get_interp_filter_subpel_kernel(filter, subpel_y_q3 << 1);
    const int intermediate_height =
        (((height - 1) * 8 + subpel_y_q3) >> 3) + filter->taps;
    assert(intermediate_height <= (MAX_SB_SIZE * 2 + 16) + 16);
    aom_convolve8_horiz_c(ref - ref_stride * ((filter->taps >> 1) - 1),
                          ref_stride, temp, MAX_SB_SIZE, kernel_x, 16, NULL, -1,
                          width, intermediate_height);
    aom_convolve8_vert_c(temp + MAX_SB_SIZE * ((filter->taps >> 1) - 1),
                         MAX_SB_SIZE, comp_pred, width, NULL, -1, kernel_y, 16,
                         width, height);
  }
}

void aom_comp_avg_upsampled_pred_c(MACROBLOCKD *xd, const AV1_COMMON *const cm,
                                   int mi_row, int mi_col, const MV *const mv,
                                   uint8_t *comp_pred, const uint8_t *pred,
                                   int width, int height, int subpel_x_q3,
                                   int subpel_y_q3, const uint8_t *ref,
                                   int ref_stride, int subpel_search) {
  int i, j;

  aom_upsampled_pred(xd, cm, mi_row, mi_col, mv, comp_pred, width, height,
                     subpel_x_q3, subpel_y_q3, ref, ref_stride, subpel_search);
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      comp_pred[j] = ROUND_POWER_OF_TWO(comp_pred[j] + pred[j], 1);
    }
    comp_pred += width;
    pred += width;
  }
}

void aom_dist_wtd_comp_avg_pred_c(uint8_t *comp_pred, const uint8_t *pred,
                                  int width, int height, const uint8_t *ref,
                                  int ref_stride,
                                  const DIST_WTD_COMP_PARAMS *jcp_param) {
  int i, j;
  const int fwd_offset = jcp_param->fwd_offset;
  const int bck_offset = jcp_param->bck_offset;

  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      int tmp = pred[j] * bck_offset + ref[j] * fwd_offset;
      tmp = ROUND_POWER_OF_TWO(tmp, DIST_PRECISION_BITS);
      comp_pred[j] = (uint8_t)tmp;
    }
    comp_pred += width;
    pred += width;
    ref += ref_stride;
  }
}

void aom_dist_wtd_comp_avg_upsampled_pred_c(
    MACROBLOCKD *xd, const AV1_COMMON *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
    int ref_stride, const DIST_WTD_COMP_PARAMS *jcp_param, int subpel_search) {
  int i, j;
  const int fwd_offset = jcp_param->fwd_offset;
  const int bck_offset = jcp_param->bck_offset;

  aom_upsampled_pred_c(xd, cm, mi_row, mi_col, mv, comp_pred, width, height,
                       subpel_x_q3, subpel_y_q3, ref, ref_stride,
                       subpel_search);

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      int tmp = pred[j] * bck_offset + comp_pred[j] * fwd_offset;
      tmp = ROUND_POWER_OF_TWO(tmp, DIST_PRECISION_BITS);
      comp_pred[j] = (uint8_t)tmp;
    }
    comp_pred += width;
    pred += width;
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
static void highbd_variance64(const uint8_t *a8, int a_stride,
                              const uint8_t *b8, int b_stride, int w, int h,
                              uint64_t *sse, int64_t *sum) {
  const uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b = CONVERT_TO_SHORTPTR(b8);
  int64_t tsum = 0;
  uint64_t tsse = 0;
  for (int i = 0; i < h; ++i) {
    int32_t lsum = 0;
    for (int j = 0; j < w; ++j) {
      const int diff = a[j] - b[j];
      lsum += diff;
      tsse += (uint32_t)(diff * diff);
    }
    tsum += lsum;
    a += a_stride;
    b += b_stride;
  }
  *sum = tsum;
  *sse = tsse;
}

uint64_t aom_highbd_sse_odd_size(const uint8_t *a, int a_stride,
                                 const uint8_t *b, int b_stride, int w, int h) {
  uint64_t sse;
  int64_t sum;
  highbd_variance64(a, a_stride, b, b_stride, w, h, &sse, &sum);
  return sse;
}

static void highbd_8_variance(const uint8_t *a8, int a_stride,
                              const uint8_t *b8, int b_stride, int w, int h,
                              uint32_t *sse, int *sum) {
  uint64_t sse_long = 0;
  int64_t sum_long = 0;
  highbd_variance64(a8, a_stride, b8, b_stride, w, h, &sse_long, &sum_long);
  *sse = (uint32_t)sse_long;
  *sum = (int)sum_long;
}

static void highbd_10_variance(const uint8_t *a8, int a_stride,
                               const uint8_t *b8, int b_stride, int w, int h,
                               uint32_t *sse, int *sum) {
  uint64_t sse_long = 0;
  int64_t sum_long = 0;
  highbd_variance64(a8, a_stride, b8, b_stride, w, h, &sse_long, &sum_long);
  *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 4);
  *sum = (int)ROUND_POWER_OF_TWO(sum_long, 2);
}

static void highbd_12_variance(const uint8_t *a8, int a_stride,
                               const uint8_t *b8, int b_stride, int w, int h,
                               uint32_t *sse, int *sum) {
  uint64_t sse_long = 0;
  int64_t sum_long = 0;
  highbd_variance64(a8, a_stride, b8, b_stride, w, h, &sse_long, &sum_long);
  *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 8);
  *sum = (int)ROUND_POWER_OF_TWO(sum_long, 4);
}

#define HIGHBD_VAR(W, H)                                                       \
  uint32_t aom_highbd_8_variance##W##x##H##_c(const uint8_t *a, int a_stride,  \
                                              const uint8_t *b, int b_stride,  \
                                              uint32_t *sse) {                 \
    int sum;                                                                   \
    highbd_8_variance(a, a_stride, b, b_stride, W, H, sse, &sum);              \
    return *sse - (uint32_t)(((int64_t)sum * sum) / (W * H));                  \
  }                                                                            \
                                                                               \
  uint32_t aom_highbd_10_variance##W##x##H##_c(const uint8_t *a, int a_stride, \
                                               const uint8_t *b, int b_stride, \
                                               uint32_t *sse) {                \
    int sum;                                                                   \
    int64_t var;                                                               \
    highbd_10_variance(a, a_stride, b, b_stride, W, H, sse, &sum);             \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (W * H));                  \
    return (var >= 0) ? (uint32_t)var : 0;                                     \
  }                                                                            \
                                                                               \
  uint32_t aom_highbd_12_variance##W##x##H##_c(const uint8_t *a, int a_stride, \
                                               const uint8_t *b, int b_stride, \
                                               uint32_t *sse) {                \
    int sum;                                                                   \
    int64_t var;                                                               \
    highbd_12_variance(a, a_stride, b, b_stride, W, H, sse, &sum);             \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (W * H));                  \
    return (var >= 0) ? (uint32_t)var : 0;                                     \
  }

#define HIGHBD_GET_VAR(S)                                                    \
  void aom_highbd_8_get##S##x##S##var_c(const uint8_t *src, int src_stride,  \
                                        const uint8_t *ref, int ref_stride,  \
                                        uint32_t *sse, int *sum) {           \
    highbd_8_variance(src, src_stride, ref, ref_stride, S, S, sse, sum);     \
  }                                                                          \
                                                                             \
  void aom_highbd_10_get##S##x##S##var_c(const uint8_t *src, int src_stride, \
                                         const uint8_t *ref, int ref_stride, \
                                         uint32_t *sse, int *sum) {          \
    highbd_10_variance(src, src_stride, ref, ref_stride, S, S, sse, sum);    \
  }                                                                          \
                                                                             \
  void aom_highbd_12_get##S##x##S##var_c(const uint8_t *src, int src_stride, \
                                         const uint8_t *ref, int ref_stride, \
                                         uint32_t *sse, int *sum) {          \
    highbd_12_variance(src, src_stride, ref, ref_stride, S, S, sse, sum);    \
  }

#define HIGHBD_MSE(W, H)                                                      \
  uint32_t aom_highbd_8_mse##W##x##H##_c(const uint8_t *src, int src_stride,  \
                                         const uint8_t *ref, int ref_stride,  \
                                         uint32_t *sse) {                     \
    int sum;                                                                  \
    highbd_8_variance(src, src_stride, ref, ref_stride, W, H, sse, &sum);     \
    return *sse;                                                              \
  }                                                                           \
                                                                              \
  uint32_t aom_highbd_10_mse##W##x##H##_c(const uint8_t *src, int src_stride, \
                                          const uint8_t *ref, int ref_stride, \
                                          uint32_t *sse) {                    \
    int sum;                                                                  \
    highbd_10_variance(src, src_stride, ref, ref_stride, W, H, sse, &sum);    \
    return *sse;                                                              \
  }                                                                           \
                                                                              \
  uint32_t aom_highbd_12_mse##W##x##H##_c(const uint8_t *src, int src_stride, \
                                          const uint8_t *ref, int ref_stride, \
                                          uint32_t *sse) {                    \
    int sum;                                                                  \
    highbd_12_variance(src, src_stride, ref, ref_stride, W, H, sse, &sum);    \
    return *sse;                                                              \
  }

void aom_highbd_var_filter_block2d_bil_first_pass(
    const uint8_t *src_ptr8, uint16_t *output_ptr,
    unsigned int src_pixels_per_line, int pixel_step,
    unsigned int output_height, unsigned int output_width,
    const uint8_t *filter) {
  unsigned int i, j;
  uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src_ptr8);
  for (i = 0; i < output_height; ++i) {
    for (j = 0; j < output_width; ++j) {
      output_ptr[j] = ROUND_POWER_OF_TWO(
          (int)src_ptr[0] * filter[0] + (int)src_ptr[pixel_step] * filter[1],
          FILTER_BITS);

      ++src_ptr;
    }

    // Next row...
    src_ptr += src_pixels_per_line - output_width;
    output_ptr += output_width;
  }
}

void aom_highbd_var_filter_block2d_bil_second_pass(
    const uint16_t *src_ptr, uint16_t *output_ptr,
    unsigned int src_pixels_per_line, unsigned int pixel_step,
    unsigned int output_height, unsigned int output_width,
    const uint8_t *filter) {
  unsigned int i, j;

  for (i = 0; i < output_height; ++i) {
    for (j = 0; j < output_width; ++j) {
      output_ptr[j] = ROUND_POWER_OF_TWO(
          (int)src_ptr[0] * filter[0] + (int)src_ptr[pixel_step] * filter[1],
          FILTER_BITS);
      ++src_ptr;
    }

    src_ptr += src_pixels_per_line - output_width;
    output_ptr += output_width;
  }
}

#define HIGHBD_SUBPIX_VAR(W, H)                                              \
  uint32_t aom_highbd_8_sub_pixel_variance##W##x##H##_c(                     \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,          \
      const uint8_t *dst, int dst_stride, uint32_t *sse) {                   \
    uint16_t fdata3[(H + 1) * W];                                            \
    uint16_t temp2[H * W];                                                   \
                                                                             \
    aom_highbd_var_filter_block2d_bil_first_pass(                            \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]); \
    aom_highbd_var_filter_block2d_bil_second_pass(                           \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);            \
                                                                             \
    return aom_highbd_8_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp2), W,  \
                                              dst, dst_stride, sse);         \
  }                                                                          \
                                                                             \
  uint32_t aom_highbd_10_sub_pixel_variance##W##x##H##_c(                    \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,          \
      const uint8_t *dst, int dst_stride, uint32_t *sse) {                   \
    uint16_t fdata3[(H + 1) * W];                                            \
    uint16_t temp2[H * W];                                                   \
                                                                             \
    aom_highbd_var_filter_block2d_bil_first_pass(                            \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]); \
    aom_highbd_var_filter_block2d_bil_second_pass(                           \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);            \
                                                                             \
    return aom_highbd_10_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp2), W, \
                                               dst, dst_stride, sse);        \
  }                                                                          \
                                                                             \
  uint32_t aom_highbd_12_sub_pixel_variance##W##x##H##_c(                    \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,          \
      const uint8_t *dst, int dst_stride, uint32_t *sse) {                   \
    uint16_t fdata3[(H + 1) * W];                                            \
    uint16_t temp2[H * W];                                                   \
                                                                             \
    aom_highbd_var_filter_block2d_bil_first_pass(                            \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]); \
    aom_highbd_var_filter_block2d_bil_second_pass(                           \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);            \
                                                                             \
    return aom_highbd_12_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp2), W, \
                                               dst, dst_stride, sse);        \
  }

#define HIGHBD_SUBPIX_AVG_VAR(W, H)                                           \
  uint32_t aom_highbd_8_sub_pixel_avg_variance##W##x##H##_c(                  \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *dst, int dst_stride, uint32_t *sse,                      \
      const uint8_t *second_pred) {                                           \
    uint16_t fdata3[(H + 1) * W];                                             \
    uint16_t temp2[H * W];                                                    \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                              \
                                                                              \
    aom_highbd_var_filter_block2d_bil_first_pass(                             \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);  \
    aom_highbd_var_filter_block2d_bil_second_pass(                            \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);             \
                                                                              \
    aom_highbd_comp_avg_pred_c(CONVERT_TO_BYTEPTR(temp3), second_pred, W, H,  \
                               CONVERT_TO_BYTEPTR(temp2), W);                 \
                                                                              \
    return aom_highbd_8_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp3), W,   \
                                              dst, dst_stride, sse);          \
  }                                                                           \
                                                                              \
  uint32_t aom_highbd_10_sub_pixel_avg_variance##W##x##H##_c(                 \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *dst, int dst_stride, uint32_t *sse,                      \
      const uint8_t *second_pred) {                                           \
    uint16_t fdata3[(H + 1) * W];                                             \
    uint16_t temp2[H * W];                                                    \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                              \
                                                                              \
    aom_highbd_var_filter_block2d_bil_first_pass(                             \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);  \
    aom_highbd_var_filter_block2d_bil_second_pass(                            \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);             \
                                                                              \
    aom_highbd_comp_avg_pred_c(CONVERT_TO_BYTEPTR(temp3), second_pred, W, H,  \
                               CONVERT_TO_BYTEPTR(temp2), W);                 \
                                                                              \
    return aom_highbd_10_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp3), W,  \
                                               dst, dst_stride, sse);         \
  }                                                                           \
                                                                              \
  uint32_t aom_highbd_12_sub_pixel_avg_variance##W##x##H##_c(                 \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *dst, int dst_stride, uint32_t *sse,                      \
      const uint8_t *second_pred) {                                           \
    uint16_t fdata3[(H + 1) * W];                                             \
    uint16_t temp2[H * W];                                                    \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                              \
                                                                              \
    aom_highbd_var_filter_block2d_bil_first_pass(                             \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);  \
    aom_highbd_var_filter_block2d_bil_second_pass(                            \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);             \
                                                                              \
    aom_highbd_comp_avg_pred_c(CONVERT_TO_BYTEPTR(temp3), second_pred, W, H,  \
                               CONVERT_TO_BYTEPTR(temp2), W);                 \
                                                                              \
    return aom_highbd_12_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp3), W,  \
                                               dst, dst_stride, sse);         \
  }                                                                           \
                                                                              \
  uint32_t aom_highbd_8_dist_wtd_sub_pixel_avg_variance##W##x##H##_c(         \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *dst, int dst_stride, uint32_t *sse,                      \
      const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) {    \
    uint16_t fdata3[(H + 1) * W];                                             \
    uint16_t temp2[H * W];                                                    \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                              \
                                                                              \
    aom_highbd_var_filter_block2d_bil_first_pass(                             \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);  \
    aom_highbd_var_filter_block2d_bil_second_pass(                            \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);             \
                                                                              \
    aom_highbd_dist_wtd_comp_avg_pred(CONVERT_TO_BYTEPTR(temp3), second_pred, \
                                      W, H, CONVERT_TO_BYTEPTR(temp2), W,     \
                                      jcp_param);                             \
                                                                              \
    return aom_highbd_8_variance##W##x##H(CONVERT_TO_BYTEPTR(temp3), W, dst,  \
                                          dst_stride, sse);                   \
  }                                                                           \
                                                                              \
  uint32_t aom_highbd_10_dist_wtd_sub_pixel_avg_variance##W##x##H##_c(        \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *dst, int dst_stride, uint32_t *sse,                      \
      const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) {    \
    uint16_t fdata3[(H + 1) * W];                                             \
    uint16_t temp2[H * W];                                                    \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                              \
                                                                              \
    aom_highbd_var_filter_block2d_bil_first_pass(                             \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);  \
    aom_highbd_var_filter_block2d_bil_second_pass(                            \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);             \
                                                                              \
    aom_highbd_dist_wtd_comp_avg_pred(CONVERT_TO_BYTEPTR(temp3), second_pred, \
                                      W, H, CONVERT_TO_BYTEPTR(temp2), W,     \
                                      jcp_param);                             \
                                                                              \
    return aom_highbd_10_variance##W##x##H(CONVERT_TO_BYTEPTR(temp3), W, dst, \
                                           dst_stride, sse);                  \
  }                                                                           \
                                                                              \
  uint32_t aom_highbd_12_dist_wtd_sub_pixel_avg_variance##W##x##H##_c(        \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *dst, int dst_stride, uint32_t *sse,                      \
      const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) {    \
    uint16_t fdata3[(H + 1) * W];                                             \
    uint16_t temp2[H * W];                                                    \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                              \
                                                                              \
    aom_highbd_var_filter_block2d_bil_first_pass(                             \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);  \
    aom_highbd_var_filter_block2d_bil_second_pass(                            \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);             \
                                                                              \
    aom_highbd_dist_wtd_comp_avg_pred(CONVERT_TO_BYTEPTR(temp3), second_pred, \
                                      W, H, CONVERT_TO_BYTEPTR(temp2), W,     \
                                      jcp_param);                             \
                                                                              \
    return aom_highbd_12_variance##W##x##H(CONVERT_TO_BYTEPTR(temp3), W, dst, \
                                           dst_stride, sse);                  \
  }

/* All three forms of the variance are available in the same sizes. */
#define HIGHBD_VARIANCES(W, H) \
  HIGHBD_VAR(W, H)             \
  HIGHBD_SUBPIX_VAR(W, H)      \
  HIGHBD_SUBPIX_AVG_VAR(W, H)

HIGHBD_VARIANCES(128, 128)
HIGHBD_VARIANCES(128, 64)
HIGHBD_VARIANCES(64, 128)
HIGHBD_VARIANCES(64, 64)
HIGHBD_VARIANCES(64, 32)
HIGHBD_VARIANCES(32, 64)
HIGHBD_VARIANCES(32, 32)
HIGHBD_VARIANCES(32, 16)
HIGHBD_VARIANCES(16, 32)
HIGHBD_VARIANCES(16, 16)
HIGHBD_VARIANCES(16, 8)
HIGHBD_VARIANCES(8, 16)
HIGHBD_VARIANCES(8, 8)
HIGHBD_VARIANCES(8, 4)
HIGHBD_VARIANCES(4, 8)
HIGHBD_VARIANCES(4, 4)
HIGHBD_VARIANCES(4, 2)
HIGHBD_VARIANCES(2, 4)
HIGHBD_VARIANCES(2, 2)
HIGHBD_VARIANCES(4, 16)
HIGHBD_VARIANCES(16, 4)
HIGHBD_VARIANCES(8, 32)
HIGHBD_VARIANCES(32, 8)
HIGHBD_VARIANCES(16, 64)
HIGHBD_VARIANCES(64, 16)

HIGHBD_GET_VAR(8)
HIGHBD_GET_VAR(16)

HIGHBD_MSE(16, 16)
HIGHBD_MSE(16, 8)
HIGHBD_MSE(8, 16)
HIGHBD_MSE(8, 8)

void aom_highbd_comp_avg_pred_c(uint8_t *comp_pred8, const uint8_t *pred8,
                                int width, int height, const uint8_t *ref8,
                                int ref_stride) {
  int i, j;
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int tmp = pred[j] + ref[j];
      comp_pred[j] = ROUND_POWER_OF_TWO(tmp, 1);
    }
    comp_pred += width;
    pred += width;
    ref += ref_stride;
  }
}

void aom_highbd_upsampled_pred_c(MACROBLOCKD *xd,
                                 const struct AV1Common *const cm, int mi_row,
                                 int mi_col, const MV *const mv,
                                 uint8_t *comp_pred8, int width, int height,
                                 int subpel_x_q3, int subpel_y_q3,
                                 const uint8_t *ref8, int ref_stride, int bd,
                                 int subpel_search) {
  // expect xd == NULL only in tests
  if (xd != NULL) {
    const MB_MODE_INFO *mi = xd->mi[0];
    const int ref_num = 0;
    const int is_intrabc = is_intrabc_block(mi);
    const struct scale_factors *const sf =
        is_intrabc ? &cm->sf_identity : xd->block_ref_scale_factors[ref_num];
    const int is_scaled = av1_is_scaled(sf);

    if (is_scaled) {
      // Note: This is mostly a copy from the >=8X8 case in
      // build_inter_predictors() function, with some small tweaks.
      // Some assumptions.
      const int plane = 0;

      // Get pre-requisites.
      const struct macroblockd_plane *const pd = &xd->plane[plane];
      const int ssx = pd->subsampling_x;
      const int ssy = pd->subsampling_y;
      assert(ssx == 0 && ssy == 0);
      const struct buf_2d *const dst_buf = &pd->dst;
      const struct buf_2d *const pre_buf =
          is_intrabc ? dst_buf : &pd->pre[ref_num];
      const int mi_x = mi_col * MI_SIZE;
      const int mi_y = mi_row * MI_SIZE;

      // Calculate subpel_x/y and x/y_step.
      const int row_start = 0;  // Because ss_y is 0.
      const int col_start = 0;  // Because ss_x is 0.
      const int pre_x = (mi_x + MI_SIZE * col_start) >> ssx;
      const int pre_y = (mi_y + MI_SIZE * row_start) >> ssy;
      int orig_pos_y = pre_y << SUBPEL_BITS;
      orig_pos_y += mv->row * (1 << (1 - ssy));
      int orig_pos_x = pre_x << SUBPEL_BITS;
      orig_pos_x += mv->col * (1 << (1 - ssx));
      int pos_y = sf->scale_value_y(orig_pos_y, sf);
      int pos_x = sf->scale_value_x(orig_pos_x, sf);
      pos_x += SCALE_EXTRA_OFF;
      pos_y += SCALE_EXTRA_OFF;

      const int top = -AOM_LEFT_TOP_MARGIN_SCALED(ssy);
      const int left = -AOM_LEFT_TOP_MARGIN_SCALED(ssx);
      const int bottom = (pre_buf->height + AOM_INTERP_EXTEND)
                         << SCALE_SUBPEL_BITS;
      const int right = (pre_buf->width + AOM_INTERP_EXTEND)
                        << SCALE_SUBPEL_BITS;
      pos_y = clamp(pos_y, top, bottom);
      pos_x = clamp(pos_x, left, right);

      const uint8_t *const pre =
          pre_buf->buf0 + (pos_y >> SCALE_SUBPEL_BITS) * pre_buf->stride +
          (pos_x >> SCALE_SUBPEL_BITS);

      InterPredParams inter_pred_params;

      const SubpelParams subpel_params = { sf->x_step_q4, sf->y_step_q4,
                                           pos_x & SCALE_SUBPEL_MASK,
                                           pos_y & SCALE_SUBPEL_MASK };

      // Get convolve parameters.
      inter_pred_params.conv_params = get_conv_params(0, plane, xd->bd);
      const int_interpfilters filters =
          av1_broadcast_interp_filter(EIGHTTAP_REGULAR);

      av1_init_inter_params(
          &inter_pred_params, width, height, mi_y >> pd->subsampling_y,
          mi_x >> pd->subsampling_x, pd->subsampling_x, pd->subsampling_y,
          xd->bd, is_cur_buf_hbd(xd), mi->use_intrabc, sf, filters);

      // Get the inter predictor.
      av1_make_inter_predictor(pre, pre_buf->stride, comp_pred8, width,
                               &inter_pred_params, &subpel_params);

      return;
    }
  }

  const InterpFilterParams *filter = av1_get_filter(subpel_search);

  if (!subpel_x_q3 && !subpel_y_q3) {
    const uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
    uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
    for (int i = 0; i < height; i++) {
      memcpy(comp_pred, ref, width * sizeof(*comp_pred));
      comp_pred += width;
      ref += ref_stride;
    }
  } else if (!subpel_y_q3) {
    const int16_t *const kernel =
        av1_get_interp_filter_subpel_kernel(filter, subpel_x_q3 << 1);
    aom_highbd_convolve8_horiz_c(ref8, ref_stride, comp_pred8, width, kernel,
                                 16, NULL, -1, width, height, bd);
  } else if (!subpel_x_q3) {
    const int16_t *const kernel =
        av1_get_interp_filter_subpel_kernel(filter, subpel_y_q3 << 1);
    aom_highbd_convolve8_vert_c(ref8, ref_stride, comp_pred8, width, NULL, -1,
                                kernel, 16, width, height, bd);
  } else {
    DECLARE_ALIGNED(16, uint16_t,
                    temp[((MAX_SB_SIZE + 16) + 16) * MAX_SB_SIZE]);
    const int16_t *const kernel_x =
        av1_get_interp_filter_subpel_kernel(filter, subpel_x_q3 << 1);
    const int16_t *const kernel_y =
        av1_get_interp_filter_subpel_kernel(filter, subpel_y_q3 << 1);
    const int intermediate_height =
        (((height - 1) * 8 + subpel_y_q3) >> 3) + filter->taps;
    assert(intermediate_height <= (MAX_SB_SIZE * 2 + 16) + 16);
    aom_highbd_convolve8_horiz_c(ref8 - ref_stride * ((filter->taps >> 1) - 1),
                                 ref_stride, CONVERT_TO_BYTEPTR(temp),
                                 MAX_SB_SIZE, kernel_x, 16, NULL, -1, width,
                                 intermediate_height, bd);
    aom_highbd_convolve8_vert_c(
        CONVERT_TO_BYTEPTR(temp + MAX_SB_SIZE * ((filter->taps >> 1) - 1)),
        MAX_SB_SIZE, comp_pred8, width, NULL, -1, kernel_y, 16, width, height,
        bd);
  }
}

void aom_highbd_comp_avg_upsampled_pred_c(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred8, const uint8_t *pred8, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8,
    int ref_stride, int bd, int subpel_search) {
  int i, j;

  const uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
  aom_highbd_upsampled_pred(xd, cm, mi_row, mi_col, mv, comp_pred8, width,
                            height, subpel_x_q3, subpel_y_q3, ref8, ref_stride,
                            bd, subpel_search);
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      comp_pred[j] = ROUND_POWER_OF_TWO(pred[j] + comp_pred[j], 1);
    }
    comp_pred += width;
    pred += width;
  }
}

void aom_highbd_dist_wtd_comp_avg_pred_c(
    uint8_t *comp_pred8, const uint8_t *pred8, int width, int height,
    const uint8_t *ref8, int ref_stride,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  int i, j;
  const int fwd_offset = jcp_param->fwd_offset;
  const int bck_offset = jcp_param->bck_offset;
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);

  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      int tmp = pred[j] * bck_offset + ref[j] * fwd_offset;
      tmp = ROUND_POWER_OF_TWO(tmp, DIST_PRECISION_BITS);
      comp_pred[j] = (uint16_t)tmp;
    }
    comp_pred += width;
    pred += width;
    ref += ref_stride;
  }
}

void aom_highbd_dist_wtd_comp_avg_upsampled_pred_c(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred8, const uint8_t *pred8, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8,
    int ref_stride, int bd, const DIST_WTD_COMP_PARAMS *jcp_param,
    int subpel_search) {
  int i, j;
  const int fwd_offset = jcp_param->fwd_offset;
  const int bck_offset = jcp_param->bck_offset;
  const uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
  aom_highbd_upsampled_pred_c(xd, cm, mi_row, mi_col, mv, comp_pred8, width,
                              height, subpel_x_q3, subpel_y_q3, ref8,
                              ref_stride, bd, subpel_search);

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      int tmp = pred[j] * bck_offset + comp_pred[j] * fwd_offset;
      tmp = ROUND_POWER_OF_TWO(tmp, DIST_PRECISION_BITS);
      comp_pred[j] = (uint16_t)tmp;
    }
    comp_pred += width;
    pred += width;
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

void aom_comp_mask_pred_c(uint8_t *comp_pred, const uint8_t *pred, int width,
                          int height, const uint8_t *ref, int ref_stride,
                          const uint8_t *mask, int mask_stride,
                          int invert_mask) {
  int i, j;
  const uint8_t *src0 = invert_mask ? pred : ref;
  const uint8_t *src1 = invert_mask ? ref : pred;
  const int stride0 = invert_mask ? width : ref_stride;
  const int stride1 = invert_mask ? ref_stride : width;
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      comp_pred[j] = AOM_BLEND_A64(mask[j], src0[j], src1[j]);
    }
    comp_pred += width;
    src0 += stride0;
    src1 += stride1;
    mask += mask_stride;
  }
}

void aom_comp_mask_upsampled_pred_c(MACROBLOCKD *xd, const AV1_COMMON *const cm,
                                    int mi_row, int mi_col, const MV *const mv,
                                    uint8_t *comp_pred, const uint8_t *pred,
                                    int width, int height, int subpel_x_q3,
                                    int subpel_y_q3, const uint8_t *ref,
                                    int ref_stride, const uint8_t *mask,
                                    int mask_stride, int invert_mask,
                                    int subpel_search) {
  if (subpel_x_q3 | subpel_y_q3) {
    aom_upsampled_pred_c(xd, cm, mi_row, mi_col, mv, comp_pred, width, height,
                         subpel_x_q3, subpel_y_q3, ref, ref_stride,
                         subpel_search);
    ref = comp_pred;
    ref_stride = width;
  }
  aom_comp_mask_pred_c(comp_pred, pred, width, height, ref, ref_stride, mask,
                       mask_stride, invert_mask);
}

#define MASK_SUBPIX_VAR(W, H)                                                  \
  unsigned int aom_masked_sub_pixel_variance##W##x##H##_c(                     \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,            \
      const uint8_t *ref, int ref_stride, const uint8_t *second_pred,          \
      const uint8_t *msk, int msk_stride, int invert_mask,                     \
      unsigned int *sse) {                                                     \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint8_t temp2[H * W];                                                      \
    DECLARE_ALIGNED(16, uint8_t, temp3[H * W]);                                \
                                                                               \
    aom_var_filter_block2d_bil_first_pass_c(src, fdata3, src_stride, 1, H + 1, \
                                            W, bilinear_filters_2t[xoffset]);  \
    aom_var_filter_block2d_bil_second_pass_c(fdata3, temp2, W, W, H, W,        \
                                             bilinear_filters_2t[yoffset]);    \
                                                                               \
    aom_comp_mask_pred_c(temp3, second_pred, W, H, temp2, W, msk, msk_stride,  \
                         invert_mask);                                         \
    return aom_variance##W##x##H##_c(temp3, W, ref, ref_stride, sse);          \
  }

MASK_SUBPIX_VAR(4, 4)
MASK_SUBPIX_VAR(4, 8)
MASK_SUBPIX_VAR(8, 4)
MASK_SUBPIX_VAR(8, 8)
MASK_SUBPIX_VAR(8, 16)
MASK_SUBPIX_VAR(16, 8)
MASK_SUBPIX_VAR(16, 16)
MASK_SUBPIX_VAR(16, 32)
MASK_SUBPIX_VAR(32, 16)
MASK_SUBPIX_VAR(32, 32)
MASK_SUBPIX_VAR(32, 64)
MASK_SUBPIX_VAR(64, 32)
MASK_SUBPIX_VAR(64, 64)
MASK_SUBPIX_VAR(64, 128)
MASK_SUBPIX_VAR(128, 64)
MASK_SUBPIX_VAR(128, 128)
MASK_SUBPIX_VAR(4, 16)
MASK_SUBPIX_VAR(16, 4)
MASK_SUBPIX_VAR(8, 32)
MASK_SUBPIX_VAR(32, 8)
MASK_SUBPIX_VAR(16, 64)
MASK_SUBPIX_VAR(64, 16)

#if CONFIG_AV1_HIGHBITDEPTH
void aom_highbd_comp_mask_pred_c(uint8_t *comp_pred8, const uint8_t *pred8,
                                 int width, int height, const uint8_t *ref8,
                                 int ref_stride, const uint8_t *mask,
                                 int mask_stride, int invert_mask) {
  int i, j;
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      if (!invert_mask)
        comp_pred[j] = AOM_BLEND_A64(mask[j], ref[j], pred[j]);
      else
        comp_pred[j] = AOM_BLEND_A64(mask[j], pred[j], ref[j]);
    }
    comp_pred += width;
    pred += width;
    ref += ref_stride;
    mask += mask_stride;
  }
}

void aom_highbd_comp_mask_upsampled_pred(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred8, const uint8_t *pred8, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8,
    int ref_stride, const uint8_t *mask, int mask_stride, int invert_mask,
    int bd, int subpel_search) {
  aom_highbd_upsampled_pred(xd, cm, mi_row, mi_col, mv, comp_pred8, width,
                            height, subpel_x_q3, subpel_y_q3, ref8, ref_stride,
                            bd, subpel_search);
  aom_highbd_comp_mask_pred(comp_pred8, pred8, width, height, comp_pred8, width,
                            mask, mask_stride, invert_mask);
}

#define HIGHBD_MASK_SUBPIX_VAR(W, H)                                           \
  unsigned int aom_highbd_8_masked_sub_pixel_variance##W##x##H##_c(            \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,            \
      const uint8_t *ref, int ref_stride, const uint8_t *second_pred,          \
      const uint8_t *msk, int msk_stride, int invert_mask,                     \
      unsigned int *sse) {                                                     \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint16_t temp2[H * W];                                                     \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                               \
                                                                               \
    aom_highbd_var_filter_block2d_bil_first_pass(                              \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);   \
    aom_highbd_var_filter_block2d_bil_second_pass(                             \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);              \
                                                                               \
    aom_highbd_comp_mask_pred_c(CONVERT_TO_BYTEPTR(temp3), second_pred, W, H,  \
                                CONVERT_TO_BYTEPTR(temp2), W, msk, msk_stride, \
                                invert_mask);                                  \
                                                                               \
    return aom_highbd_8_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp3), W,    \
                                              ref, ref_stride, sse);           \
  }                                                                            \
                                                                               \
  unsigned int aom_highbd_10_masked_sub_pixel_variance##W##x##H##_c(           \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,            \
      const uint8_t *ref, int ref_stride, const uint8_t *second_pred,          \
      const uint8_t *msk, int msk_stride, int invert_mask,                     \
      unsigned int *sse) {                                                     \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint16_t temp2[H * W];                                                     \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                               \
                                                                               \
    aom_highbd_var_filter_block2d_bil_first_pass(                              \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);   \
    aom_highbd_var_filter_block2d_bil_second_pass(                             \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);              \
                                                                               \
    aom_highbd_comp_mask_pred_c(CONVERT_TO_BYTEPTR(temp3), second_pred, W, H,  \
                                CONVERT_TO_BYTEPTR(temp2), W, msk, msk_stride, \
                                invert_mask);                                  \
                                                                               \
    return aom_highbd_10_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp3), W,   \
                                               ref, ref_stride, sse);          \
  }                                                                            \
                                                                               \
  unsigned int aom_highbd_12_masked_sub_pixel_variance##W##x##H##_c(           \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,            \
      const uint8_t *ref, int ref_stride, const uint8_t *second_pred,          \
      const uint8_t *msk, int msk_stride, int invert_mask,                     \
      unsigned int *sse) {                                                     \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint16_t temp2[H * W];                                                     \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                               \
                                                                               \
    aom_highbd_var_filter_block2d_bil_first_pass(                              \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);   \
    aom_highbd_var_filter_block2d_bil_second_pass(                             \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);              \
                                                                               \
    aom_highbd_comp_mask_pred_c(CONVERT_TO_BYTEPTR(temp3), second_pred, W, H,  \
                                CONVERT_TO_BYTEPTR(temp2), W, msk, msk_stride, \
                                invert_mask);                                  \
                                                                               \
    return aom_highbd_12_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp3), W,   \
                                               ref, ref_stride, sse);          \
  }

HIGHBD_MASK_SUBPIX_VAR(4, 4)
HIGHBD_MASK_SUBPIX_VAR(4, 8)
HIGHBD_MASK_SUBPIX_VAR(8, 4)
HIGHBD_MASK_SUBPIX_VAR(8, 8)
HIGHBD_MASK_SUBPIX_VAR(8, 16)
HIGHBD_MASK_SUBPIX_VAR(16, 8)
HIGHBD_MASK_SUBPIX_VAR(16, 16)
HIGHBD_MASK_SUBPIX_VAR(16, 32)
HIGHBD_MASK_SUBPIX_VAR(32, 16)
HIGHBD_MASK_SUBPIX_VAR(32, 32)
HIGHBD_MASK_SUBPIX_VAR(32, 64)
HIGHBD_MASK_SUBPIX_VAR(64, 32)
HIGHBD_MASK_SUBPIX_VAR(64, 64)
HIGHBD_MASK_SUBPIX_VAR(64, 128)
HIGHBD_MASK_SUBPIX_VAR(128, 64)
HIGHBD_MASK_SUBPIX_VAR(128, 128)
HIGHBD_MASK_SUBPIX_VAR(4, 16)
HIGHBD_MASK_SUBPIX_VAR(16, 4)
HIGHBD_MASK_SUBPIX_VAR(8, 32)
HIGHBD_MASK_SUBPIX_VAR(32, 8)
HIGHBD_MASK_SUBPIX_VAR(16, 64)
HIGHBD_MASK_SUBPIX_VAR(64, 16)
#endif  // CONFIG_AV1_HIGHBITDEPTH

static INLINE void obmc_variance(const uint8_t *pre, int pre_stride,
                                 const int32_t *wsrc, const int32_t *mask,
                                 int w, int h, unsigned int *sse, int *sum) {
  int i, j;

  *sse = 0;
  *sum = 0;

  for (i = 0; i < h; i++) {
    for (j = 0; j < w; j++) {
      int diff = ROUND_POWER_OF_TWO_SIGNED(wsrc[j] - pre[j] * mask[j], 12);
      *sum += diff;
      *sse += diff * diff;
    }

    pre += pre_stride;
    wsrc += w;
    mask += w;
  }
}

#define OBMC_VAR(W, H)                                            \
  unsigned int aom_obmc_variance##W##x##H##_c(                    \
      const uint8_t *pre, int pre_stride, const int32_t *wsrc,    \
      const int32_t *mask, unsigned int *sse) {                   \
    int sum;                                                      \
    obmc_variance(pre, pre_stride, wsrc, mask, W, H, sse, &sum);  \
    return *sse - (unsigned int)(((int64_t)sum * sum) / (W * H)); \
  }

#define OBMC_SUBPIX_VAR(W, H)                                                  \
  unsigned int aom_obmc_sub_pixel_variance##W##x##H##_c(                       \
      const uint8_t *pre, int pre_stride, int xoffset, int yoffset,            \
      const int32_t *wsrc, const int32_t *mask, unsigned int *sse) {           \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint8_t temp2[H * W];                                                      \
                                                                               \
    aom_var_filter_block2d_bil_first_pass_c(pre, fdata3, pre_stride, 1, H + 1, \
                                            W, bilinear_filters_2t[xoffset]);  \
    aom_var_filter_block2d_bil_second_pass_c(fdata3, temp2, W, W, H, W,        \
                                             bilinear_filters_2t[yoffset]);    \
                                                                               \
    return aom_obmc_variance##W##x##H##_c(temp2, W, wsrc, mask, sse);          \
  }

OBMC_VAR(4, 4)
OBMC_SUBPIX_VAR(4, 4)

OBMC_VAR(4, 8)
OBMC_SUBPIX_VAR(4, 8)

OBMC_VAR(8, 4)
OBMC_SUBPIX_VAR(8, 4)

OBMC_VAR(8, 8)
OBMC_SUBPIX_VAR(8, 8)

OBMC_VAR(8, 16)
OBMC_SUBPIX_VAR(8, 16)

OBMC_VAR(16, 8)
OBMC_SUBPIX_VAR(16, 8)

OBMC_VAR(16, 16)
OBMC_SUBPIX_VAR(16, 16)

OBMC_VAR(16, 32)
OBMC_SUBPIX_VAR(16, 32)

OBMC_VAR(32, 16)
OBMC_SUBPIX_VAR(32, 16)

OBMC_VAR(32, 32)
OBMC_SUBPIX_VAR(32, 32)

OBMC_VAR(32, 64)
OBMC_SUBPIX_VAR(32, 64)

OBMC_VAR(64, 32)
OBMC_SUBPIX_VAR(64, 32)

OBMC_VAR(64, 64)
OBMC_SUBPIX_VAR(64, 64)

OBMC_VAR(64, 128)
OBMC_SUBPIX_VAR(64, 128)

OBMC_VAR(128, 64)
OBMC_SUBPIX_VAR(128, 64)

OBMC_VAR(128, 128)
OBMC_SUBPIX_VAR(128, 128)

OBMC_VAR(4, 16)
OBMC_SUBPIX_VAR(4, 16)
OBMC_VAR(16, 4)
OBMC_SUBPIX_VAR(16, 4)
OBMC_VAR(8, 32)
OBMC_SUBPIX_VAR(8, 32)
OBMC_VAR(32, 8)
OBMC_SUBPIX_VAR(32, 8)
OBMC_VAR(16, 64)
OBMC_SUBPIX_VAR(16, 64)
OBMC_VAR(64, 16)
OBMC_SUBPIX_VAR(64, 16)

#if CONFIG_AV1_HIGHBITDEPTH
static INLINE void highbd_obmc_variance64(const uint8_t *pre8, int pre_stride,
                                          const int32_t *wsrc,
                                          const int32_t *mask, int w, int h,
                                          uint64_t *sse, int64_t *sum) {
  int i, j;
  uint16_t *pre = CONVERT_TO_SHORTPTR(pre8);

  *sse = 0;
  *sum = 0;

  for (i = 0; i < h; i++) {
    for (j = 0; j < w; j++) {
      int diff = ROUND_POWER_OF_TWO_SIGNED(wsrc[j] - pre[j] * mask[j], 12);
      *sum += diff;
      *sse += diff * diff;
    }

    pre += pre_stride;
    wsrc += w;
    mask += w;
  }
}

static INLINE void highbd_obmc_variance(const uint8_t *pre8, int pre_stride,
                                        const int32_t *wsrc,
                                        const int32_t *mask, int w, int h,
                                        unsigned int *sse, int *sum) {
  int64_t sum64;
  uint64_t sse64;
  highbd_obmc_variance64(pre8, pre_stride, wsrc, mask, w, h, &sse64, &sum64);
  *sum = (int)sum64;
  *sse = (unsigned int)sse64;
}

static INLINE void highbd_10_obmc_variance(const uint8_t *pre8, int pre_stride,
                                           const int32_t *wsrc,
                                           const int32_t *mask, int w, int h,
                                           unsigned int *sse, int *sum) {
  int64_t sum64;
  uint64_t sse64;
  highbd_obmc_variance64(pre8, pre_stride, wsrc, mask, w, h, &sse64, &sum64);
  *sum = (int)ROUND_POWER_OF_TWO(sum64, 2);
  *sse = (unsigned int)ROUND_POWER_OF_TWO(sse64, 4);
}

static INLINE void highbd_12_obmc_variance(const uint8_t *pre8, int pre_stride,
                                           const int32_t *wsrc,
                                           const int32_t *mask, int w, int h,
                                           unsigned int *sse, int *sum) {
  int64_t sum64;
  uint64_t sse64;
  highbd_obmc_variance64(pre8, pre_stride, wsrc, mask, w, h, &sse64, &sum64);
  *sum = (int)ROUND_POWER_OF_TWO(sum64, 4);
  *sse = (unsigned int)ROUND_POWER_OF_TWO(sse64, 8);
}

#define HIGHBD_OBMC_VAR(W, H)                                              \
  unsigned int aom_highbd_obmc_variance##W##x##H##_c(                      \
      const uint8_t *pre, int pre_stride, const int32_t *wsrc,             \
      const int32_t *mask, unsigned int *sse) {                            \
    int sum;                                                               \
    highbd_obmc_variance(pre, pre_stride, wsrc, mask, W, H, sse, &sum);    \
    return *sse - (unsigned int)(((int64_t)sum * sum) / (W * H));          \
  }                                                                        \
                                                                           \
  unsigned int aom_highbd_10_obmc_variance##W##x##H##_c(                   \
      const uint8_t *pre, int pre_stride, const int32_t *wsrc,             \
      const int32_t *mask, unsigned int *sse) {                            \
    int sum;                                                               \
    int64_t var;                                                           \
    highbd_10_obmc_variance(pre, pre_stride, wsrc, mask, W, H, sse, &sum); \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (W * H));              \
    return (var >= 0) ? (uint32_t)var : 0;                                 \
  }                                                                        \
                                                                           \
  unsigned int aom_highbd_12_obmc_variance##W##x##H##_c(                   \
      const uint8_t *pre, int pre_stride, const int32_t *wsrc,             \
      const int32_t *mask, unsigned int *sse) {                            \
    int sum;                                                               \
    int64_t var;                                                           \
    highbd_12_obmc_variance(pre, pre_stride, wsrc, mask, W, H, sse, &sum); \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (W * H));              \
    return (var >= 0) ? (uint32_t)var : 0;                                 \
  }

#define HIGHBD_OBMC_SUBPIX_VAR(W, H)                                           \
  unsigned int aom_highbd_obmc_sub_pixel_variance##W##x##H##_c(                \
      const uint8_t *pre, int pre_stride, int xoffset, int yoffset,            \
      const int32_t *wsrc, const int32_t *mask, unsigned int *sse) {           \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint16_t temp2[H * W];                                                     \
                                                                               \
    aom_highbd_var_filter_block2d_bil_first_pass(                              \
        pre, fdata3, pre_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);   \
    aom_highbd_var_filter_block2d_bil_second_pass(                             \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);              \
                                                                               \
    return aom_highbd_obmc_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp2), W, \
                                                 wsrc, mask, sse);             \
  }                                                                            \
                                                                               \
  unsigned int aom_highbd_10_obmc_sub_pixel_variance##W##x##H##_c(             \
      const uint8_t *pre, int pre_stride, int xoffset, int yoffset,            \
      const int32_t *wsrc, const int32_t *mask, unsigned int *sse) {           \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint16_t temp2[H * W];                                                     \
                                                                               \
    aom_highbd_var_filter_block2d_bil_first_pass(                              \
        pre, fdata3, pre_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);   \
    aom_highbd_var_filter_block2d_bil_second_pass(                             \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);              \
                                                                               \
    return aom_highbd_10_obmc_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp2), \
                                                    W, wsrc, mask, sse);       \
  }                                                                            \
                                                                               \
  unsigned int aom_highbd_12_obmc_sub_pixel_variance##W##x##H##_c(             \
      const uint8_t *pre, int pre_stride, int xoffset, int yoffset,            \
      const int32_t *wsrc, const int32_t *mask, unsigned int *sse) {           \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint16_t temp2[H * W];                                                     \
                                                                               \
    aom_highbd_var_filter_block2d_bil_first_pass(                              \
        pre, fdata3, pre_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);   \
    aom_highbd_var_filter_block2d_bil_second_pass(                             \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);              \
                                                                               \
    return aom_highbd_12_obmc_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp2), \
                                                    W, wsrc, mask, sse);       \
  }

HIGHBD_OBMC_VAR(4, 4)
HIGHBD_OBMC_SUBPIX_VAR(4, 4)

HIGHBD_OBMC_VAR(4, 8)
HIGHBD_OBMC_SUBPIX_VAR(4, 8)

HIGHBD_OBMC_VAR(8, 4)
HIGHBD_OBMC_SUBPIX_VAR(8, 4)

HIGHBD_OBMC_VAR(8, 8)
HIGHBD_OBMC_SUBPIX_VAR(8, 8)

HIGHBD_OBMC_VAR(8, 16)
HIGHBD_OBMC_SUBPIX_VAR(8, 16)

HIGHBD_OBMC_VAR(16, 8)
HIGHBD_OBMC_SUBPIX_VAR(16, 8)

HIGHBD_OBMC_VAR(16, 16)
HIGHBD_OBMC_SUBPIX_VAR(16, 16)

HIGHBD_OBMC_VAR(16, 32)
HIGHBD_OBMC_SUBPIX_VAR(16, 32)

HIGHBD_OBMC_VAR(32, 16)
HIGHBD_OBMC_SUBPIX_VAR(32, 16)

HIGHBD_OBMC_VAR(32, 32)
HIGHBD_OBMC_SUBPIX_VAR(32, 32)

HIGHBD_OBMC_VAR(32, 64)
HIGHBD_OBMC_SUBPIX_VAR(32, 64)

HIGHBD_OBMC_VAR(64, 32)
HIGHBD_OBMC_SUBPIX_VAR(64, 32)

HIGHBD_OBMC_VAR(64, 64)
HIGHBD_OBMC_SUBPIX_VAR(64, 64)

HIGHBD_OBMC_VAR(64, 128)
HIGHBD_OBMC_SUBPIX_VAR(64, 128)

HIGHBD_OBMC_VAR(128, 64)
HIGHBD_OBMC_SUBPIX_VAR(128, 64)

HIGHBD_OBMC_VAR(128, 128)
HIGHBD_OBMC_SUBPIX_VAR(128, 128)

HIGHBD_OBMC_VAR(4, 16)
HIGHBD_OBMC_SUBPIX_VAR(4, 16)
HIGHBD_OBMC_VAR(16, 4)
HIGHBD_OBMC_SUBPIX_VAR(16, 4)
HIGHBD_OBMC_VAR(8, 32)
HIGHBD_OBMC_SUBPIX_VAR(8, 32)
HIGHBD_OBMC_VAR(32, 8)
HIGHBD_OBMC_SUBPIX_VAR(32, 8)
HIGHBD_OBMC_VAR(16, 64)
HIGHBD_OBMC_SUBPIX_VAR(16, 64)
HIGHBD_OBMC_VAR(64, 16)
HIGHBD_OBMC_SUBPIX_VAR(64, 16)
#endif  // CONFIG_AV1_HIGHBITDEPTH
