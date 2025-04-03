#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <Arduino.h>
#include "display_driver.h"
#include "pebble_fonts.h"  // Include the Pebble fonts header

// Define MenuItem structure for icon menu
struct MenuItem {
  const char* label;
  uint16_t iconColor;
  bool hasToggle;
  bool toggleState;
};

class UIManager {
  public:
    UIManager();
    void begin();
    
    // Time functions
    void setTime(int hour, int minute, int second);
    void updateTime();
    
    // Regular menu functions
    void setMenuItems(const char** items, int count);
    void setIconMenuItems(MenuItem* items, int count);
    void menuNext();
    void menuPrevious();
    int getCurrentMenuItem();
    
    // Drawing functions
    void drawWatchFace();
    void drawMenu();
    void drawIconMenu();
    void drawStatusBar(bool showBattery, bool showBluetooth, bool showTime);
    void drawNotificationCard(const char* title, const char* message);
    void drawRecordingPage();
    void drawProgressBar(int x, int y, int width, int height, int percentage, uint16_t color);
    void drawActionBar(bool showUp, bool showSelect, bool showDown);
    
        // Text functions with Pebble OS fonts
    void drawTextWithFont(int x, int y, const char* text, FontId font_id, TextAlignment alignment, uint16_t color);
    
  private:
    int hours, minutes, seconds;
    int selectedMenuItem;
    int menuItemCount;
    const char** menuItems;  // Added pointer to external string array
    MenuItem* iconMenuItems;
    int iconMenuItemCount;
    char menuItemTexts[10][20]; // Store up to 10 menu items with 20 chars each
};

#endif