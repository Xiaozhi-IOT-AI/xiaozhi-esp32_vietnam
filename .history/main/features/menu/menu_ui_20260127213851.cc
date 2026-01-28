#include "menu_ui.h"
#include "../theme/theme_config.h"
#include <esp_log.h>

#define TAG "MenuUI"

// Vietnamese font - defined in build system
extern const lv_font_t BUILTIN_TEXT_FONT;

// Default menu items with Vietnamese labels
const MenuItem MenuUI::default_items_[MENU_ACTION_COUNT] = {
    {"Trợ lý",    LV_SYMBOL_HOME,      0x87CEEB, nullptr},  // Sky blue
    {"Đồng hồ",   LV_SYMBOL_REFRESH,   0xFF6B6B, nullptr},  // Red/coral
    {"Thẻ nhớ",   LV_SYMBOL_SD_CARD,   0xFFCC00, nullptr},  // Yellow
    {"Radio",     LV_SYMBOL_VOLUME_MAX, 0x98FB98, nullptr}, // Light green
    {"Âm lịch",   LV_SYMBOL_LIST,      0xDDA0DD, nullptr},  // Plum
    {"Thông tin", LV_SYMBOL_SETTINGS,  0x87CEEB, nullptr},  // Cyan
};

MenuUI::MenuUI(lv_obj_t* parent, int width, int height) 
    : parent_(parent), width_(width), height_(height) {
    CreateUI();
}

MenuUI::~MenuUI() {
    if (menu_root_) {
        lv_obj_delete(menu_root_);
    }
}

void MenuUI::CreateUI() {
    // Create menu container (full screen)
    menu_root_ = lv_obj_create(parent_);
    if (!menu_root_) {
        ESP_LOGE(TAG, "Failed to create menu root object");
        return;
    }
    lv_obj_set_size(menu_root_, width_, height_);
    lv_obj_align(menu_root_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(menu_root_, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(menu_root_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(menu_root_, 0, 0);
    lv_obj_set_style_radius(menu_root_, 0, 0);
    lv_obj_set_style_pad_all(menu_root_, 0, 0);  // No padding on container
    lv_obj_add_flag(menu_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(menu_root_, LV_OBJ_FLAG_SCROLLABLE);
    
    // Get theme colors
    auto& theme_mgr = ThemeManager::GetInstance();
    auto& menu_theme = theme_mgr.GetMenuTheme();
    
    // Calculate button sizes for 2x3 grid (2 columns, 3 rows - better for portrait)
    // Portrait mode: 240x320
    int margin = 15;  // margin from screen edge
    int gap = 10;     // gap between buttons
    int cols = 2;
    int rows = 3;
    int available_width = width_ - (2 * margin) - ((cols - 1) * gap);
    int available_height = height_ - (2 * margin) - ((rows - 1) * gap);
    int btn_width = available_width / cols;
    int btn_height = available_height / rows;
    int btn_radius = 15;
    
    // Calculate starting position to center the grid
    int total_grid_width = cols * btn_width + (cols - 1) * gap;
    int total_grid_height = rows * btn_height + (rows - 1) * gap;
    int start_x = (width_ - total_grid_width) / 2;
    int start_y = (height_ - total_grid_height) / 2;
    
    ESP_LOGI(TAG, "Menu grid: %dx%d, btn: %dx%d, start: (%d,%d)", 
             cols, rows, btn_width, btn_height, start_x, start_y);
    
    // Create 6 buttons in 2x3 grid (2 columns, 3 rows)
    for (int i = 0; i < MENU_ACTION_COUNT; i++) {
        int row = i / cols;  // 0,0,1,1,2,2
        int col = i % cols;  // 0,1,0,1,0,1
        int x = start_x + col * (btn_width + gap);
        int y = start_y + row * (btn_height + gap);
        
        // Use lv_button instead of lv_obj for proper click handling
        buttons_[i] = lv_obj_create(menu_root_);
        if (!buttons_[i]) {
            ESP_LOGE(TAG, "Failed to create button %d", i);
            continue;
        }
        lv_obj_set_size(buttons_[i], btn_width, btn_height);
        lv_obj_set_pos(buttons_[i], x, y);
        lv_obj_set_style_radius(buttons_[i], btn_radius, 0);
        lv_obj_set_style_border_width(buttons_[i], 0, 0);
        lv_obj_set_style_pad_all(buttons_[i], 5, 0);
        
        // Set background color from theme or default
        uint32_t color = menu_theme.button_colors[i];
        if (color == 0) {
            color = default_items_[i].bg_color;
        }
        lv_obj_set_style_bg_color(buttons_[i], lv_color_hex(color), 0);
        lv_obj_set_style_bg_opa(buttons_[i], LV_OPA_COVER, 0);
        
        // Click effect - darker when pressed
        lv_obj_set_style_bg_color(buttons_[i], lv_color_darken(lv_color_hex(color), LV_OPA_30), LV_STATE_PRESSED);
        
        // Make clickable - critical flags for touch
        lv_obj_add_flag(buttons_[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(buttons_[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_user_data(buttons_[i], (void*)(intptr_t)i);
        
        // Add event callback for CLICKED and PRESSED (for debug)
        lv_obj_add_event_cb(buttons_[i], OnButtonClick, LV_EVENT_CLICKED, this);
        lv_obj_add_event_cb(buttons_[i], OnButtonPressed, LV_EVENT_PRESSED, this);
        
        // Layout for icon + label
        lv_obj_set_flex_flow(buttons_[i], LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(buttons_[i], LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        // Icon label - use montserrat for LVGL symbols (they work in symbol fonts)
        icons_[i] = lv_label_create(buttons_[i]);
        if (icons_[i]) {
            lv_label_set_text(icons_[i], default_items_[i].icon);
            lv_obj_set_style_text_font(icons_[i], &lv_font_montserrat_28, 0);  // Symbols work in montserrat
            lv_obj_set_style_text_color(icons_[i], lv_color_hex(menu_theme.text_color), 0);
            // Disable click on child - let parent handle it
            lv_obj_add_flag(icons_[i], LV_OBJ_FLAG_EVENT_BUBBLE);
            lv_obj_remove_flag(icons_[i], LV_OBJ_FLAG_CLICKABLE);
        }
        
        // Text label - use Vietnamese font
        labels_[i] = lv_label_create(buttons_[i]);
        if (labels_[i]) {
            lv_label_set_text(labels_[i], default_items_[i].name);
            lv_obj_set_style_text_font(labels_[i], &BUILTIN_TEXT_FONT, 0);  // Vietnamese font
            lv_obj_set_style_text_color(labels_[i], lv_color_hex(menu_theme.text_color), 0);
            lv_obj_set_style_text_align(labels_[i], LV_TEXT_ALIGN_CENTER, 0);
            // Disable click on child - let parent handle it
            lv_obj_add_flag(labels_[i], LV_OBJ_FLAG_EVENT_BUBBLE);
            lv_obj_remove_flag(labels_[i], LV_OBJ_FLAG_CLICKABLE);
        }
    }
    
    ESP_LOGI(TAG, "Menu UI created with %d buttons (2 cols x 3 rows)", MENU_ACTION_COUNT);
}

void MenuUI::Show() {
    if (menu_root_) {
        lv_obj_clear_flag(menu_root_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(menu_root_);  // Bring to front
        is_visible_ = true;
        ESP_LOGI(TAG, "Menu shown and moved to foreground");
    }
}

void MenuUI::Hide() {
    if (menu_root_) {
        lv_obj_add_flag(menu_root_, LV_OBJ_FLAG_HIDDEN);
        is_visible_ = false;
        ESP_LOGI(TAG, "Menu hidden");
    }
}

void MenuUI::SetCallback(MenuAction action, MenuItemCallback callback, void* user_data) {
    if (action >= 0 && action < MENU_ACTION_COUNT) {
        callbacks_[action] = callback;
        callback_data_[action] = user_data;
    }
}

void MenuUI::UpdateColors(uint32_t* colors, int count) {
    for (int i = 0; i < count && i < MENU_ACTION_COUNT; i++) {
        if (buttons_[i]) {
            lv_obj_set_style_bg_color(buttons_[i], lv_color_hex(colors[i]), 0);
            lv_obj_set_style_bg_color(buttons_[i], 
                lv_color_darken(lv_color_hex(colors[i]), LV_OPA_20), LV_STATE_PRESSED);
        }
    }
}

void MenuUI::OnButtonClick(lv_event_t* e) {
    MenuUI* self = static_cast<MenuUI*>(lv_event_get_user_data(e));
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    lv_obj_t* current_target = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    
    // Use current_target (the object with the event handler) to get the button
    lv_obj_t* btn = current_target;
    int action = (int)(intptr_t)lv_obj_get_user_data(btn);
    
    ESP_LOGI(TAG, "Button clicked: %d (%s)", action, default_items_[action].name);
    
    if (action >= 0 && action < MENU_ACTION_COUNT && self->callbacks_[action]) {
        ESP_LOGI(TAG, "Executing callback for action %d", action);
        self->callbacks_[action](self->callback_data_[action]);
    } else {
        ESP_LOGW(TAG, "No callback registered for action %d", action);
    }
}
