#ifndef MENU_UI_H
#define MENU_UI_H

#include <lvgl.h>
#include <functional>

/**
 * Settings Menu UI for Vietnam Xiaozhi devices
 * Grid layout with 6 colored buttons:
 * - Trợ lý (Assistant)
 * - Đồng hồ (Clock)
 * - Thẻ nhớ (SD Card)
 * - Radio
 * - Âm lịch (Lunar Calendar)
 * - Thông tin (Info)
 */

// Menu item callback type
typedef void (*MenuItemCallback)(void* user_data);

// Menu item structure
struct MenuItem {
    const char* name;           // Vietnamese name
    const char* icon;           // LVGL symbol or emoji
    uint32_t bg_color;          // Background color (RGB hex)
    MenuItemCallback callback;  // Click callback
};

// Menu actions enum
enum MenuAction {
    MENU_ACTION_ASSISTANT = 0,
    MENU_ACTION_CLOCK,
    MENU_ACTION_SD_CARD,
    MENU_ACTION_RADIO,
    MENU_ACTION_LUNAR,
    MENU_ACTION_INFO,
    MENU_ACTION_COUNT
};

class MenuUI {
public:
    MenuUI(lv_obj_t* parent, int width, int height);
    ~MenuUI();
    
    // Show/hide menu
    void Show();
    void Hide();
    bool IsVisible() const { return is_visible_; }
    
    // Set callback for menu actions
    void SetCallback(MenuAction action, MenuItemCallback callback, void* user_data = nullptr);
    
    // Update button colors from theme
    void UpdateColors(uint32_t* colors, int count);
    
    // Get root object
    lv_obj_t* GetRoot() { return menu_root_; }
    
private:
    void CreateUI();
    static void OnButtonClick(lv_event_t* e);
    
    lv_obj_t* parent_;
    lv_obj_t* menu_root_ = nullptr;
    lv_obj_t* buttons_[MENU_ACTION_COUNT] = {nullptr};
    lv_obj_t* labels_[MENU_ACTION_COUNT] = {nullptr};
    lv_obj_t* icons_[MENU_ACTION_COUNT] = {nullptr};
    
    int width_;
    int height_;
    bool is_visible_ = false;
    
    // Callbacks
    MenuItemCallback callbacks_[MENU_ACTION_COUNT] = {nullptr};
    void* callback_data_[MENU_ACTION_COUNT] = {nullptr};
    
    // Default menu items (Vietnamese)
    static const MenuItem default_items_[MENU_ACTION_COUNT];
};

#endif // MENU_UI_H
