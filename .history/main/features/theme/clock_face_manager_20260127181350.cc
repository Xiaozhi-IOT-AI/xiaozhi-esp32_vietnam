#include "clock_face_manager.h"
#include "settings.h"
#include <cJSON.h>
#include <esp_log.h>
#include <cmath>
#include <cstring>
#include <sys/time.h>
#include "misc/lv_timer_private.h"

#define TAG "ClockFaceManager"
#define PI 3.14159265f

ClockFaceManager::ClockFaceManager(lv_obj_t* parent, int width, int height)
    : parent_(parent), width_(width), height_(height) {
    
    // Portrait mode: sử dụng chiều rộng làm đường kính
    int diameter = (width < height) ? width - 20 : height - 20;
    radius_ = diameter / 2;
    center_x_ = width / 2;
    center_y_ = height / 2;
    
    InitDefaultFaces();
    LoadSelectedFace();
    CreateUI();
}

ClockFaceManager::~ClockFaceManager() {
    if (update_timer_) {
        lv_timer_delete(update_timer_);
    }
    if (canvas_buffer_) {
        lv_free(canvas_buffer_);
    }
    if (container_) {
        lv_obj_delete(container_);
    }
    
    // Free background images
    for (auto& img : bg_images_) {
        if (img.data) {
            free(img.data);
        }
    }
}

void ClockFaceManager::InitDefaultFaces() {
    faces_.clear();
    
    // Face 0: Classic Analog (Black background, white hands)
    ClockFaceConfig classic = {};
    classic.type = CLOCK_FACE_ANALOG_CLASSIC;
    strcpy(classic.name, "Classic");
    classic.has_background_image = false;
    classic.hour_hand_color = 0xFFFFFF;
    classic.minute_hand_color = 0xFFFFFF;
    classic.second_hand_color = 0xFF0000;
    classic.center_dot_color = 0xFFFFFF;
    classic.background_color = 0x000000;
    classic.number_color = 0xFFFFFF;
    classic.tick_color = 0xCCCCCC;
    classic.show_numbers = true;
    classic.show_tick_marks = true;
    classic.show_date = false;
    classic.show_seconds = true;
    faces_.push_back(classic);
    
    // Face 1: Pink Unicorn (từ hình ảnh user gửi)
    ClockFaceConfig pink = {};
    pink.type = CLOCK_FACE_ANALOG_PINK;
    strcpy(pink.name, "Unicorn");
    pink.has_background_image = true;  // Sẽ load từ server
    pink.hour_hand_color = 0xFF69B4;   // Hot pink
    pink.minute_hand_color = 0xFF1493; // Deep pink
    pink.second_hand_color = 0x87CEEB; // Sky blue
    pink.center_dot_color = 0xFF69B4;
    pink.background_color = 0xFFF0F5;  // Lavender blush
    pink.number_color = 0xFF1493;
    pink.tick_color = 0xFFB6C1;
    classic.show_numbers = true;
    classic.show_tick_marks = false;
    classic.show_date = false;
    classic.show_seconds = true;
    faces_.push_back(pink);
    
    // Face 2: Digital
    ClockFaceConfig digital = {};
    digital.type = CLOCK_FACE_DIGITAL;
    strcpy(digital.name, "Digital");
    digital.has_background_image = false;
    digital.background_color = 0x1A1A2E;
    digital.number_color = 0x00FF00;  // Green LED style
    digital.show_date = true;
    digital.show_seconds = true;
    faces_.push_back(digital);
    
    // Face 3: Minimal
    ClockFaceConfig minimal = {};
    minimal.type = CLOCK_FACE_MINIMAL;
    strcpy(minimal.name, "Minimal");
    minimal.has_background_image = false;
    minimal.hour_hand_color = 0xFFFFFF;
    minimal.minute_hand_color = 0xCCCCCC;
    minimal.second_hand_color = 0xFF6B6B;
    minimal.center_dot_color = 0xFFFFFF;
    minimal.background_color = 0x2D2D44;
    minimal.number_color = 0xFFFFFF;
    minimal.tick_color = 0x4A4A6A;
    minimal.show_numbers = false;
    minimal.show_tick_marks = true;
    minimal.show_date = false;
    minimal.show_seconds = true;
    faces_.push_back(minimal);
    
    // Initialize background image cache
    bg_images_.resize(faces_.size());
    
    ESP_LOGI(TAG, "Initialized %d default clock faces", (int)faces_.size());
}

void ClockFaceManager::CreateUI() {
    // Create main container
    container_ = lv_obj_create(parent_);
    lv_obj_set_size(container_, width_, height_);
    lv_obj_set_pos(container_, 0, 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(container_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
    
    // Create background image holder
    bg_image_ = lv_image_create(container_);
    lv_obj_set_size(bg_image_, width_, height_);
    lv_obj_center(bg_image_);
    lv_obj_add_flag(bg_image_, LV_OBJ_FLAG_HIDDEN);
    
    // Create canvas for analog clock drawing
    int diameter = radius_ * 2;
    size_t buf_size = LV_CANVAS_BUF_SIZE(diameter, diameter, 16, LV_DRAW_BUF_STRIDE_ALIGN);
    canvas_buffer_ = (uint8_t*)lv_malloc(buf_size);
    if (canvas_buffer_) {
        memset(canvas_buffer_, 0, buf_size);
        canvas_ = lv_canvas_create(container_);
        lv_canvas_set_buffer(canvas_, canvas_buffer_, diameter, diameter, LV_COLOR_FORMAT_RGB565);
        lv_obj_center(canvas_);
    }
    
    // Create digital time label (hidden by default)
    digital_time_ = lv_label_create(container_);
    lv_label_set_text(digital_time_, "00:00:00");
    lv_obj_set_style_text_font(digital_time_, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(digital_time_, lv_color_hex(0x00FF00), 0);
    lv_obj_center(digital_time_);
    lv_obj_add_flag(digital_time_, LV_OBJ_FLAG_HIDDEN);
    
    // Create date label
    date_label_ = lv_label_create(container_);
    lv_label_set_text(date_label_, "");
    lv_obj_set_style_text_font(date_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(date_label_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(date_label_, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_flag(date_label_, LV_OBJ_FLAG_HIDDEN);
    
    ESP_LOGI(TAG, "Clock face UI created");
}

void ClockFaceManager::RenderCurrentFace() {
    if (current_index_ < 0 || current_index_ >= (int)faces_.size()) {
        return;
    }
    
    const ClockFaceConfig& config = faces_[current_index_];
    
    // Set background color
    lv_obj_set_style_bg_color(container_, lv_color_hex(config.background_color), 0);
    
    // Handle background image
    if (config.has_background_image && current_index_ < (int)bg_images_.size() && bg_images_[current_index_].loaded) {
        lv_obj_clear_flag(bg_image_, LV_OBJ_FLAG_HIDDEN);
        // Image already set via SetBackgroundImageData
    } else {
        lv_obj_add_flag(bg_image_, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Render based on type
    switch (config.type) {
        case CLOCK_FACE_ANALOG_CLASSIC:
        case CLOCK_FACE_ANALOG_PINK:
        case CLOCK_FACE_CUSTOM:
            lv_obj_clear_flag(canvas_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(digital_time_, LV_OBJ_FLAG_HIDDEN);
            DrawAnalogClock(config);
            break;
            
        case CLOCK_FACE_DIGITAL:
            lv_obj_add_flag(canvas_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(digital_time_, LV_OBJ_FLAG_HIDDEN);
            DrawDigitalClock(config);
            break;
            
        case CLOCK_FACE_MINIMAL:
            lv_obj_clear_flag(canvas_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(digital_time_, LV_OBJ_FLAG_HIDDEN);
            DrawMinimalClock(config);
            break;
            
        default:
            break;
    }
    
    // Show/hide date
    if (config.show_date) {
        lv_obj_clear_flag(date_label_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(date_label_, LV_OBJ_FLAG_HIDDEN);
    }
}

void ClockFaceManager::DrawAnalogClock(const ClockFaceConfig& config) {
    if (!canvas_) return;
    
    int diameter = radius_ * 2;
    int cx = radius_;  // Canvas center
    int cy = radius_;
    
    // Clear with background color (transparent if has background image)
    if (config.has_background_image) {
        lv_canvas_fill_bg(canvas_, lv_color_hex(0x000000), LV_OPA_TRANSP);
    } else {
        lv_canvas_fill_bg(canvas_, lv_color_hex(config.background_color), LV_OPA_COVER);
    }
    
    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);
    
    // Draw tick marks
    if (config.show_tick_marks) {
        lv_color_t tick_color = lv_color_hex(config.tick_color);
        
        for (int i = 0; i < 60; i++) {
            float angle = (i * 6 - 90) * PI / 180.0f;
            int tick_len = (i % 5 == 0) ? 12 : 6;
            int tick_width = (i % 5 == 0) ? 3 : 1;
            
            int x1 = cx + (int)((radius_ - 5) * cos(angle));
            int y1 = cy + (int)((radius_ - 5) * sin(angle));
            int x2 = cx + (int)((radius_ - 5 - tick_len) * cos(angle));
            int y2 = cy + (int)((radius_ - 5 - tick_len) * sin(angle));
            
            lv_draw_line_dsc_t line_dsc;
            lv_draw_line_dsc_init(&line_dsc);
            line_dsc.color = tick_color;
            line_dsc.width = tick_width;
            line_dsc.round_start = 1;
            line_dsc.round_end = 1;
            line_dsc.p1.x = x1;
            line_dsc.p1.y = y1;
            line_dsc.p2.x = x2;
            line_dsc.p2.y = y2;
            
            lv_draw_line(&layer, &line_dsc);
        }
    }
    
    // Draw numbers (12, 3, 6, 9)
    if (config.show_numbers) {
        lv_color_t num_color = lv_color_hex(config.number_color);
        
        const char* numbers[] = {"12", "3", "6", "9"};
        int positions[][2] = {
            {cx, cy - radius_ + 28},      // 12
            {cx + radius_ - 28, cy},      // 3
            {cx, cy + radius_ - 28},      // 6
            {cx - radius_ + 22, cy}       // 9
        };
        
        lv_draw_label_dsc_t label_dsc;
        lv_draw_label_dsc_init(&label_dsc);
        label_dsc.color = num_color;
        label_dsc.font = &lv_font_montserrat_28;
        label_dsc.align = LV_TEXT_ALIGN_CENTER;
        
        for (int i = 0; i < 4; i++) {
            lv_area_t area = {
                (lv_coord_t)(positions[i][0] - 18),
                (lv_coord_t)(positions[i][1] - 14),
                (lv_coord_t)(positions[i][0] + 18),
                (lv_coord_t)(positions[i][1] + 14)
            };
            label_dsc.text = numbers[i];
            lv_draw_label(&layer, &label_dsc, &area);
        }
    }
    
    lv_canvas_finish_layer(canvas_, &layer);
    
    // Draw hands
    float hour_angle = (hour_ % 12) * 30 + minute_ * 0.5f;
    float minute_angle = minute_ * 6;
    float second_angle = second_ * 6;
    
    // Hour hand
    DrawHand(cx, cy, radius_ * 0.5f, hour_angle, 
             lv_color_hex(config.hour_hand_color), 6);
    
    // Minute hand
    DrawHand(cx, cy, radius_ * 0.7f, minute_angle, 
             lv_color_hex(config.minute_hand_color), 4);
    
    // Second hand
    if (config.show_seconds) {
        DrawHand(cx, cy, radius_ * 0.85f, second_angle, 
                 lv_color_hex(config.second_hand_color), 2);
    }
    
    // Center dot
    lv_canvas_init_layer(canvas_, &layer);
    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = lv_color_hex(config.center_dot_color);
    arc_dsc.width = 8;
    arc_dsc.center.x = cx;
    arc_dsc.center.y = cy;
    arc_dsc.radius = 5;
    arc_dsc.start_angle = 0;
    arc_dsc.end_angle = 360;
    lv_draw_arc(&layer, &arc_dsc);
    lv_canvas_finish_layer(canvas_, &layer);
}

void ClockFaceManager::DrawDigitalClock(const ClockFaceConfig& config) {
    char time_str[16];
    if (config.show_seconds) {
        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", hour_, minute_, second_);
    } else {
        snprintf(time_str, sizeof(time_str), "%02d:%02d", hour_, minute_);
    }
    
    lv_label_set_text(digital_time_, time_str);
    lv_obj_set_style_text_color(digital_time_, lv_color_hex(config.number_color), 0);
    
    // Update date label
    if (config.show_date) {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        struct tm* timeinfo = localtime(&tv.tv_sec);
        
        const char* weekdays[] = {"CN", "T2", "T3", "T4", "T5", "T6", "T7"};
        char date_str[32];
        snprintf(date_str, sizeof(date_str), "%s, %02d/%02d/%04d",
                 weekdays[timeinfo->tm_wday],
                 timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900);
        lv_label_set_text(date_label_, date_str);
    }
}

void ClockFaceManager::DrawMinimalClock(const ClockFaceConfig& config) {
    // Same as analog but without numbers
    ClockFaceConfig minimal_config = config;
    minimal_config.show_numbers = false;
    DrawAnalogClock(minimal_config);
}

void ClockFaceManager::DrawHand(int center_x, int center_y, int length, float angle,
                                 lv_color_t color, int width) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);
    
    float rad = (angle - 90) * PI / 180.0f;
    int end_x = center_x + (int)(length * cos(rad));
    int end_y = center_y + (int)(length * sin(rad));
    
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = color;
    line_dsc.width = width;
    line_dsc.round_start = 1;
    line_dsc.round_end = 1;
    
    lv_point_precise_t points[2] = {
        {(lv_value_precise_t)center_x, (lv_value_precise_t)center_y},
        {(lv_value_precise_t)end_x, (lv_value_precise_t)end_y}
    };
    lv_draw_line(&layer, &line_dsc, &points[0], &points[1]);
    
    lv_canvas_finish_layer(canvas_, &layer);
}

void ClockFaceManager::UpdateTime() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm* timeinfo = localtime(&tv.tv_sec);
    
    hour_ = timeinfo->tm_hour;
    minute_ = timeinfo->tm_min;
    second_ = timeinfo->tm_sec;
    
    if (is_visible_ && !is_animating_) {
        RenderCurrentFace();
    }
}

void ClockFaceManager::SetTime(int hour, int minute, int second) {
    hour_ = hour;
    minute_ = minute;
    second_ = second;
    
    if (is_visible_) {
        RenderCurrentFace();
    }
}

void ClockFaceManager::NextFace() {
    if (faces_.empty()) return;
    
    int new_index = (current_index_ + 1) % faces_.size();
    AnimateSwipe(-1);  // Swipe left animation
    current_index_ = new_index;
    
    ESP_LOGI(TAG, "Clock face: %d/%d (%s)", current_index_ + 1, (int)faces_.size(), 
             faces_[current_index_].name);
    
    if (change_callback_) {
        change_callback_(current_index_, &faces_[current_index_], callback_data_);
    }
    
    SaveSelectedFace();
}

void ClockFaceManager::PreviousFace() {
    if (faces_.empty()) return;
    
    int new_index = (current_index_ > 0) ? current_index_ - 1 : faces_.size() - 1;
    AnimateSwipe(+1);  // Swipe right animation
    current_index_ = new_index;
    
    ESP_LOGI(TAG, "Clock face: %d/%d (%s)", current_index_ + 1, (int)faces_.size(), 
             faces_[current_index_].name);
    
    if (change_callback_) {
        change_callback_(current_index_, &faces_[current_index_], callback_data_);
    }
    
    SaveSelectedFace();
}

void ClockFaceManager::SetCurrentFace(int index) {
    if (index >= 0 && index < (int)faces_.size()) {
        current_index_ = index;
        RenderCurrentFace();
        SaveSelectedFace();
    }
}

const ClockFaceConfig* ClockFaceManager::GetCurrentFace() const {
    if (current_index_ >= 0 && current_index_ < (int)faces_.size()) {
        return &faces_[current_index_];
    }
    return nullptr;
}

void ClockFaceManager::AnimateSwipe(int direction) {
    // Simple animation - just update immediately for now
    // TODO: Add smooth swipe animation
    is_animating_ = true;
    RenderCurrentFace();
    is_animating_ = false;
}

void ClockFaceManager::Show() {
    if (container_) {
        UpdateTime();
        lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN);
        is_visible_ = true;
        
        // Start auto-update timer
        if (!update_timer_) {
            update_timer_ = lv_timer_create(TimerCallback, 1000, this);
        }
        
        ESP_LOGI(TAG, "Clock face shown: %s", faces_[current_index_].name);
    }
}

void ClockFaceManager::Hide() {
    if (container_) {
        lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
        is_visible_ = false;
        
        // Stop timer
        if (update_timer_) {
            lv_timer_delete(update_timer_);
            update_timer_ = nullptr;
        }
        
        ESP_LOGI(TAG, "Clock face hidden");
    }
}

void ClockFaceManager::TimerCallback(lv_timer_t* timer) {
    ClockFaceManager* self = static_cast<ClockFaceManager*>(timer->user_data);
    if (self && self->is_visible_) {
        self->UpdateTime();
    }
}

void ClockFaceManager::AddClockFace(const ClockFaceConfig& config) {
    faces_.push_back(config);
    bg_images_.resize(faces_.size());
    ESP_LOGI(TAG, "Added clock face: %s (total: %d)", config.name, (int)faces_.size());
}

void ClockFaceManager::ClearAllFaces() {
    for (auto& img : bg_images_) {
        if (img.data) {
            free(img.data);
            img.data = nullptr;
        }
    }
    bg_images_.clear();
    faces_.clear();
    current_index_ = 0;
}

bool ClockFaceManager::ParseClockFacesJson(const char* json_str) {
    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse clock faces JSON");
        return false;
    }
    
    cJSON* faces_arr = cJSON_GetObjectItem(root, "clock_faces");
    if (!faces_arr) {
        faces_arr = root;  // Try root as array
    }
    
    if (!cJSON_IsArray(faces_arr)) {
        cJSON_Delete(root);
        return false;
    }
    
    cJSON* face_obj;
    cJSON_ArrayForEach(face_obj, faces_arr) {
        ClockFaceConfig config = {};
        config.type = CLOCK_FACE_CUSTOM;
        
        cJSON* name = cJSON_GetObjectItem(face_obj, "name");
        if (cJSON_IsString(name)) {
            strncpy(config.name, name->valuestring, sizeof(config.name) - 1);
        }
        
        cJSON* bg_url = cJSON_GetObjectItem(face_obj, "background_url");
        if (cJSON_IsString(bg_url) && strlen(bg_url->valuestring) > 0) {
            strncpy(config.background_url, bg_url->valuestring, sizeof(config.background_url) - 1);
            config.has_background_image = true;
        }
        
        // Parse colors
        cJSON* hour = cJSON_GetObjectItem(face_obj, "hour_hand_color");
        if (cJSON_IsString(hour)) {
            config.hour_hand_color = ThemeManager::ParseHexColor(hour->valuestring);
        } else {
            config.hour_hand_color = 0xFFFFFF;
        }
        
        cJSON* minute = cJSON_GetObjectItem(face_obj, "minute_hand_color");
        if (cJSON_IsString(minute)) {
            config.minute_hand_color = ThemeManager::ParseHexColor(minute->valuestring);
        } else {
            config.minute_hand_color = 0xFFFFFF;
        }
        
        cJSON* second = cJSON_GetObjectItem(face_obj, "second_hand_color");
        if (cJSON_IsString(second)) {
            config.second_hand_color = ThemeManager::ParseHexColor(second->valuestring);
        } else {
            config.second_hand_color = 0xFF0000;
        }
        
        cJSON* bg_color = cJSON_GetObjectItem(face_obj, "background_color");
        if (cJSON_IsString(bg_color)) {
            config.background_color = ThemeManager::ParseHexColor(bg_color->valuestring);
        } else {
            config.background_color = 0x000000;
        }
        
        cJSON* num_color = cJSON_GetObjectItem(face_obj, "number_color");
        if (cJSON_IsString(num_color)) {
            config.number_color = ThemeManager::ParseHexColor(num_color->valuestring);
        } else {
            config.number_color = 0xFFFFFF;
        }
        
        cJSON* show_nums = cJSON_GetObjectItem(face_obj, "show_numbers");
        config.show_numbers = cJSON_IsTrue(show_nums);
        
        cJSON* show_ticks = cJSON_GetObjectItem(face_obj, "show_tick_marks");
        config.show_tick_marks = cJSON_IsTrue(show_ticks);
        
        cJSON* show_secs = cJSON_GetObjectItem(face_obj, "show_seconds");
        config.show_seconds = !cJSON_IsFalse(show_secs);  // Default true
        
        AddClockFace(config);
    }
    
    cJSON_Delete(root);
    ESP_LOGI(TAG, "Parsed %d clock faces from server", (int)faces_.size());
    return true;
}

void ClockFaceManager::SetBackgroundImageData(int face_index, const uint8_t* data, size_t len) {
    if (face_index < 0 || face_index >= (int)bg_images_.size()) {
        return;
    }
    
    // Free old data
    if (bg_images_[face_index].data) {
        free(bg_images_[face_index].data);
    }
    
    // Copy new data
    bg_images_[face_index].data = (uint8_t*)malloc(len);
    if (bg_images_[face_index].data) {
        memcpy(bg_images_[face_index].data, data, len);
        bg_images_[face_index].len = len;
        bg_images_[face_index].loaded = true;
        
        // Set to LVGL image if this is current face
        if (face_index == current_index_ && bg_image_) {
            // TODO: Decode image and set to lv_image
            ESP_LOGI(TAG, "Background image loaded for face %d (%zu bytes)", face_index, len);
        }
    }
}

void ClockFaceManager::SetChangeCallback(ClockFaceChangeCallback callback, void* user_data) {
    change_callback_ = callback;
    callback_data_ = user_data;
}

bool ClockFaceManager::SaveSelectedFace() {
    Settings settings("clock", true);
    settings.SetInt("face_index", current_index_);
    ESP_LOGI(TAG, "Saved clock face index: %d", current_index_);
    return true;
}

bool ClockFaceManager::LoadSelectedFace() {
    Settings settings("clock", false);
    int index = settings.GetInt("face_index", 0);
    
    if (index >= 0 && index < (int)faces_.size()) {
        current_index_ = index;
        ESP_LOGI(TAG, "Loaded clock face index: %d", current_index_);
        return true;
    }
    return false;
}
