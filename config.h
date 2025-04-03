// config.h
#ifndef CONFIG_H
#define CONFIG_H

// Display settings
#define DISPLAY_TYPE TFT_eSPI // Using TFT_eSPI library

// Theme/Color settings
#define THEME_BACKGROUND COLOR_BLACK
#define THEME_TEXT COLOR_WHITE
#define THEME_ACCENT COLOR_CYAN
#define THEME_HIGHLIGHT COLOR_BLUE

// Time display format (12/24 hour)
#define TIME_FORMAT_24H false

// Font settings
#define FONT_SMALL 1
#define FONT_MEDIUM 2
#define FONT_LARGE 3

#endif // CONFIG_H