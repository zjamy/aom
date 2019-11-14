#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/depthenc.h"
#include "common/depthinput.h"
#include "common/tools_common.h"
#include "common/video_writer.h"

static const char *exec_name;

void usage_exit(void) {
  fprintf(
      stderr,
      "Usage: %s <D16/D8> <depthscaler> <width> <height> <infile> <outfile> "
      "See comments in depth_encoder.c for more information. "
      "now it's more like a container\n",
      exec_name);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
  FILE *infile = NULL;
  aom_codec_ctx_t codec;
  depth_cfg_t dcfg;
  int frame_cnt = 0;
  depth_image_t raw;
  int64_t pts = 0;
  size_t framesize;
  const int fps = 30;
  const char *fourcc_arg = NULL;
  const char *scaler_arg = NULL;
  const char *width_arg = NULL;
  const char *height_arg = NULL;
  const char *infile_arg = NULL;
  const char *outfile_arg = NULL;
  const char *framecount_arg = NULL;

  exec_name = argv[0];

  memset(&dcfg, 0, sizeof(dcfg));

  if (argc != 7)
    die("Invalid number of arguments");

  fourcc_arg = argv[1];
  scaler_arg = argv[2];
  width_arg = argv[3];
  height_arg = argv[4];
  infile_arg = argv[5];
  outfile_arg = argv[6];
  framecount_arg = argv[7];

  if (strcmp(fourcc_arg, "D16") == 0) {
    dcfg.codec_fourcc = D16_FOURCC;
    dcfg.bit_depth = 16;
  } else if (strcmp(fourcc_arg, "D8") == 0) {
    dcfg.codec_fourcc = D8_FOURCC;
    dcfg.bit_depth = 8;
  }
  dcfg.frame_width = (int)strtol(width_arg, NULL, 0);
  dcfg.frame_height = (int)strtol(height_arg, NULL, 0);
  dcfg.depth_scaler = (int)strtol(scaler_arg, NULL, 0);
  dcfg.ratescale = 1;
  dcfg.rate = fps;
  frame_cnt = (int)strtol(framecount_arg, NULL, 0);
  framesize = dcfg.frame_width * dcfg.frame_height * dcfg.bit_depth /
              8

              if (dcfg.frame_width <= 0 || dcfg.frame_height <= 0 ||
                  (dcfg.frame_width % 2) != 0 || (dcfg.frame_height % 2) != 0) {
    die("Invalid frame size: %dx%d", dcfg.frame_width, dcfg.frame_height);
  }

  FILE *const infile = fopen(infile_arg, "wb");
  if (!infile)
    return NULL;
  FILE *const outfile = fopen(outfile_arg, "wb");
  if (!outfile)
    return NULL;

  if (!depth_img_alloc(&raw, dcfg)) {
    die("Failed to allocate image.");
  }

  depth_write_file_header(outfile, dcfg, frame_cnt);

  // prepare frames.
  while (depth_img_read(&raw, infile)) {
    depth_write_frame_header(outfile, pts, framesize);
    depth_write_frame(outfile, &raw);
    pts++; //pts needs better update
    if (frame_cnt > 0 && pts >= frame_cnt)
      break;
  }

  printf("\n");
  printf("Processed %d frames.\n", pts);
  fclose(infile);
  fclose(outfile);
  depth_img_free(&raw);

  return EXIT_SUCCESS;
}
