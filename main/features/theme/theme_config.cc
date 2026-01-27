#include "theme_config.h"
#include "settings.h"
#include <esp_log.h>
#include <cstring>

#define TAG "ThemeConfig"

bool ThemeManager::ParseThemeJson(const char* json_str) {
    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse theme JSON");
        return false;
    }
    
    cJSON* theme = cJSON_GetObjectItem(root, "theme");
    if (!theme) {
        theme = root;  // Try root as theme object
    }
    
    bool result = ParseThemeJson(theme);
    cJSON_Delete(root);
    return result;
}

bool ThemeManager::ParseThemeJson(cJSON* theme) {
    if (!cJSON_IsObject(theme)) {
        return false;
    }
    
    // Parse basic info
    cJSON* id = cJSON_GetObjectItem(theme, "id");
    if (cJSON_IsString(id)) {
        strncpy(current_theme_.id, id->valuestring, sizeof(current_theme_.id) - 1);
    }
    
    cJSON* name = cJSON_GetObjectItem(theme, "name");
    if (cJSON_IsString(name)) {
        strncpy(current_theme_.name, name->valuestring, sizeof(current_theme_.name) - 1);
    }
    
    // Parse colors
    cJSON* colors = cJSON_GetObjectItem(theme, "colors");
    if (cJSON_IsObject(colors)) {
        cJSON* primary = cJSON_GetObjectItem(colors, "primary");
        if (cJSON_IsString(primary)) {
            current_theme_.primary_color = ParseHexColor(primary->valuestring);
        }
        
        cJSON* secondary = cJSON_GetObjectItem(colors, "secondary");
        if (cJSON_IsString(secondary)) {
            current_theme_.secondary_color = ParseHexColor(secondary->valuestring);
        }
        
        cJSON* bg = cJSON_GetObjectItem(colors, "background");
        if (cJSON_IsString(bg)) {
            current_theme_.background_color = ParseHexColor(bg->valuestring);
        }
        
        cJSON* text = cJSON_GetObjectItem(colors, "text");
        if (cJSON_IsString(text)) {
            current_theme_.text_color = ParseHexColor(text->valuestring);
        }
        
        cJSON* accent = cJSON_GetObjectItem(colors, "accent");
        if (cJSON_IsString(accent)) {
            current_theme_.accent_color = ParseHexColor(accent->valuestring);
        }
    }
    
    // Parse clock config
    cJSON* clock = cJSON_GetObjectItem(theme, "clock");
    if (cJSON_IsObject(clock)) {
        cJSON* hour = cJSON_GetObjectItem(clock, "hour_hand");
        if (cJSON_IsString(hour)) {
            current_theme_.clock.hour_hand_color = ParseHexColor(hour->valuestring);
        }
        
        cJSON* minute = cJSON_GetObjectItem(clock, "minute_hand");
        if (cJSON_IsString(minute)) {
            current_theme_.clock.minute_hand_color = ParseHexColor(minute->valuestring);
        }
        
        cJSON* second = cJSON_GetObjectItem(clock, "second_hand");
        if (cJSON_IsString(second)) {
            current_theme_.clock.second_hand_color = ParseHexColor(second->valuestring);
        }
        
        cJSON* center = cJSON_GetObjectItem(clock, "center_dot");
        if (cJSON_IsString(center)) {
            current_theme_.clock.center_dot_color = ParseHexColor(center->valuestring);
        }
        
        cJSON* number = cJSON_GetObjectItem(clock, "number_color");
        if (cJSON_IsString(number)) {
            current_theme_.clock.number_color = ParseHexColor(number->valuestring);
        }
        
        cJSON* tick = cJSON_GetObjectItem(clock, "tick_color");
        if (cJSON_IsString(tick)) {
            current_theme_.clock.tick_color = ParseHexColor(tick->valuestring);
        }
        
        cJSON* bg = cJSON_GetObjectItem(clock, "background");
        if (cJSON_IsString(bg)) {
            current_theme_.clock.background_color = ParseHexColor(bg->valuestring);
        }
        
        cJSON* bg_img = cJSON_GetObjectItem(clock, "background_image");
        if (cJSON_IsString(bg_img) && strlen(bg_img->valuestring) > 0) {
            current_theme_.clock.use_background_image = true;
            strncpy(current_theme_.clock.background_image_url, bg_img->valuestring,
                    sizeof(current_theme_.clock.background_image_url) - 1);
        }
        
        cJSON* show_nums = cJSON_GetObjectItem(clock, "show_numbers");
        if (cJSON_IsBool(show_nums)) {
            current_theme_.clock.show_numbers = cJSON_IsTrue(show_nums);
        }
        
        cJSON* show_ticks = cJSON_GetObjectItem(clock, "show_tick_marks");
        if (cJSON_IsBool(show_ticks)) {
            current_theme_.clock.show_tick_marks = cJSON_IsTrue(show_ticks);
        }
    }
    
    // Parse menu config
    cJSON* menu = cJSON_GetObjectItem(theme, "menu");
    if (cJSON_IsObject(menu)) {
        cJSON* btn_colors = cJSON_GetObjectItem(menu, "button_colors");
        if (cJSON_IsArray(btn_colors)) {
            int i = 0;
            cJSON* color;
            cJSON_ArrayForEach(color, btn_colors) {
                if (i < 6 && cJSON_IsString(color)) {
                    current_theme_.menu.button_colors[i] = ParseHexColor(color->valuestring);
                    i++;
                }
            }
        }
        
        cJSON* text_color = cJSON_GetObjectItem(menu, "text_color");
        if (cJSON_IsString(text_color)) {
            current_theme_.menu.text_color = ParseHexColor(text_color->valuestring);
        }
        
        cJSON* bg_color = cJSON_GetObjectItem(menu, "background");
        if (cJSON_IsString(bg_color)) {
            current_theme_.menu.background_color = ParseHexColor(bg_color->valuestring);
        }
    }
    
    // Parse idle screen config
    cJSON* idle = cJSON_GetObjectItem(theme, "idle_screen");
    if (cJSON_IsObject(idle)) {
        cJSON* weather = cJSON_GetObjectItem(idle, "show_weather");
        if (cJSON_IsBool(weather)) {
            current_theme_.show_weather = cJSON_IsTrue(weather);
        }
        
        cJSON* lunar = cJSON_GetObjectItem(idle, "show_lunar");
        if (cJSON_IsBool(lunar)) {
            current_theme_.show_lunar = cJSON_IsTrue(lunar);
        }
        
        cJSON* bg_url = cJSON_GetObjectItem(idle, "background_image");
        if (cJSON_IsString(bg_url)) {
            strncpy(current_theme_.idle_background_url, bg_url->valuestring,
                    sizeof(current_theme_.idle_background_url) - 1);
        }
    }
    
    ESP_LOGI(TAG, "Theme parsed: %s (%s)", current_theme_.name, current_theme_.id);
    ESP_LOGI(TAG, "Clock colors: hour=0x%06lX, min=0x%06lX, sec=0x%06lX",
             current_theme_.clock.hour_hand_color,
             current_theme_.clock.minute_hand_color,
             current_theme_.clock.second_hand_color);
    
    return true;
}

bool ThemeManager::SaveTheme() {
    Settings settings("theme", true);
    
    settings.SetString("id", current_theme_.id);
    settings.SetString("name", current_theme_.name);
    
    // Save clock colors
    settings.SetInt("clock_hour", current_theme_.clock.hour_hand_color);
    settings.SetInt("clock_min", current_theme_.clock.minute_hand_color);
    settings.SetInt("clock_sec", current_theme_.clock.second_hand_color);
    settings.SetInt("clock_center", current_theme_.clock.center_dot_color);
    settings.SetInt("clock_num", current_theme_.clock.number_color);
    settings.SetInt("clock_bg", current_theme_.clock.background_color);
    settings.SetInt("clock_show_num", current_theme_.clock.show_numbers ? 1 : 0);
    
    // Save menu colors
    for (int i = 0; i < 6; i++) {
        char key[16];
        snprintf(key, sizeof(key), "menu_btn%d", i);
        settings.SetInt(key, current_theme_.menu.button_colors[i]);
    }
    
    ESP_LOGI(TAG, "Theme saved to NVS");
    return true;
}

bool ThemeManager::LoadTheme() {
    Settings settings("theme", false);
    
    std::string id = settings.GetString("id");
    if (id.empty()) {
        ESP_LOGI(TAG, "No saved theme, using defaults");
        return false;
    }
    
    strncpy(current_theme_.id, id.c_str(), sizeof(current_theme_.id) - 1);
    
    std::string name = settings.GetString("name");
    if (!name.empty()) {
        strncpy(current_theme_.name, name.c_str(), sizeof(current_theme_.name) - 1);
    }
    
    // Load clock colors
    int hour = settings.GetInt("clock_hour");
    if (hour > 0) current_theme_.clock.hour_hand_color = hour;
    
    int min = settings.GetInt("clock_min");
    if (min > 0) current_theme_.clock.minute_hand_color = min;
    
    int sec = settings.GetInt("clock_sec");
    if (sec > 0) current_theme_.clock.second_hand_color = sec;
    
    int center = settings.GetInt("clock_center");
    if (center > 0) current_theme_.clock.center_dot_color = center;
    
    int num = settings.GetInt("clock_num");
    if (num > 0) current_theme_.clock.number_color = num;
    
    int bg = settings.GetInt("clock_bg");
    if (bg >= 0) current_theme_.clock.background_color = bg;
    
    int show_num = settings.GetInt("clock_show_num");
    current_theme_.clock.show_numbers = (show_num == 1);
    
    // Load menu colors
    for (int i = 0; i < 6; i++) {
        char key[16];
        snprintf(key, sizeof(key), "menu_btn%d", i);
        int color = settings.GetInt(key);
        if (color > 0) {
            current_theme_.menu.button_colors[i] = color;
        }
    }
    
    ESP_LOGI(TAG, "Theme loaded from NVS: %s", current_theme_.name);
    return true;
}

lv_color_t ThemeManager::HexToLvColor(uint32_t hex) {
    uint8_t r = (hex >> 16) & 0xFF;
    uint8_t g = (hex >> 8) & 0xFF;
    uint8_t b = hex & 0xFF;
    return lv_color_make(r, g, b);
}

uint32_t ThemeManager::ParseHexColor(const char* hex_str) {
    if (!hex_str) return 0;
    
    // Skip # prefix if present
    if (hex_str[0] == '#') {
        hex_str++;
    }
    
    // Parse hex string
    uint32_t color = 0;
    for (int i = 0; i < 6 && hex_str[i]; i++) {
        char c = hex_str[i];
        int digit = 0;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        }
        color = (color << 4) | digit;
    }
    
    return color;
}

void ThemeManager::ApplyTheme() {
    // This will be called by display components to apply theme
    ESP_LOGI(TAG, "Applying theme: %s", current_theme_.name);
    // Theme application is handled by individual UI components
}
