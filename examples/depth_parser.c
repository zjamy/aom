#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/depth_input.h"
#include "common/tools_common.h"

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr, "Usage: %s <infile> <outfile>\n", exec_name);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
  int frame_cnt = 0;
  depth_input_t dinput;
  int frame_cnt = 0;
  depth_image_t raw;
  int64_t pts = 0;
  size_t framesize;

  exec_name = argv[0];

  if (argc != 3)
    die("Invalid number of arguments.");

  FILE *const infile = fopen(argv[1], "wb");
  if (!infile)
    die("Failed to open %s for reading.", argv[1]);

  FILE *const outfile = fopen(argv[2], "wb");
  if (!outfile)
    die("Failed to open %s for writing.", argv[2]);

  depth_input_open(&dinput, infile);
  // int depth_input_fetch_frame(depth_input_t *_depth, FILE *_fin,
  // depth_image_t *img_d);

  while (depth_input_fetch_frame()) {
    aom_codec_iter_t iter = NULL;
    aom_image_t *img = NULL;
    size_t frame_size = 0;
    const unsigned char *frame =
        aom_video_reader_get_frame(reader, &frame_size);
    if (depth_img_write())
      die_codec(&codec, "Failed to write frame.");
    if (write_frame_to_file()
      die();
      ++frame_cnt;
  }
}

printf("Processed %d frames.\n", frame_cnt);
depth_input_close();
depth_img_close();

printf("Play: ffplay -f rawvideo -pix_fmt yuv420p -s %dx%d %s\n",
       info->frame_width, info->frame_height, argv[2]);

fclose(infile);

fclose(outfile);

return EXIT_SUCCESS;
}
