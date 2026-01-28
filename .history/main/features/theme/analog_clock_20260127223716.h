#ifndef ANALOG_CLOCK_H
#define ANALOG_CLOCK_H

#include <lvgl.h>
#include "../theme/theme_config.h"
#include <ctime>

/**
 * Analog Clock UI with theme-aware colors
 * Features:
 * - Configurable hand colors (from server theme)
 * - Background image support
 * - Smooth second hand animation
 * - 12/3/6/9 number display
 */

class AnalogClock {
public:
    AnalogClock(lv_obj_t* parent, int x, int y, int diameter);
    ~AnalogClock();
    
    // Update clock time
    void UpdateTime();
    void SetTime(int hour, int minute, int second);
    
    // Theme colors
    void SetClockTheme(const ClockThemeConfig& theme);
    void SetHourHandColor(uint32_t color);
    void SetMinuteHandColor(uint32_t color);
    void SetSecondHandColor(uint32_t color);
    void SetBackgroundColor(uint32_t color);
    void SetNumberColor(uint32_t color);
    
    // Show/hide
    void Show();
    void Hide();
    bool IsVisible() const { return is_visible_; }
    
    // Background image (from theme)
    void SetBackgroundImage(const void* img_data, int width, int height);
    void ClearBackgroundImage();
    
    // Get root object
    lv_obj_t* GetRoot() { return clock_root_; }
    
    // Start/stop auto update timer
    void StartAutoUpdate();
    void StopAutoUpdate();
    
private:
    void CreateUI();
    void DrawClockFace();
    void DrawHands();
    void DrawHand(int center_x, int center_y, int length, float angle, 
                  lv_color_t color, int width, bool is_round = true);
    
    static void TimerCallback(lv_timer_t* timer);
    
    lv_obj_t* parent_;
    lv_obj_t* clock_root_ = nullptr;
    lv_obj_t* canvas_ = nullptr;
    lv_obj_t* bg_image_ = nullptr;
    
    int x_, y_;
    int diameter_;
    int center_x_, center_y_;
    int radius_;
    int screen_width_ = 240;   // Fullscreen width
    int screen_height_ = 320;  // Fullscreen height
    
    uint8_t* canvas_buffer_ = nullptr;
    
    // Current time
    int hour_ = 0;
    int minute_ = 0;
    int second_ = 0;
    
    // Theme colors
    ClockThemeConfig theme_;
    
    bool is_visible_ = true;
    bool has_bg_image_ = false;
    
    lv_timer_t* update_timer_ = nullptr;
};

#endif // ANALOG_CLOCK_H
