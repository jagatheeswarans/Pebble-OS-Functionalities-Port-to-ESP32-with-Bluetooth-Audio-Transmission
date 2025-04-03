#include <Arduino.h>
#include "ui_manager.h"
#include "display_driver.h"
#include "pebble_fonts.h"

// Define colors if not already defined in display_driver.h
#ifndef COLOR_DARKGRAY
#define COLOR_DARKGRAY 0x7BEF
#endif

#ifndef COLOR_LIGHTGRAY
#define COLOR_LIGHTGRAY 0xC618
#endif

UIManager::UIManager() {
  hours = 12;
  minutes = 0;
  seconds = 0;
  selectedMenuItem = 0;
  menuItemCount = 0;
  menuItems = NULL;
  iconMenuItems = NULL;
  iconMenuItemCount = 0;
}

void UIManager::begin() {
  // Initialize UI manager
  selectedMenuItem = 0;
  
  // Initialize fonts
  fonts_init();
}

void UIManager::setTime(int hour, int minute, int second) {
  hours = hour;
  minutes = minute;
  seconds = second;
}

void UIManager::updateTime() {
  static unsigned long lastTimeUpdate = 0;
  unsigned long currentTime = millis();
  
  // Only update time when a full second has passed
  if (currentTime - lastTimeUpdate >= 1000) {
    lastTimeUpdate = currentTime;
    
    // Update time (in a real app, this would sync with an RTC)
    seconds++;
    if (seconds >= 60) {
      seconds = 0;
      minutes++;
      if (minutes >= 60) {
        minutes = 0;
        hours++;
        if (hours >= 24) {
          hours = 0;
        }
      }
    }
    
    // Redraw watch face with new time
    drawWatchFace();
  }
}

void UIManager::drawWatchFace() {
  display_clear();
  
  // Draw status bar
  drawStatusBar(true, true, true);
  
  // Draw time using Pebble fonts - using even smaller font
  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", hours, minutes, seconds);
  
  // Use FONT_KEY_GOTHIC_24 instead of FONT_KEY_BITHAM_42_BOLD for the time
  drawTextWithFont(SCREEN_WIDTH / 2, 50, timeStr, FONT_KEY_GOTHIC_24, ALIGN_CENTER, COLOR_WHITE);
  
  // Increase spacing and use even smaller font for date
  drawTextWithFont(SCREEN_WIDTH / 2, 80, "APR 3, 2025", FONT_KEY_GOTHIC_14, ALIGN_CENTER, COLOR_WHITE);
  
  // Further adjust spacing for steps - more space between elements
  drawTextWithFont(SCREEN_WIDTH / 2, 110, "Steps: 8,421", FONT_KEY_GOTHIC_14, ALIGN_CENTER, COLOR_GREEN);
  
  // Further adjust spacing for battery
  drawTextWithFont(SCREEN_WIDTH / 2, 140, "Battery: 78%", FONT_KEY_GOTHIC_14, ALIGN_CENTER, COLOR_YELLOW);
}

void UIManager::drawStatusBar(bool showBattery, bool showBluetooth, bool showTime) {
  // Draw background for status bar
  display_fillRect(0, 0, SCREEN_WIDTH, 20, COLOR_BLACK);
  
  // Draw time in status bar if requested
  if (showTime) {
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", hours, minutes);
    drawTextWithFont(2, 5, timeStr, FONT_KEY_GOTHIC_14, ALIGN_LEFT, COLOR_WHITE);
  }
  
  // Draw Bluetooth icon if requested
  if (showBluetooth) {
    // Simple Bluetooth icon
    int btX = SCREEN_WIDTH - 55;
    display_fillCircle(btX, 10, 5, COLOR_BLUE);
  }
  
  // Draw battery icon if requested
  if (showBattery) {
    // Simple battery icon
    int batX = SCREEN_WIDTH - 25;
    display_drawRect(batX, 5, 20, 10, COLOR_WHITE);
    display_fillRect(batX + 2, 7, 12, 6, COLOR_GREEN);
    display_fillRect(batX + 20, 7, 2, 6, COLOR_WHITE);
  }
}

void UIManager::setMenuItems(const char** items, int count) {
  // Limit menu items to our array capacity
  if (count > 10) {
    count = 10;
  }
  
  // Copy each string to our internal storage
  for (int i = 0; i < count; i++) {
    // Copy the string safely with length limit
    strncpy(menuItemTexts[i], items[i], 19);
    menuItemTexts[i][19] = '\0'; // Ensure null termination
    Serial.printf("Stored menu item %d: %s\n", i, menuItemTexts[i]);
  }
  
  menuItemCount = count;
  selectedMenuItem = 0;
}

void UIManager::setIconMenuItems(MenuItem* items, int count) {
  iconMenuItems = items;
  iconMenuItemCount = count;
}

void UIManager::menuNext() {
  selectedMenuItem = (selectedMenuItem + 1) % menuItemCount;
}

void UIManager::menuPrevious() {
  selectedMenuItem = (selectedMenuItem + menuItemCount - 1) % menuItemCount;
}

int UIManager::getCurrentMenuItem() {
  return selectedMenuItem;
}

void UIManager::drawMenu() {
  display_clear();
  
  // Draw status bar at the top
  drawStatusBar(true, true, true);
  
  // Draw title
  drawTextWithFont(10, 30, "Menu", FONT_KEY_GOTHIC_24, ALIGN_LEFT, COLOR_WHITE);
  
  // Add debugging info
  char debugText[50];
  sprintf(debugText, "MenuItems: %d, Current: %d", menuItemCount, selectedMenuItem);
  drawTextWithFont(10, 190, debugText, FONT_KEY_GOTHIC_14, ALIGN_LEFT, COLOR_RED);
  
  // Draw menu items
  for (int i = 0; i < menuItemCount; i++) {
    int y = 60 + (i * 25);
    
    // Highlight selected item
    if (i == selectedMenuItem) {
      display_fillRect(0, y - 2, SCREEN_WIDTH, 20, COLOR_BLUE);
      // Use our internal storage
      drawTextWithFont(15, y, menuItemTexts[i], FONT_KEY_GOTHIC_14, ALIGN_LEFT, COLOR_YELLOW);
    } else {
      // Use our internal storage
      drawTextWithFont(15, y, menuItemTexts[i], FONT_KEY_GOTHIC_14, ALIGN_LEFT, COLOR_WHITE);
    }
  }
}

void UIManager::drawNotificationCard(const char* title, const char* message) {
  display_clear();
  
  // Draw notification title with Pebble font
  drawTextWithFont(10, 10, "NOTIFICATION", FONT_KEY_GOTHIC_14, ALIGN_LEFT, COLOR_YELLOW);
  drawTextWithFont(10, 35, title, FONT_KEY_GOTHIC_14, ALIGN_LEFT, COLOR_WHITE);
  
  // Draw notification card background
  display_fillRect(10, 50, SCREEN_WIDTH - 20, 80, COLOR_GRAY);
  display_drawRect(10, 50, SCREEN_WIDTH - 20, 80, COLOR_WHITE);
  
  // Draw notification content
  // Truncate message if too long
  char truncMsg[35];
  strncpy(truncMsg, message, 34);
  truncMsg[34] = '\0';
  
  drawTextWithFont(12, 60, truncMsg, FONT_KEY_GOTHIC_14, ALIGN_LEFT, COLOR_WHITE);
  
  // Draw dismiss text
  drawTextWithFont(15, 95, "Press SELECT\n    to dismiss", FONT_KEY_GOTHIC_14, ALIGN_LEFT, COLOR_LIGHTGRAY);
}

void UIManager::drawIconMenu() {
  display_clear();
  
  // Draw status bar
  drawStatusBar(true, true, true);
  
  // Draw title with Pebble font
  drawTextWithFont(10, 25, "Settings", FONT_KEY_GOTHIC_24, ALIGN_LEFT, COLOR_WHITE);
  
  // Define grid layout (2x2)
  int itemWidth = SCREEN_WIDTH / 2;
  int itemHeight = 60;
  
  // Draw menu items in a grid
  for (int i = 0; i < iconMenuItemCount && i < 4; i++) {
    int row = i / 2;
    int col = i % 2;
    int x = col * itemWidth + 5;
    int y = row * itemHeight + 50;
    
    // Draw icon (colored rectangle)
    display_fillRect(x + 5, y + 5, itemWidth - 15, 30, iconMenuItems[i].iconColor);
    
    // Draw item label with Pebble font
    drawTextWithFont(x + 8, y + 38, iconMenuItems[i].label, FONT_KEY_GOTHIC_14, ALIGN_LEFT, COLOR_WHITE);
    
    // Draw toggle switch if item has one
    if (iconMenuItems[i].hasToggle) {
      // Draw toggle background
      display_fillRect(x + itemWidth - 30, y + 38, 20, 10, COLOR_GRAY);
      
      // Draw toggle position
      if (iconMenuItems[i].toggleState) {
        display_fillCircle(x + itemWidth - 15, y + 43, 6, COLOR_GREEN);
      } else {
        display_fillCircle(x + itemWidth - 25, y + 43, 6, COLOR_RED);
      }
    }
    
    // Highlight selected item
    if (i == selectedMenuItem) {
      display_drawRect(x, y, itemWidth - 10, itemHeight - 5, COLOR_CYAN);
    }
  }
}

void UIManager::drawRecordingPage() {
  display_clear();
  
  // Draw status bar
  drawStatusBar(true, true, true);
  
  // Draw title with Pebble font
  drawTextWithFont(10, 35, "Recording", FONT_KEY_GOTHIC_24, ALIGN_LEFT, COLOR_BLUE);
  
  // Draw recording animation (pulsing circle)
  // This would be animated in a real app
  display_fillCircle(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 15, 25, COLOR_BLUE);
  display_drawCircle(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 15, 35, COLOR_BLUE);
  
  // Draw recording time with Pebble font
  //drawTextWithFont(SCREEN_WIDTH / 2, 120, "00:15", FONT_KEY_BITHAM_30_BLACK, ALIGN_CENTER, COLOR_WHITE);
  
  // Draw instructions
  //drawTextWithFont(SCREEN_WIDTH / 2, 150, "Press SELECT to stop", FONT_KEY_GOTHIC_14, ALIGN_CENTER, COLOR_WHITE);
}

void UIManager::drawProgressBar(int x, int y, int width, int height, int percentage, uint16_t color) {
  // Draw outline
  display_drawRect(x, y, width, height, COLOR_WHITE);
  
  // Calculate fill width
  int fillWidth = (width - 4) * percentage / 100;
  
  // Draw fill
  display_fillRect(x + 2, y + 2, fillWidth, height - 4, color);
}

void UIManager::drawActionBar(bool showUp, bool showSelect, bool showDown) {
  // Draw action bar background
  display_fillRect(SCREEN_WIDTH - 20, 0, 20, SCREEN_HEIGHT, COLOR_GRAY);
  
  // Draw up button if needed
  if (showUp) {
    // Draw triangle for up button
    int centerX = SCREEN_WIDTH - 10;
    int topY = 30;
    
    // Draw triangle manually if fillTriangle not available
    for (int i = 0; i < 10; i++) {
      int width = i;
      display_drawLine(centerX - width/2, topY + i, centerX + width/2, topY + i, COLOR_WHITE);
    }
  }
  
  // Draw select button if needed
  if (showSelect) {
    display_fillCircle(SCREEN_WIDTH - 10, SCREEN_HEIGHT / 2, 6, COLOR_WHITE);
  }
  
  // Draw down button if needed
  if (showDown) {
    // Draw triangle for down button
    int centerX = SCREEN_WIDTH - 10;
    int bottomY = SCREEN_HEIGHT - 40;
    
    // Draw triangle manually if fillTriangle not available
    for (int i = 0; i < 10; i++) {
      int width = 10 - i;
      display_drawLine(centerX - width/2, bottomY + i, centerX + width/2, bottomY + i, COLOR_WHITE);
    }
  }
}

// New method to draw text with Pebble fonts
void UIManager::drawTextWithFont(int x, int y, const char* text, FontId font_id, TextAlignment alignment, uint16_t color) {
  draw_text(x, y, text, font_id, alignment, color);
}