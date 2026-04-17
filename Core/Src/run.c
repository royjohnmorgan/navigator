#include "run.h"
#include "test_bmp4_data.h"

#define TFT_WIDTH                320U
#define TFT_HEIGHT               240U

/*
 * Control pins for the SPI display module.
 * PA0 -> D/C
 * PA1 -> RST
 */
#define TFT_DC_PORT              GPIOB
#define TFT_DC_PIN               GPIO_PIN_9
#define TFT_RST_PORT             GPIOA
#define TFT_RST_PIN              GPIO_PIN_8
#define TFT_LGHT_PORT             GPIOB
#define TFT_LGHT_PIN              GPIO_PIN_8

#define ILI9341_SWRESET          0x01U
#define ILI9341_SLPOUT           0x11U
#define ILI9341_DISPON           0x29U
#define ILI9341_CASET            0x2AU
#define ILI9341_PASET            0x2BU
#define ILI9341_RAMWR            0x2CU
#define ILI9341_MADCTL           0x36U
#define ILI9341_COLMOD           0x3AU

static void tft_select_command_mode(void)
{
  HAL_GPIO_WritePin(TFT_DC_PORT, TFT_DC_PIN, GPIO_PIN_RESET);
}

static void tft_select_data_mode(void)
{
  HAL_GPIO_WritePin(TFT_DC_PORT, TFT_DC_PIN, GPIO_PIN_SET);
}

static void tft_write_command(SPI_HandleTypeDef *spi, uint8_t command)
{
  tft_select_command_mode();
  HAL_SPI_Transmit(spi, &command, 1U, HAL_MAX_DELAY);
}

static void tft_write_data(SPI_HandleTypeDef *spi, const uint8_t *data, uint16_t len)
{
  tft_select_data_mode();
  HAL_SPI_Transmit(spi, (uint8_t *)data, len, HAL_MAX_DELAY);
}

static void tft_reset(void)
{
  HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_RESET);
  HAL_Delay(20U);
  HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_SET);
  HAL_Delay(120U);
}

static void tft_set_address_window(SPI_HandleTypeDef *spi,
                                   uint16_t x0,
                                   uint16_t y0,
                                   uint16_t x1,
                                   uint16_t y1)
{
  uint8_t data[4];

  tft_write_command(spi, ILI9341_CASET);
  data[0] = (uint8_t)(x0 >> 8);
  data[1] = (uint8_t)(x0 & 0xFFU);
  data[2] = (uint8_t)(x1 >> 8);
  data[3] = (uint8_t)(x1 & 0xFFU);
  tft_write_data(spi, data, sizeof(data));

  tft_write_command(spi, ILI9341_PASET);
  data[0] = (uint8_t)(y0 >> 8);
  data[1] = (uint8_t)(y0 & 0xFFU);
  data[2] = (uint8_t)(y1 >> 8);
  data[3] = (uint8_t)(y1 & 0xFFU);
  tft_write_data(spi, data, sizeof(data));

  tft_write_command(spi, ILI9341_RAMWR);
}

static void tft_init_ili9341(SPI_HandleTypeDef *spi)
{
  uint8_t pixel_format = 0x55U; /* 16-bit/pixel (RGB565) */
  uint8_t rotation = 0x28U;     /* Landscape, BGR */

  tft_reset();

  tft_write_command(spi, ILI9341_SWRESET);
  HAL_Delay(150U);

  tft_write_command(spi, ILI9341_COLMOD);
  tft_write_data(spi, &pixel_format, 1U);

  tft_write_command(spi, ILI9341_MADCTL);
  tft_write_data(spi, &rotation, 1U);

  tft_write_command(spi, ILI9341_SLPOUT);
  HAL_Delay(120U);

  tft_write_command(spi, ILI9341_DISPON);
  HAL_Delay(20U);
}

static uint16_t read_u16_le(const uint8_t *data, uint32_t offset)
{
  return (uint16_t)((uint16_t)data[offset] |
                    ((uint16_t)data[offset + 1U] << 8));
}

static uint32_t read_u32_le(const uint8_t *data, uint32_t offset)
{
  return (uint32_t)data[offset] |
         ((uint32_t)data[offset + 1U] << 8) |
         ((uint32_t)data[offset + 2U] << 16) |
         ((uint32_t)data[offset + 3U] << 24);
}

static void tft_render_bmp4(SPI_HandleTypeDef *spi, const BitmapImage *bitmap)
{
  uint16_t line[TFT_WIDTH];
  uint16_t palette[16];
  uint32_t data_offset;
  uint32_t dib_size;
  uint32_t width;
  int32_t height_signed;
  uint32_t height;
  uint16_t bits_per_pixel;
  uint32_t compression;
  uint32_t colors_used;
  uint32_t palette_offset;
  uint32_t row_stride;
  uint8_t top_down = 0U;

  if ((bitmap == NULL) ||
      (bitmap->data == NULL) ||
      (bitmap->width != TFT_WIDTH) ||
      (bitmap->height != TFT_HEIGHT))
  {
    return;
  }

  if ((bitmap->data_len < 118U) ||
      (bitmap->data[0] != 'B') ||
      (bitmap->data[1] != 'M'))
  {
    return;
  }

  data_offset = read_u32_le(bitmap->data, 10U);
  dib_size = read_u32_le(bitmap->data, 14U);
  width = read_u32_le(bitmap->data, 18U);
  height_signed = (int32_t)read_u32_le(bitmap->data, 22U);
  bits_per_pixel = read_u16_le(bitmap->data, 28U);
  compression = read_u32_le(bitmap->data, 30U);
  colors_used = read_u32_le(bitmap->data, 46U);
  palette_offset = 14U + dib_size;

  if (height_signed < 0)
  {
    top_down = 1U;
    height = (uint32_t)(-height_signed);
  }
  else
  {
    height = (uint32_t)height_signed;
  }

  if ((width != bitmap->width) ||
      (height != bitmap->height) ||
      (bits_per_pixel != 4U) ||
      (compression != 0U))
  {
    return;
  }

  if ((colors_used == 0U) || (colors_used > 16U))
  {
    colors_used = 16U;
  }

  if ((palette_offset + (colors_used * 4U)) > bitmap->data_len)
  {
    return;
  }

  for (uint32_t i = 0; i < colors_used; i++)
  {
    uint32_t entry_offset = palette_offset + (i * 4U);
    uint8_t blue = bitmap->data[entry_offset];
    uint8_t green = bitmap->data[entry_offset + 1U];
    uint8_t red = bitmap->data[entry_offset + 2U];
    uint16_t color = (uint16_t)(((uint16_t)(red & 0xF8U) << 8) |
                                ((uint16_t)(green & 0xFCU) << 3) |
                                ((uint16_t)(blue & 0xF8U) >> 3));

    palette[i] = (uint16_t)((color << 8) | (color >> 8));
  }

  for (uint32_t i = colors_used; i < 16U; i++)
  {
    palette[i] = 0U;
  }

  row_stride = ((((width + 1U) / 2U) + 3U) / 4U) * 4U;
  if ((data_offset + (row_stride * height)) > bitmap->data_len)
  {
    return;
  }

  tft_set_address_window(spi, 0U, 0U, TFT_WIDTH - 1U, TFT_HEIGHT - 1U);
  tft_select_data_mode();

  for (uint16_t y = 0U; y < TFT_HEIGHT; y++)
  {
    uint32_t src_y = top_down ? y : ((height - 1U) - y);
    uint32_t row_offset = data_offset + (src_y * row_stride);

    for (uint16_t x = 0U; x < TFT_WIDTH; x++)
    {
      uint8_t packed = bitmap->data[row_offset + ((uint32_t)x / 2U)];
      uint8_t color_index = ((x & 1U) == 0U) ? (packed >> 4) : (packed & 0x0FU);

      if (color_index >= colors_used)
      {
        return;
      }

      line[x] = palette[color_index];
    }

    (void)HAL_SPI_Transmit(spi, (uint8_t *)line, TFT_WIDTH * 2U, HAL_MAX_DELAY);
  }
}

static void tft_render_rgb565_raw(SPI_HandleTypeDef *spi, const BitmapImage *bitmap)
{
  uint16_t line[TFT_WIDTH];

  if ((bitmap == NULL) ||
      (bitmap->data == NULL) ||
      (bitmap->width != TFT_WIDTH) ||
      (bitmap->height != TFT_HEIGHT) ||
      (bitmap->data_len != ((uint32_t)bitmap->width * (uint32_t)bitmap->height * 2U)))
  {
    return;
  }

  tft_set_address_window(spi, 0U, 0U, TFT_WIDTH - 1U, TFT_HEIGHT - 1U);
  tft_select_data_mode();

  for (uint16_t y = 0U; y < TFT_HEIGHT; y++)
  {
    uint32_t row_offset = (uint32_t)y * (uint32_t)TFT_WIDTH * 2U;

    for (uint16_t x = 0U; x < TFT_WIDTH; x++)
    {
      uint32_t pixel_offset = row_offset + ((uint32_t)x * 2U);
      line[x] = (uint16_t)(((uint16_t)bitmap->data[pixel_offset] << 8) |
                           (uint16_t)bitmap->data[pixel_offset + 1U]);
    }

    (void)HAL_SPI_Transmit(spi, (uint8_t *)line, TFT_WIDTH * 2U, HAL_MAX_DELAY);
  }
}

static void tft_render_bitmap_image(SPI_HandleTypeDef *spi, const BitmapImage *bitmap)
{
  if (bitmap == NULL)
  {
    return;
  }

  switch (bitmap->format)
  {
    case BITMAP_FORMAT_BMP4:
      tft_render_bmp4(spi, bitmap);
      break;

    case BITMAP_FORMAT_RGB565_RAW:
      tft_render_rgb565_raw(spi, bitmap);
      break;

    default:
      break;
  }
}

int run(SPI_HandleTypeDef *spi)
{
  //Switch screen on
  HAL_GPIO_WritePin(TFT_LGHT_PORT, TFT_LGHT_PIN, GPIO_PIN_SET);

  //Delay to allow screen to start
  HAL_Delay(1000U);

  //Init display
  tft_init_ili9341(spi);

  //Render image
  tft_render_bitmap_image(spi, &g_test_bmp4_bitmap);

  while (1)
  {
    HAL_Delay(2000U);
  }

  return 0;
}
