#ifndef INC_BITMAP_IMAGE_H_
#define INC_BITMAP_IMAGE_H_

#include <stdint.h>

typedef enum
{
  BITMAP_FORMAT_BMP4 = 0,
  BITMAP_FORMAT_RGB565_RAW = 1
} BitmapFormat;

typedef struct
{
  BitmapFormat format;
  uint16_t width;
  uint16_t height;
  const uint8_t *data;
  uint32_t data_len;
} BitmapImage;

#endif
