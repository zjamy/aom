#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "aom/aom_integer.h"
#include "aom_ports/msvc.h"
#include "depthinput.h"

static const char *IVF_SIGNATURE = "DKIF";
static void fix_framerate(int *num, int *den) {
  if (*den <= 0 || *den >= 1000000000 || *num <= 0 || *num >= 1000) {
    // framerate seems to be invalid, just default to 30fps.
    *num = 30;
    *den = 1;
  }
}

int depth_input_open(depth_input_t *_depth, FILE *_fin) {
  char raw_buf[32];
  int is_ivf = 0;

  if (fread(raw_buf, 1, 32, _fin) == 32) {
    if (memcmp(IVF_SIGNATURE, raw_buf, 4) == 0) {
      is_ivf = 1;

      if (mem_get_le16(raw_buf + 4) != 1) {
        fprintf(stderr, "Error: Unrecognized IVF version! This file may not"
                        " decode properly.");
      }

      _depth->fourcc = mem_get_le32(raw_buf + 8);
      _depth->dw = mem_get_le16(raw_buf + 12);
      _depth->dh = mem_get_le16(raw_buf + 14);
      _depth->fps_rate = mem_get_le32(raw_buf + 16);
      _depth->fps_scale = mem_get_le32(raw_buf + 20);
      _depth->frame_cnt = mem_get_le32(raw_buf + 24);
      _depth->depth_scaler = mem_get_le32(raw_buf + 28);
      fix_framerate(&_depth->fps_rate, &_depth->fps_scale);
      // ToDo: parse depth_scaler
      if (_depth->fourcc == D16_FOURCC) {
        _depth->bit_depth = 16;
      } else if (_depth->fourcc == D8_FOURCC) {
        _depth->bit_depth = 8;
      }
    }
  }

  if (is_ivf) {
    char header[12];
    if (fread(header, 1, 12, _fin) == 12) {
      _depth->dimg.framesize = mem_get_le32(header);
      _depth->dimg.pts = mem_get_le32(&header[4]);
      _depth->dimg.pts += ((int64_t)mem_get_le32(&raw_header[8]) << 32);
    }
    //_depth->dimg.framesize should be aligned with _depth->bit_depth/8 *
    //_depth->dw * _depth->dh;
    _depth->dimg.buf = (unsigned char *)malloc(_depth->dimg.framesize);
  } else {
    return 0;
  }
  return (_depth->dimg.buf != NULL) ? is_ivf : 0;
}

// needs to handle framesize and buffersize well
void depth_input_close(depth_input_t *_depth) { free(_depth->dimg.buf); }

int depth_input_fetch_frame(depth_input_t *_depth, FILE *_fin) {
  char header[12];
  if (fread(header, 1, 12, _fin) == 12) {
    _depth->dimg.framesize = mem_get_le32(header);
    _depth->dimg.pts = mem_get_le32(&header[4]);
    _depth->dimg.pts += ((int64_t)mem_get_le32(&raw_header[8]) << 32);
  }

  if (!feof(_fin)) {
    // didn't consider buffer alignment in this moment
    if (fread(_depth->dimg.buf, 1, _depth->dimg.framesize, _fin) !=
        _depth->dimg.framesize) {
      warn("Failed to read full frame");
      return 1;
    }

    return 0;
  }
}
