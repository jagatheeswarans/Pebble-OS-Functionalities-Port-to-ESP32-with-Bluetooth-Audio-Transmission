// display_driver.cpp
#include "display_driver.h"

// Create an instance of the display
TFT_eSPI tft = TFT_eSPI();

bool display_init() {
  tft.init();
  tft.setRotation(0); // Set to portrait mode
  tft.fillScreen(COLOR_BLACK);
  return true;
}

void display_clear() {
  tft.fillScreen(COLOR_BLACK);
}

void display_fillScreen(uint16_t color) {
  tft.fillScreen(color);
}

void display_drawText(int x, int y, const char* text, uint16_t color, uint8_t size) {
  tft.setTextColor(color);
  tft.setTextSize(size);
  tft.setCursor(x, y);
  tft.print(text);
}

void display_drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
  tft.drawLine(x0, y0, x1, y1, color);
}

void display_drawRect(int x, int y, int w, int h, uint16_t color) {
  tft.drawRect(x, y, w, h, color);
}

void display_fillRect(int x, int y, int w, int h, uint16_t color) {
  tft.fillRect(x, y, w, h, color);
}

void display_drawCircle(int x, int y, int r, uint16_t color) {
  tft.drawCircle(x, y, r, color);
}

void display_fillCircle(int x, int y, int r, uint16_t color) {
  tft.fillCircle(x, y, r, color);
}

void display_setRotation(uint8_t rotation) {
  tft.setRotation(rotation);
}