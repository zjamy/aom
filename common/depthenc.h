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
#ifndef AOM_COMMON_DEPTHENC_H_
#define AOM_COMMON_DEPTHENC_H_

#include "common/tools_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct depth_enc_cfg {
  uint32_t fourcc;
  uint32_t frame_width;
  uint32_t frame_height;
  uint32_t rate;
  uint32_t ratescale;
  uint32_t depth_scaler; // needs better define
  uint32_t bit_depth;
} depth_cfg_t;

typedef struct depth_image {
  unsigned char *buf;
  uint32_t bw;
  uint32_t bh;
  uint32_t bdepth;
} depth_image_t;

void depth_write_file_header(FILE *outfile, const depth_cfg_t *dcfg,
                             int frame_cnt);

void depth_write_frame_header(FILE *outfile, int64_t pts, size_t frame_size);

void depth_write_frame_size(FILE *outfile, size_t frame_size);

void depth_write_frame(void *outputfile, const uint16_t *buffer,
                       unsigned int width, unsigned int height, int bit_depth);

void depth_img_alloc(depth_image_t *dimg, const depth_cfg_t *dcfg);

void depth_img_free(depth_image_t *dimg);

int depth_img_read(depth_image_t *dimg, FILE *infile);
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // AOM_COMMON_DEPTHENC_H_
