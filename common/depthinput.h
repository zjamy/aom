#ifndef AOM_COMMON_DEPTHINPUT_H_
#define AOM_COMMON_DEPTHNPUT_H_

#include "aom/aom_image.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define D16_FOURCC xxxxx
#define D8_FOURCC xxxx

typedef struct depth_image {
  int framesize;
  int64_t pts;
  unsigned char *buf;
  // do we need pitch?
} depth_image_t;

typedef struct depth_input {
  unsigned int fourcc;
  int dw;
  int dh;
  int fps_rate;
  int fps_scale;
  int frame_cnt;
  unsigned int depth_scaler;
  unsigned int bit_depth;
  depth_image_t dimg; // needs refinement, we may need multi
                      // frames
} depth_input_t;

int depth_input_open(depth_input_t *_depth, FILE *_fin);
void depth_input_close(depth_input_t *_depth);
int depth_input_fetch_frame(depth_input_t *_depth, FILE *_fin,
                            depth_image_t *img_d);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // AOM_COMMON_DEPTHINPUT_H_
