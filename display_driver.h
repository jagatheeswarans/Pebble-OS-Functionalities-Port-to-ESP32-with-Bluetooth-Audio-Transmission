// display_driver.h
#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <Arduino.h>
#include <TFT_eSPI.h> // Include the display library you're using

// Screen dimensions - adjust these based on your display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 160

// Color definitions
#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF
#define COLOR_BLUE 0xF800
#define COLOR_RED 0x001F
#define COLOR_GREEN 0x07E0
#define COLOR_CYAN 0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_YELLOW 0xFFE0
#define COLOR_GRAY 0x8410

// Make TFT_eSPI instance accessible to other files
extern TFT_eSPI tft;

// Function declarations
bool display_init();
void display_clear();
void display_fillScreen(uint16_t color);
void display_drawText(int x, int y, const char* text, uint16_t color, uint8_t size);
void display_drawLine(int x0, int y0, int x1, int y1, uint16_t color);
void display_drawRect(int x, int y, int w, int h, uint16_t color);
void display_fillRect(int x, int y, int w, int h, uint16_t color);
void display_drawCircle(int x, int y, int r, uint16_t color);
void display_fillCircle(int x, int y, int r, uint16_t color);
void display_setRotation(uint8_t rotation);

#endif // DISPLAY_DRIVER_H