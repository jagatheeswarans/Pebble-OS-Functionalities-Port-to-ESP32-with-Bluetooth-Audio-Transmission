// pebble_fonts.cpp
#include "pebble_fonts.h"
#include "display_driver.h"

// Define the font mappings
static PebbleFont pebble_fonts[FONT_KEY_TOTAL_COUNT] = {
  {1, FONT_STYLE_NONE, "Gothic 14"},         // FONT_KEY_GOTHIC_14
  {2, FONT_STYLE_NONE, "Gothic 18"},         // FONT_KEY_GOTHIC_18
  {2, FONT_STYLE_BOLD, "Gothic 24"},         // FONT_KEY_GOTHIC_24
  {3, FONT_STYLE_NONE, "Gothic 28"},         // FONT_KEY_GOTHIC_28
  {3, FONT_STYLE_BOLD, "Bitham 30 Black"},   // FONT_KEY_BITHAM_30_BLACK
  {4, FONT_STYLE_BOLD, "Bitham 42 Bold"},    // FONT_KEY_BITHAM_42_BOLD
  {4, FONT_STYLE_NONE, "Bitham 42 Light"},   // FONT_KEY_BITHAM_42_LIGHT
  {2, FONT_STYLE_CONDENSED, "Roboto Condensed 21"}, // FONT_KEY_ROBOTO_CONDENSED_21
  {4, FONT_STYLE_BOLD, "Roboto Bold 49"},    // FONT_KEY_ROBOTO_BOLD_SUBSET_49
};

extern TFT_eSPI tft;  // Access the global TFT instance from display_driver.cpp

void fonts_init() {
  // Any initialization tasks for fonts
  // For this implementation, we're using TFT_eSPI's built-in font system
}

uint8_t fonts_get_tft_size(FontId font_id) {
  if (font_id >= FONT_KEY_TOTAL_COUNT) {
    return 1; // Default size if invalid
  }
  return pebble_fonts[font_id].size;
}

// Calculate the approximate width of text in pixels
int text_width(const char* text, FontId font_id) {
  if (!text || font_id >= FONT_KEY_TOTAL_COUNT) {
    return 0;
  }
  
  uint8_t size = pebble_fonts[font_id].size;
  // Very rough approximation based on TFT_eSPI character size
  return strlen(text) * 6 * size;
}

// Draw text with a specific font and alignment
void draw_text(int x, int y, const char* text, FontId font_id, TextAlignment alignment, uint16_t color) {
  if (!text || font_id >= FONT_KEY_TOTAL_COUNT) {
    return;
  }
  
  uint8_t size = pebble_fonts[font_id].size;
  
  // Handle alignment
  int text_x = x;
  if (alignment != ALIGN_LEFT) {
    int width = text_width(text, font_id);
    if (alignment == ALIGN_CENTER) {
      text_x = x - (width / 2);
    } else if (alignment == ALIGN_RIGHT) {
      text_x = x - width;
    }
  }
  
  // Set text parameters
  tft.setTextColor(color);
  tft.setTextSize(size);
  
  // Apply any style modifications available in TFT_eSPI
  // In a complete implementation, you might use different fonts for different styles
  
  // Draw the text
  tft.setCursor(text_x, y);
  tft.print(text);
}