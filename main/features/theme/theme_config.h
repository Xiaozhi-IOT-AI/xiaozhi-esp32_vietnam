#ifndef THEME_CONFIG_H
#define THEME_CONFIG_H

#include <cJSON.h>
#include <string>
#include <lvgl.h>

/**
 * Theme Configuration for Vietnam Xiaozhi devices
 * Supports:
 * - Clock hand colors (pushed from server)
 * - Background images
 * - Menu button colors
 * - Text colors
 */

// Clock Theme Config
struct ClockThemeConfig {
    // Hand colors (RGB hex values)
    uint32_t hour_hand_color = 0xFFFFFF;      // White default
    uint32_t minute_hand_color = 0xFFFFFF;    // White default
    uint32_t second_hand_color = 0xFF0000;    // Red default
    uint32_t center_dot_color = 0xFFFFFF;     // White default
    
    // Clock face
    uint32_t number_color = 0xFFFFFF;
    uint32_t tick_color = 0xCCCCCC;
    uint32_t background_color = 0x000000;     // Black default
    bool show_numbers = true;
    bool show_tick_marks = true;
    
    // Background image
    bool use_background_image = false;
    char background_image_url[256] = {0};
};

// Menu Theme Config
struct MenuThemeConfig {
    uint32_t button_colors[6] = {
        0x87CEEB,  // Sky blue - Trợ lý
        0xFF6B6B,  // Red - Đồng hồ
        0xFFCC00,  // Yellow - Thẻ nhớ
        0x98FB98,  // Green - Radio
        0xDDA0DD,  // Purple - Âm lịch
        0x87CEEB   // Cyan - Thông tin
    };
    uint32_t text_color = 0xFFFFFF;
    uint32_t background_color = 0x1A1A2E;
};

// Main Theme Config
struct ThemeConfig {
    char id[64] = "default";
    char name[64] = "Default Theme";
    char version[16] = "1.0";
    
    // Colors
    uint32_t primary_color = 0xFF69B4;
    uint32_t secondary_color = 0xFF1493;
    uint32_t background_color = 0x1A1A2E;
    uint32_t text_color = 0xFFFFFF;
    uint32_t accent_color = 0x00CED1;
    
    // Sub-configs
    ClockThemeConfig clock;
    MenuThemeConfig menu;
    
    // Idle screen
    bool show_weather = true;
    bool show_lunar = true;
    char idle_background_url[256] = {0};
};

class ThemeManager {
public:
    static ThemeManager& GetInstance() {
        static ThemeManager instance;
        return instance;
    }
    
    // Parse theme from JSON (from OTA response or custom message)
    bool ParseThemeJson(const char* json_str);
    bool ParseThemeJson(cJSON* theme_obj);
    
    // Get current theme config
    ThemeConfig& GetTheme() { return current_theme_; }
    ClockThemeConfig& GetClockTheme() { return current_theme_.clock; }
    MenuThemeConfig& GetMenuTheme() { return current_theme_.menu; }
    
    // Apply theme to display
    void ApplyTheme();
    
    // Save/Load theme from NVS
    bool SaveTheme();
    bool LoadTheme();
    
    // Get LVGL color from hex
    static lv_color_t HexToLvColor(uint32_t hex);
    static uint32_t ParseHexColor(const char* hex_str);
    
private:
    ThemeManager() { LoadTheme(); }
    ThemeConfig current_theme_;
};

#endif // THEME_CONFIG_H
