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

#include "common/depthenc.h"

#include "aom/aom_encoder.h"
#include "aom_ports/mem_ops.h"

void depth_write_file_header(FILE *outfile, const depth_cfg_t *dcfg,
                             int frame_cnt) {
  char header[32];

  header[0] = 'D';
  header[1] = 'K';
  header[2] = 'I';
  header[3] = 'F';
  mem_put_le16(header + 4, 1);  // version
  mem_put_le16(header + 6, 32); // header size
  mem_put_le32(
      header + 8,
      dcfg->fourcc); // fourcc D16 means 16bit depth; D8 means 8bit depth
  mem_put_le16(header + 12, dcfg->frame_wdith);  // width
  mem_put_le16(header + 14, dcfg->frame_height); // height
  mem_put_le32(header + 16, dcfg->rate);         // rate
  mem_put_le32(header + 20, dcfg->ratescale);    // scale
  mem_put_le32(header + 24, frame_cnt);          // length
  // we need to find a way to pass the depth scaler, maybe some time define is
  // required
  mem_put_le32(header + 28, dcfg->depth_scaler); // depth scaler

  fwrite(header, 1, 32, outfile);
}

void depth_write_frame_header(FILE *outfile, int64_t pts, size_t frame_size) {
  char header[12];

  mem_put_le32(header, (int)frame_size);
  mem_put_le32(header + 4, (int)(pts & 0xFFFFFFFF));
  mem_put_le32(header + 8, (int)(pts >> 32));
  fwrite(header, 1, 12, outfile);
}

void depth_write_frame_data(void *outfile, const char *buffer,
                            unsigned int width, unsigned int height,
                            unsigned int bit_depth) {
  // do we need some pitch or alignment?
  const unsigned int stride = width * bit_depth;
  for (int y = 0; y < h; ++y) {
    fwrite(buffer, bit_depth / 8, width, (FILE *)outputfile);
    buf += stride;
  }
}

void depth_img_alloc(depth_image_t *dimg, const depth_cfg_t *dcfg) {
  dimg->buf = (unsigned char *)malloc(dcfg->bit_depth / 8 * dcfg->frame_wdith *
                                      dcfg->frame_height);
  dimg->bw = dcfg->frame_wdith;
  dimg->bh = dcfg->frame_height;
  dimg->bdepth = dcfg->bit_depth;
}

void depth_img_free(depth_image_t *dimg) { free(dimg); }

int depth_img_read(depth_image_t *dimg, FILE *infile) {
  unsigned char *buf = dimg->buf;
  const int stride = dimg->bw * dimg->bdepth / 8;
  const int w = dimg->bw * dimg->bdepth / 8;
  const int h = dimg->bh;
  int y;

  for (y = 0; y < h; ++y) {
    if (fread(buf, 1, w, file) != (size_t)w)
      return 0;
    buf += stride;
  }
}

void depth_write_frame(void *outputfile, const uint16_t *buffer,
                       unsigned int width, unsigned int height, int bit_depth) {
  int y;
  for (y = 0; y < h; ++y) {
    fwrite(buf, 1, w, file);
    buf += stride;
  }
}

