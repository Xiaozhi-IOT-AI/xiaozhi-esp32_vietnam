#ifndef CLOCK_FACE_MANAGER_H
#define CLOCK_FACE_MANAGER_H

#include <lvgl.h>
#include <vector>
#include <string>
#include <functional>
#include "theme_config.h"

/**
 * Clock Face Manager - Manages multiple clock faces with swipe navigation
 * 
 * Features:
 * - Multiple clock face styles (digital, analog, minimal, with background)
 * - Swipe left/right to change clock face
 * - Background images from server
 * - Hand colors from theme
 * - Auto-save selected clock face to NVS
 */

// Clock face type
enum ClockFaceType {
    CLOCK_FACE_ANALOG_CLASSIC = 0,   // Analog đen với kim trắng
    CLOCK_FACE_ANALOG_PINK,          // Analog với nền hồng unicorn
    CLOCK_FACE_DIGITAL,              // Digital đơn giản
    CLOCK_FACE_MINIMAL,              // Tối giản
    CLOCK_FACE_CUSTOM,               // Custom từ server
    CLOCK_FACE_COUNT
};

// Clock face configuration
struct ClockFaceConfig {
    ClockFaceType type;
    char name[32];
    char background_url[256];        // URL hình nền từ server
    bool has_background_image;
    
    // Hand colors
    uint32_t hour_hand_color;
    uint32_t minute_hand_color;
    uint32_t second_hand_color;
    uint32_t center_dot_color;
    
    // Face colors
    uint32_t background_color;
    uint32_t number_color;
    uint32_t tick_color;
    
    // Options
    bool show_numbers;
    bool show_tick_marks;
    bool show_date;
    bool show_seconds;
};

// Callback when clock face changes
typedef void (*ClockFaceChangeCallback)(int index, const ClockFaceConfig* config, void* user_data);

class ClockFaceManager {
public:
    ClockFaceManager(lv_obj_t* parent, int width, int height);
    ~ClockFaceManager();
    
    // Initialize default clock faces
    void InitDefaultFaces();
    
    // Add clock face from server config
    void AddClockFace(const ClockFaceConfig& config);
    void ClearAllFaces();
    
    // Parse clock faces from JSON (from server)
    bool ParseClockFacesJson(const char* json_str);
    
    // Navigation
    void NextFace();
    void PreviousFace();
    void SetCurrentFace(int index);
    int GetCurrentFaceIndex() const { return current_index_; }
    int GetFaceCount() const { return faces_.size(); }
    const ClockFaceConfig* GetCurrentFace() const;
    
    // Show/Hide
    void Show();
    void Hide();
    bool IsVisible() const { return is_visible_; }
    
    // Update time display
    void UpdateTime();
    void SetTime(int hour, int minute, int second);
    
    // Set background image data (after download)
    void SetBackgroundImageData(int face_index, const uint8_t* data, size_t len);
    
    // Callback
    void SetChangeCallback(ClockFaceChangeCallback callback, void* user_data = nullptr);
    
    // Save/Load from NVS
    bool SaveSelectedFace();
    bool LoadSelectedFace();
    
    // Get root object
    lv_obj_t* GetRoot() { return container_; }
    
private:
    void CreateUI();
    void RenderCurrentFace();
    void DrawAnalogClock(const ClockFaceConfig& config);
    void DrawDigitalClock(const ClockFaceConfig& config);
    void DrawMinimalClock(const ClockFaceConfig& config);
    void DrawHand(int center_x, int center_y, int length, float angle, 
                  lv_color_t color, int width);
    void AnimateSwipe(int direction);  // -1 left, +1 right
    
    static void TimerCallback(lv_timer_t* timer);
    
    lv_obj_t* parent_;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* canvas_ = nullptr;
    lv_obj_t* bg_image_ = nullptr;
    lv_obj_t* digital_time_ = nullptr;
    lv_obj_t* date_label_ = nullptr;
    
    uint8_t* canvas_buffer_ = nullptr;
    
    int width_;
    int height_;
    int center_x_;
    int center_y_;
    int radius_;
    
    // Time
    int hour_ = 0;
    int minute_ = 0;
    int second_ = 0;
    
    // Clock faces
    std::vector<ClockFaceConfig> faces_;
    int current_index_ = 0;
    bool is_visible_ = false;
    
    // Animation
    lv_anim_t swipe_anim_;
    bool is_animating_ = false;
    
    // Timer
    lv_timer_t* update_timer_ = nullptr;
    
    // Callback
    ClockFaceChangeCallback change_callback_ = nullptr;
    void* callback_data_ = nullptr;
    
    // Background images cache (downloaded from server)
    struct BgImageCache {
        uint8_t* data = nullptr;
        size_t len = 0;
        bool loaded = false;
    };
    std::vector<BgImageCache> bg_images_;
};

#endif // CLOCK_FACE_MANAGER_H
