#include "run.h"

#define TFT_WIDTH                240U
#define TFT_HEIGHT               320U

/*
 * Control pins for the SPI display module.
 * PA0 -> D/C
 * PA1 -> RST
 */
#define TFT_DC_PORT              GPIOA
#define TFT_DC_PIN               GPIO_PIN_0
#define TFT_RST_PORT             GPIOA
#define TFT_RST_PIN              GPIO_PIN_1

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
  uint8_t rotation = 0x48U;     /* MX=0, MY=1, MV=0, BGR=1 */

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

static void tft_draw_basic_image(SPI_HandleTypeDef *spi)
{
  enum { IMAGE_W = 64, IMAGE_H = 64 };
  uint16_t line[IMAGE_W];
  const uint16_t start_x = (TFT_WIDTH - IMAGE_W) / 2U;
  const uint16_t start_y = (TFT_HEIGHT - IMAGE_H) / 2U;

  tft_set_address_window(spi,
                         start_x,
                         start_y,
                         start_x + IMAGE_W - 1U,
                         start_y + IMAGE_H - 1U);

  tft_select_data_mode();

  for (uint16_t y = 0; y < IMAGE_H; y++)
  {
    for (uint16_t x = 0; x < IMAGE_W; x++)
    {
      uint16_t color;

      if (((x / 8U) + (y / 8U)) % 2U == 0U)
      {
        color = 0xF800U; /* Red */
      }
      else
      {
        color = 0x001FU; /* Blue */
      }

      line[x] = (uint16_t)((color << 8) | (color >> 8));
    }

    HAL_SPI_Transmit(spi, (uint8_t *)line, IMAGE_W * 2U, HAL_MAX_DELAY);
  }
}

int run(SPI_HandleTypeDef *spi)
{
  tft_init_ili9341(spi);
  tft_draw_basic_image(spi);

  while (1)
  {
    HAL_Delay(2000U);
  }

  return 0;
}
