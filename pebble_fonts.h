// pebble_fonts.h
#ifndef PEBBLE_FONTS_H
#define PEBBLE_FONTS_H

#include <Arduino.h>
#include "display_driver.h"

// Font IDs based on Pebble OS
typedef enum {
  FONT_KEY_GOTHIC_14,        // Small system font
  FONT_KEY_GOTHIC_18,        // Medium system font
  FONT_KEY_GOTHIC_24,        // Large system font
  FONT_KEY_GOTHIC_28,        // Largest system font
  FONT_KEY_BITHAM_30_BLACK,  // Large and bold font
  FONT_KEY_BITHAM_42_BOLD,   // Very large bold font
  FONT_KEY_BITHAM_42_LIGHT,  // Very large light font
  FONT_KEY_ROBOTO_CONDENSED_21, // Medium condensed font
  FONT_KEY_ROBOTO_BOLD_SUBSET_49,  // Digital clock font
  // Add more fonts as needed
  FONT_KEY_TOTAL_COUNT  // Keep this at the end for counting purposes
} FontId;

// Font style flags
#define FONT_STYLE_NONE        0
#define FONT_STYLE_BOLD        1
#define FONT_STYLE_ITALIC      2
#define FONT_STYLE_CONDENSED   4

// Text alignment options
typedef enum {
  ALIGN_LEFT,
  ALIGN_CENTER,
  ALIGN_RIGHT
} TextAlignment;

// Font structure
typedef struct {
  uint8_t size;           // Size multiplier for TFT_eSPI
  uint8_t style;          // Style flags
  const char* name;       // Font name for debug
} PebbleFont;

// Functions
void fonts_init();
uint8_t fonts_get_tft_size(FontId font_id);
void draw_text(int x, int y, const char* text, FontId font_id, TextAlignment alignment, uint16_t color);
int text_width(const char* text, FontId font_id);

#endif // PEBBLE_FONTS_H