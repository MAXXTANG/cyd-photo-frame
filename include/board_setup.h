#define USE_HSPI_PORT
#ifdef PHOTO_ST7789
#define ST7789_DRIVER
#define TFT_INVERSION_OFF
#else
#define ILI9341_2_DRIVER
#endif
#define TFT_RGB_ORDER TFT_BGR
#define TFT_WIDTH 240
#define TFT_HEIGHT 320
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1
#define TFT_BL 21
#define TFT_BACKLIGHT_ON HIGH
#define SPI_FREQUENCY 27000000
#define LOAD_GLCD
#define LOAD_FONT2
