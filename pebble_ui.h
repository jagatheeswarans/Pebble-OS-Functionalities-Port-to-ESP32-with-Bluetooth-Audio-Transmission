// pebble_ui.h
#ifndef PEBBLE_UI_H
#define PEBBLE_UI_H

#include <Arduino.h>
#include "display_driver.h"

// UI element types
typedef enum {
  ELEMENT_RECT,
  ELEMENT_TEXT,
  ELEMENT_CIRCLE,
  ELEMENT_LINE
} ElementType;

// UI element structure
typedef struct {
  ElementType type;
  int x;
  int y;
  int width;
  int height;
  uint16_t color;
  char text[32];
  bool visible;
} UIElement;

// Window structure
typedef struct {
  UIElement elements[16];  // Maximum 16 elements per window
  int elementCount;
  char title[32];
} Window;

// Function declarations
void ui_init_window(Window* window, const char* title);
void ui_add_text(Window* window, int x, int y, const char* text, uint16_t color);
void ui_add_rect(Window* window, int x, int y, int width, int height, uint16_t color);
void ui_add_circle(Window* window, int x, int y, int radius, uint16_t color);
void ui_add_line(Window* window, int x1, int y1, int x2, int y2, uint16_t color);
void ui_draw_window(Window* window);
void ui_update_text(Window* window, int elementIndex, const char* newText);
void ui_set_element_visibility(Window* window, int elementIndex, bool visible);

#endif // PEBBLE_UI_H