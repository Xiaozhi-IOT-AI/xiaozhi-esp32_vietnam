#include "menu_ui.h"
#include "../theme/theme_config.h"
#include <esp_log.h>

#define TAG "MenuUI"

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
    lv_obj_set_pos(menu_root_, 0, 0);
    lv_obj_set_style_bg_color(menu_root_, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(menu_root_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(menu_root_, 0, 0);
    lv_obj_set_style_radius(menu_root_, 0, 0);
    lv_obj_set_style_pad_all(menu_root_, 10, 0);
    lv_obj_add_flag(menu_root_, LV_OBJ_FLAG_HIDDEN);
    
    // Get theme colors
    auto& theme_mgr = ThemeManager::GetInstance();
    auto& menu_theme = theme_mgr.GetMenuTheme();
    
    // Calculate button sizes for 3x2 grid
    // Portrait mode: 240x320, with padding
    int padding = 10;
    int gap = 8;
    int available_width = width_ - (2 * padding) - (2 * gap);
    int available_height = height_ - (2 * padding) - gap;
    int btn_width = available_width / 3;
    int btn_height = available_height / 2;
    int btn_radius = 15;
    
    // Create 6 buttons in 3x2 grid
    for (int i = 0; i < MENU_ACTION_COUNT; i++) {
        int row = i / 3;
        int col = i % 3;
        int x = padding + col * (btn_width + gap);
        int y = padding + row * (btn_height + gap);
        
        // Button container
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
        
        // Click effect
        lv_obj_set_style_bg_color(buttons_[i], lv_color_darken(lv_color_hex(color), LV_OPA_20), LV_STATE_PRESSED);
        
        // Make clickable
        lv_obj_add_flag(buttons_[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(buttons_[i], (void*)(intptr_t)i);
        lv_obj_add_event_cb(buttons_[i], OnButtonClick, LV_EVENT_CLICKED, this);
        
        // Create flex container for vertical layout
        lv_obj_set_flex_flow(buttons_[i], LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(buttons_[i], LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        // Icon label
        icons_[i] = lv_label_create(buttons_[i]);
        if (icons_[i]) {
            lv_label_set_text(icons_[i], default_items_[i].icon);
            lv_obj_set_style_text_font(icons_[i], &lv_font_montserrat_28, 0);
            lv_obj_set_style_text_color(icons_[i], lv_color_hex(menu_theme.text_color), 0);
        }
        
        // Text label
        labels_[i] = lv_label_create(buttons_[i]);
        if (labels_[i]) {
            lv_label_set_text(labels_[i], default_items_[i].name);
            lv_obj_set_style_text_font(labels_[i], &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(labels_[i], lv_color_hex(menu_theme.text_color), 0);
            lv_obj_set_style_text_align(labels_[i], LV_TEXT_ALIGN_CENTER, 0);
        }
    }
    
    ESP_LOGI(TAG, "Menu UI created with %d buttons", MENU_ACTION_COUNT);
}

void MenuUI::Show() {
    if (menu_root_) {
        lv_obj_clear_flag(menu_root_, LV_OBJ_FLAG_HIDDEN);
        is_visible_ = true;
        ESP_LOGI(TAG, "Menu shown");
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
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int action = (int)(intptr_t)lv_obj_get_user_data(btn);
    
    ESP_LOGI(TAG, "Button clicked: %d (%s)", action, default_items_[action].name);
    
    if (action >= 0 && action < MENU_ACTION_COUNT && self->callbacks_[action]) {
        self->callbacks_[action](self->callback_data_[action]);
    }
}
