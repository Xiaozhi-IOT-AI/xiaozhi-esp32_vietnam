#include "analog_clock.h"
#include <esp_log.h>
#include <cmath>
#include <cstring>
#include <sys/time.h>

#define TAG "AnalogClock"
#define PI 3.14159265f

AnalogClock::AnalogClock(lv_obj_t* parent, int x, int y, int diameter)
    : parent_(parent), x_(x), y_(y), diameter_(diameter) {
    
    radius_ = diameter / 2;
    center_x_ = radius_;
    center_y_ = radius_;
    
    // Load theme defaults
    auto& theme_mgr = ThemeManager::GetInstance();
    theme_ = theme_mgr.GetClockTheme();
    
    CreateUI();
}

AnalogClock::~AnalogClock() {
    StopAutoUpdate();
    if (canvas_buffer_) {
        lv_free(canvas_buffer_);
    }
    if (clock_root_) {
        lv_obj_delete(clock_root_);
    }
}

void AnalogClock::CreateUI() {
    // Create clock container
    clock_root_ = lv_obj_create(parent_);
    lv_obj_set_size(clock_root_, diameter_, diameter_);
    lv_obj_set_pos(clock_root_, x_, y_);
    lv_obj_set_style_bg_opa(clock_root_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(clock_root_, 0, 0);
    lv_obj_set_style_pad_all(clock_root_, 0, 0);
    lv_obj_clear_flag(clock_root_, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create canvas for drawing clock
    size_t buf_size = LV_CANVAS_BUF_SIZE(diameter_, diameter_, 16, LV_DRAW_BUF_STRIDE_ALIGN);
    canvas_buffer_ = (uint8_t*)lv_malloc(buf_size);
    if (!canvas_buffer_) {
        ESP_LOGE(TAG, "Failed to allocate canvas buffer");
        return;
    }
    memset(canvas_buffer_, 0, buf_size);
    
    canvas_ = lv_canvas_create(clock_root_);
    lv_canvas_set_buffer(canvas_, canvas_buffer_, diameter_, diameter_, LV_COLOR_FORMAT_RGB565);
    lv_obj_center(canvas_);
    
    // Initial draw
    UpdateTime();
    
    ESP_LOGI(TAG, "Analog clock created: %dx%d at (%d,%d)", diameter_, diameter_, x_, y_);
}

void AnalogClock::DrawClockFace() {
    if (!canvas_) return;
    
    // Clear with background color
    lv_color_t bg_color = ThemeManager::HexToLvColor(theme_.background_color);
    lv_canvas_fill_bg(canvas_, bg_color, LV_OPA_COVER);
    
    // Draw outer circle
    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);
    
    // Draw tick marks
    if (theme_.show_tick_marks) {
        lv_color_t tick_color = ThemeManager::HexToLvColor(theme_.tick_color);
        
        for (int i = 0; i < 60; i++) {
            float angle = (i * 6 - 90) * PI / 180.0f;
            int tick_len = (i % 5 == 0) ? 10 : 5;
            int tick_width = (i % 5 == 0) ? 3 : 1;
            
            int x1 = center_x_ + (int)((radius_ - 5) * cos(angle));
            int y1 = center_y_ + (int)((radius_ - 5) * sin(angle));
            int x2 = center_x_ + (int)((radius_ - 5 - tick_len) * cos(angle));
            int y2 = center_y_ + (int)((radius_ - 5 - tick_len) * sin(angle));
            
            lv_draw_line_dsc_t line_dsc;
            lv_draw_line_dsc_init(&line_dsc);
            line_dsc.color = tick_color;
            line_dsc.width = tick_width;
            line_dsc.round_start = 1;
            line_dsc.round_end = 1;
            
            lv_point_precise_t points[2] = {
                {(lv_value_precise_t)x1, (lv_value_precise_t)y1},
                {(lv_value_precise_t)x2, (lv_value_precise_t)y2}
            };
            lv_draw_line(&layer, &line_dsc, &points[0], &points[1]);
        }
    }
    
    // Draw numbers (12, 3, 6, 9)
    if (theme_.show_numbers) {
        lv_color_t num_color = ThemeManager::HexToLvColor(theme_.number_color);
        
        const char* numbers[] = {"12", "3", "6", "9"};
        int positions[][2] = {
            {center_x_, center_y_ - radius_ + 25},       // 12
            {center_x_ + radius_ - 25, center_y_},       // 3
            {center_x_, center_y_ + radius_ - 25},       // 6
            {center_x_ - radius_ + 20, center_y_}        // 9
        };
        
        lv_draw_label_dsc_t label_dsc;
        lv_draw_label_dsc_init(&label_dsc);
        label_dsc.color = num_color;
        label_dsc.font = &lv_font_montserrat_20;
        label_dsc.align = LV_TEXT_ALIGN_CENTER;
        
        for (int i = 0; i < 4; i++) {
            lv_area_t area = {
                (lv_coord_t)(positions[i][0] - 15),
                (lv_coord_t)(positions[i][1] - 12),
                (lv_coord_t)(positions[i][0] + 15),
                (lv_coord_t)(positions[i][1] + 12)
            };
            lv_draw_label(&layer, &label_dsc, &area, numbers[i], NULL);
        }
    }
    
    lv_canvas_finish_layer(canvas_, &layer);
}

void AnalogClock::DrawHand(int center_x, int center_y, int length, float angle,
                           lv_color_t color, int width, bool is_round) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);
    
    // Calculate end point
    float rad = (angle - 90) * PI / 180.0f;
    int end_x = center_x + (int)(length * cos(rad));
    int end_y = center_y + (int)(length * sin(rad));
    
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = color;
    line_dsc.width = width;
    line_dsc.round_start = is_round ? 1 : 0;
    line_dsc.round_end = is_round ? 1 : 0;
    
    lv_point_precise_t points[2] = {
        {(lv_value_precise_t)center_x, (lv_value_precise_t)center_y},
        {(lv_value_precise_t)end_x, (lv_value_precise_t)end_y}
    };
    lv_draw_line(&layer, &line_dsc, &points[0], &points[1]);
    
    lv_canvas_finish_layer(canvas_, &layer);
}

void AnalogClock::DrawHands() {
    // Calculate angles
    float hour_angle = (hour_ % 12) * 30 + minute_ * 0.5f;   // 30 degrees per hour + minute offset
    float minute_angle = minute_ * 6;                         // 6 degrees per minute
    float second_angle = second_ * 6;                         // 6 degrees per second
    
    // Hour hand (shortest, thickest)
    lv_color_t hour_color = ThemeManager::HexToLvColor(theme_.hour_hand_color);
    DrawHand(center_x_, center_y_, radius_ * 0.5f, hour_angle, hour_color, 6);
    
    // Minute hand (medium length)
    lv_color_t minute_color = ThemeManager::HexToLvColor(theme_.minute_hand_color);
    DrawHand(center_x_, center_y_, radius_ * 0.7f, minute_angle, minute_color, 4);
    
    // Second hand (longest, thinnest)
    lv_color_t second_color = ThemeManager::HexToLvColor(theme_.second_hand_color);
    DrawHand(center_x_, center_y_, radius_ * 0.85f, second_angle, second_color, 2);
    
    // Center dot
    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);
    
    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = ThemeManager::HexToLvColor(theme_.center_dot_color);
    arc_dsc.width = 8;
    arc_dsc.center.x = center_x_;
    arc_dsc.center.y = center_y_;
    arc_dsc.radius = 4;
    arc_dsc.start_angle = 0;
    arc_dsc.end_angle = 360;
    lv_draw_arc(&layer, &arc_dsc);
    
    lv_canvas_finish_layer(canvas_, &layer);
}

void AnalogClock::UpdateTime() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm* timeinfo = localtime(&tv.tv_sec);
    
    hour_ = timeinfo->tm_hour;
    minute_ = timeinfo->tm_min;
    second_ = timeinfo->tm_sec;
    
    // Redraw clock
    DrawClockFace();
    DrawHands();
}

void AnalogClock::SetTime(int hour, int minute, int second) {
    hour_ = hour;
    minute_ = minute;
    second_ = second;
    
    DrawClockFace();
    DrawHands();
}

void AnalogClock::SetClockTheme(const ClockThemeConfig& theme) {
    theme_ = theme;
    DrawClockFace();
    DrawHands();
}

void AnalogClock::SetHourHandColor(uint32_t color) {
    theme_.hour_hand_color = color;
    DrawClockFace();
    DrawHands();
}

void AnalogClock::SetMinuteHandColor(uint32_t color) {
    theme_.minute_hand_color = color;
    DrawClockFace();
    DrawHands();
}

void AnalogClock::SetSecondHandColor(uint32_t color) {
    theme_.second_hand_color = color;
    DrawClockFace();
    DrawHands();
}

void AnalogClock::SetBackgroundColor(uint32_t color) {
    theme_.background_color = color;
    DrawClockFace();
    DrawHands();
}

void AnalogClock::SetNumberColor(uint32_t color) {
    theme_.number_color = color;
    DrawClockFace();
    DrawHands();
}

void AnalogClock::Show() {
    if (clock_root_) {
        lv_obj_clear_flag(clock_root_, LV_OBJ_FLAG_HIDDEN);
        is_visible_ = true;
    }
}

void AnalogClock::Hide() {
    if (clock_root_) {
        lv_obj_add_flag(clock_root_, LV_OBJ_FLAG_HIDDEN);
        is_visible_ = false;
    }
}

void AnalogClock::TimerCallback(lv_timer_t* timer) {
    AnalogClock* self = static_cast<AnalogClock*>(timer->user_data);
    if (self && self->is_visible_) {
        self->UpdateTime();
    }
}

void AnalogClock::StartAutoUpdate() {
    if (!update_timer_) {
        update_timer_ = lv_timer_create(TimerCallback, 1000, this);
        ESP_LOGI(TAG, "Clock auto-update started");
    }
}

void AnalogClock::StopAutoUpdate() {
    if (update_timer_) {
        lv_timer_delete(update_timer_);
        update_timer_ = nullptr;
        ESP_LOGI(TAG, "Clock auto-update stopped");
    }
}

void AnalogClock::SetBackgroundImage(const void* img_data, int width, int height) {
    // TODO: Implement background image support
    has_bg_image_ = true;
}

void AnalogClock::ClearBackgroundImage() {
    has_bg_image_ = false;
    DrawClockFace();
    DrawHands();
}
